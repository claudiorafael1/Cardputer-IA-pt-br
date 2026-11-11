#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>
#include <stdio.h>

// Fixed-size buffers avoid repeated heap allocation from Arduino String.
static constexpr size_t INPUT_CAP = 128;
static constexpr size_t LINE_CAP = 256;
static constexpr size_t KEY_CAP = 80;
static constexpr size_t REPLY_CAP = 192;
static constexpr size_t PATH_CAP = 64;

M5Canvas canvas(&M5Cardputer.Display);
static char inputBuffer[INPUT_CAP] = "> ";
static char brainFile[PATH_CAP] = "/regras.txt";
static char lastTopic[KEY_CAP] = "";
static char previousUserText[INPUT_CAP] = "";
static bool hasSD = false;

#define SD_SPI_SCK 40
#define SD_SPI_MISO 39
#define SD_SPI_MOSI 14
#define SD_SPI_CS 12
SPIClass SPI2(HSPI);

#define COLOR_BG      BLACK
#define COLOR_HEADER  M5Cardputer.Display.color565(20, 25, 30)
#define COLOR_AI      M5Cardputer.Display.color565(0, 255, 180)
#define COLOR_USER    WHITE
#define COLOR_SYS     M5Cardputer.Display.color565(255, 120, 0)
#define COLOR_ERROR   RED

struct InternalRule { const char *key; const char *reply; };
static const InternalRule internalRules[] = {
  {"eu sou", "O que faz voce dizer que e *?"},
  {"me sinto", "O que esta fazendo voce se sentir *?"},
  {"estou", "Como e para voce estar *?"},
  {"triste", "Sinto muito. Quer me contar o que aconteceu?"},
  {"ansioso", "O que esta preocupando voce neste momento?"},
  {"feliz", "Que bom! O que aconteceu de bom?"},
  {"medo", "Do que voce tem medo?"},
  {"familia", "Como e sua relacao com sua familia?"},
  {"amigo", "O que essa amizade significa para voce?"},
  {"trabalho", "Como voce se sente em relacao ao trabalho?"},
  {"escola", "O que esta acontecendo na escola?"},
  {"sonho", "O que esse sonho significa para voce?"},
  {"amor", "O que o amor representa para voce?"},
  {"porque", "O que voce acha que pode ser a razao?"},
  {"nao sei", "Tudo bem nao saber. O que voce consegue perceber sobre isso?"}
};
static constexpr size_t INTERNAL_RULE_COUNT = sizeof(internalRules) / sizeof(internalRules[0]);

static void copyText(char *dst, size_t cap, const char *src) {
  if (!cap) return;
  if (!src) src = "";
  snprintf(dst, cap, "%s", src);
}

static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Decode common Portuguese UTF-8 accents while copying to a bounded ASCII buffer.
// This avoids String.replace() chains and keeps phrase matching deterministic.
static char accentAscii(unsigned char c) {
  switch (c) {
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0xA0: case 0xA1:
    case 0xA2: case 0xA3: case 0xA4: case 0xA5: return 'a';
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0xA8: case 0xA9: case 0xAA:
    case 0xAB: return 'e';
    case 0x8C: case 0x8D: case 0x8E: case 0x8F: case 0xAC: case 0xAD: case 0xAE:
    case 0xAF: return 'i';
    case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x98: case 0x99:
    case 0x9A: case 0x9B: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: return 'o';
    case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0xB9: case 0xBA: case 0xBB:
    case 0xBC: case 0xBD: case 0xBE: case 0xBF: return 'u';
    case 0x87: case 0xA7: return 'c';
    default: return ' ';
  }
}

static void normalize(const char *src, char *dst, size_t cap) {
  if (!cap) return;
  size_t out = 0;
  bool pendingSpace = false;
  for (size_t i = 0; src && src[i] && out + 1 < cap; ++i) {
    unsigned char c = (unsigned char)src[i];
    char normalized;
    if (c == 0xC3 && src[i + 1]) {
      normalized = accentAscii((unsigned char)src[++i]);
    } else if (c < 0x80) {
      if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/') normalized = (char)c;
      else normalized = ' ';
    } else {
      normalized = ' ';
    }

    if (isSpace(normalized)) {
      pendingSpace = out > 0;
    } else {
      if (pendingSpace && out + 1 < cap) dst[out++] = ' ';
      pendingSpace = false;
      dst[out++] = normalized;
    }
  }
  dst[out] = '\0';
}

static bool containsPhrase(const char *text, const char *phrase) {
  if (!text || !phrase || !*phrase) return false;
  const size_t n = strlen(phrase);
  const char *p = text;
  while ((p = strstr(p, phrase)) != nullptr) {
    bool left = (p == text || p[-1] == ' ');
    bool right = (p[n] == '\0' || p[n] == ' ');
    if (left && right) return true;
    ++p;
  }
  return false;
}

static bool readLine(File &file, char *dst, size_t cap) {
  if (!file || !file.available() || cap < 2) return false;
  size_t n = file.readBytesUntil('\n', dst, cap - 1);
  dst[n] = '\0';
  // Discard the tail of an overlong line rather than treating it as another rule.
  if (n == cap - 1 && file.available()) {
    int c;
    while ((c = file.read()) >= 0 && c != '\n') yield();
  }
  while (n && (dst[n - 1] == '\r' || dst[n - 1] == ' ' || dst[n - 1] == '\t')) dst[--n] = '\0';
  size_t start = 0;
  while (dst[start] == ' ' || dst[start] == '\t') ++start;
  if (start) memmove(dst, dst + start, strlen(dst + start) + 1);
  return true;
}

static void drawHeader() {
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), 20, COLOR_HEADER);
  M5Cardputer.Display.drawLine(0, 20, M5Cardputer.Display.width(), 20, COLOR_AI);
  M5Cardputer.Display.setTextColor(COLOR_AI, COLOR_HEADER);
  M5Cardputer.Display.drawString("ELIZA NEURAL OS", 5, 2);
  if (hasSD) {
    M5Cardputer.Display.setTextColor(GREEN, COLOR_HEADER);
    M5Cardputer.Display.drawString("[SD OK]", M5Cardputer.Display.width() - 65, 2);
  } else {
    M5Cardputer.Display.setTextColor(COLOR_ERROR, COLOR_HEADER);
    M5Cardputer.Display.drawString("[NO SD]", M5Cardputer.Display.width() - 65, 2);
  }
}

static void drawInput() {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 24,
                               M5Cardputer.Display.width(), 24, COLOR_BG);
  M5Cardputer.Display.setTextColor(WHITE, COLOR_BG);
  M5Cardputer.Display.drawString(String(inputBuffer) + "_", 4,
                                 M5Cardputer.Display.height() - 20);
}

static void remember(const char *userText) { copyText(previousUserText, sizeof(previousUserText), userText); }
static void clearRecentContext() { previousUserText[0] = '\0'; }

static void logConversation(const char *userText, const char *reply) {
  remember(userText);
  if (!hasSD) return;
  File f = SD.open("/pensamentos/log.txt", FILE_APPEND);
  if (!f) return;
  f.print(F("V: ")); f.println(userText);
  f.print(F("E: ")); f.println(reply);
  f.close();
}

static void searchMemory(const char *term, char *reply, size_t replyCap) {
  if (!*term) { copyText(reply, replyCap, "ELIZA: Diga um termo para procurar."); return; }
  if (!hasSD) { copyText(reply, replyCap, "SISTEMA: Cartao SD indisponivel."); return; }
  File f = SD.open("/pensamentos/log.txt", FILE_READ);
  if (!f) { copyText(reply, replyCap, "ELIZA: Ainda nao tenho lembrancas salvas."); return; }

  char line[LINE_CAP], normalized[LINE_CAP], normalizedTerm[INPUT_CAP];
  char latest[INPUT_CAP] = "";
  normalize(term, normalizedTerm, sizeof(normalizedTerm));
  while (readLine(f, line, sizeof(line))) {
    yield();
    if (strncmp(line, "V: ", 3) != 0) continue;
    normalize(line + 3, normalized, sizeof(normalized));
    if (containsPhrase(normalized, normalizedTerm)) copyText(latest, sizeof(latest), line + 3);
  }
  f.close();
  if (*latest) snprintf(reply, replyCap, "ELIZA: Encontrei uma lembranca: \"%.100s\"", latest);
  else snprintf(reply, replyCap, "ELIZA: Nao encontrei lembrancas sobre '%.80s'.", term);
}

static void reflectText(const char *src, char *dst, size_t cap) {
  char normalized[INPUT_CAP];
  normalize(src, normalized, sizeof(normalized));
  size_t out = 0;
  const char *p = normalized;
  while (*p && out + 1 < cap) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char *start = p;
    while (*p && *p != ' ') ++p;
    size_t len = (size_t)(p - start);
    const char *replacement = nullptr;
    if (len == 2 && strncmp(start, "eu", len) == 0) replacement = "voce";
    else if (len == 2 && strncmp(start, "me", len) == 0) replacement = "te";
    else if ((len == 3 && strncmp(start, "sou", len) == 0)) replacement = "e";
    else if ((len == 5 && strncmp(start, "estou", len) == 0)) replacement = "esta";
    else if ((len == 4 && strncmp(start, "meu", len) == 0)) replacement = "seu";
    else if ((len == 5 && strncmp(start, "minha", len) == 0)) replacement = "sua";
    else if ((len == 4 && strncmp(start, "meus", len) == 0)) replacement = "seus";
    else if ((len == 6 && strncmp(start, "minhas", len) == 0)) replacement = "suas";
    const char *word = replacement ? replacement : start;
    size_t wordLen = replacement ? strlen(replacement) : len;
    if (out && out + 1 < cap) dst[out++] = ' ';
    if (wordLen > cap - out - 1) wordLen = cap - out - 1;
    memcpy(dst + out, word, wordLen);
    out += wordLen;
  }
  dst[out] = '\0';
}

static void pickReply(const char *responses, char *dst, size_t cap) {
  unsigned count = 0;
  const char *p = responses;
  while (true) {
    ++count;
    p = strchr(p, '|');
    if (!p) break;
    ++p;
  }
  unsigned selected = (unsigned)random(count);
  p = responses;
  while (selected-- && (p = strchr(p, '|')) != nullptr) ++p;
  const char *end = strchr(p, '|');
  size_t n = end ? (size_t)(end - p) : strlen(p);
  while (n && (p[n - 1] == ' ' || p[n - 1] == '\r')) --n;
  if (n >= cap) n = cap - 1;
  memcpy(dst, p, n);
  dst[n] = '\0';
}

static bool setIntent(const char *text, char *reply, size_t cap) {
  if (!strcmp(text, "oi") || !strcmp(text, "ola") || !strcmp(text, "bom dia") ||
      !strcmp(text, "boa tarde") || !strcmp(text, "boa noite") || !strcmp(text, "e ai"))
    copyText(reply, cap, "ELIZA: Oi! O que voce gostaria de conversar hoje?");
  else if (containsPhrase(text, "quem e voce") || containsPhrase(text, "o que voce e"))
    copyText(reply, cap, "ELIZA: Sou a Eliza, assistente offline no Cardputer. Uso regras locais e memoria no cartao SD.");
  else if (containsPhrase(text, "o que voce pode fazer") || !strcmp(text, "ajuda"))
    copyText(reply, cap, "ELIZA: Comandos: /ajuda, /tema NOME, /tema geral, /memoria TERMO e /limpar.");
  else if (containsPhrase(text, "como voce esta") || containsPhrase(text, "tudo bem"))
    copyText(reply, cap, "ELIZA: Estou funcionando e pronta para conversar. E voce, como esta?");
  else if (containsPhrase(text, "obrigado") || containsPhrase(text, "obrigada") || containsPhrase(text, "valeu"))
    copyText(reply, cap, "ELIZA: De nada! O que mais posso fazer por voce?");
  else if (containsPhrase(text, "tchau") || containsPhrase(text, "ate mais") || containsPhrase(text, "adeus"))
    copyText(reply, cap, "ELIZA: Ate mais. Quando quiser continuar, estarei aqui.");
  else if (containsPhrase(text, "triste") || containsPhrase(text, "deprimido") || containsPhrase(text, "deprimida"))
    copyText(reply, cap, "ELIZA: Sinto muito. Quer me contar o que aconteceu?");
  else if (containsPhrase(text, "ansioso") || containsPhrase(text, "ansiosa") || containsPhrase(text, "preocupado") || containsPhrase(text, "preocupada"))
    copyText(reply, cap, "ELIZA: O que esta preocupando voce mais neste momento?");
  else if (containsPhrase(text, "feliz") || containsPhrase(text, "contente") || containsPhrase(text, "alegre"))
    copyText(reply, cap, "ELIZA: Que bom! O que aconteceu de bom?");
  else if (containsPhrase(text, "nao sei") || containsPhrase(text, "nao tenho certeza"))
    copyText(reply, cap, "ELIZA: Tudo bem ainda nao ter certeza. O que voce consegue perceber?");
  else if (containsPhrase(text, "codigo") || containsPhrase(text, "programacao") || containsPhrase(text, "arduino") || containsPhrase(text, "esp32"))
    copyText(reply, cap, "ELIZA: Qual problema de logica ou projeto para Arduino/ESP32 voce esta tentando resolver?");
  else return false;
  return true;
}

static void getResponse(const char *input, char *reply, size_t replyCap) {
  char text[INPUT_CAP];
  normalize(input, text, sizeof(text));
  if (!*text) { copyText(reply, replyCap, "ELIZA: Escreva algo para iniciarmos."); return; }

  if (strncmp(text, "/tema ", 6) == 0) {
    const char *theme = text + 6;
    if (!strcmp(theme, "geral") || !strcmp(theme, "padrao")) {
      copyText(brainFile, sizeof(brainFile), "/regras.txt");
      lastTopic[0] = '\0';
      copyText(reply, replyCap, "SISTEMA: Retornando ao cerebro geral.");
      return;
    }
    char path[PATH_CAP];
    size_t j = 0;
    for (size_t i = 0; theme[i] && j + 12 < sizeof(path); ++i) {
      char c = theme[i];
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') path[j++] = c;
    }
    path[j] = '\0';
    char fullPath[PATH_CAP];
    snprintf(fullPath, sizeof(fullPath), "/temas/%s.txt", path);
    if (hasSD && *path && SD.exists(fullPath)) {
      copyText(brainFile, sizeof(brainFile), fullPath);
      copyText(lastTopic, sizeof(lastTopic), path);
      snprintf(reply, replyCap, "SISTEMA: Tema carregado: %s.", path);
    } else snprintf(reply, replyCap, "SISTEMA: Tema nao encontrado: %s.", path);
    return;
  }
  if (!strcmp(text, "/ajuda") || !strcmp(text, "/help")) {
    copyText(reply, replyCap, "ELIZA: /ajuda, /tema NOME, /tema geral, /memoria TERMO, /limpar."); return;
  }
  if (strncmp(text, "/memoria ", 9) == 0) { searchMemory(text + 9, reply, replyCap); return; }
  if (strncmp(text, "lembra de ", 10) == 0) { searchMemory(text + 10, reply, replyCap); return; }
  if (!strcmp(text, "/limpar")) {
    clearRecentContext(); copyText(reply, replyCap, "ELIZA: Contexto recente apagado; arquivo de memoria mantido."); return;
  }
  if (setIntent(text, reply, replyCap)) return;

  char line[LINE_CAP], key[KEY_CAP], bestKey[KEY_CAP] = "";
  char bestReply[REPLY_CAP] = "";
  int bestScore = -1;
  int bestPosition = -1;
  if (hasSD) {
    File f = SD.open(brainFile, FILE_READ);
    while (f && readLine(f, line, sizeof(line))) {
      yield();
      if (line[0] == '#' || !line[0]) continue;
      char *separator = strchr(line, '|');
      if (!separator) continue;
      *separator = '\0';
      normalize(line, key, sizeof(key));
      if (!*key || !containsPhrase(text, key)) continue;
      int score = (int)strlen(key);
      if (score > bestScore) {
        bestScore = score;
        bestPosition = (int)(strstr(text, key) - text);
        copyText(bestKey, sizeof(bestKey), key);
        pickReply(separator + 1, bestReply, sizeof(bestReply));
      }
    }
    if (f) f.close();
  }

  for (size_t i = 0; i < INTERNAL_RULE_COUNT; ++i) {
    normalize(internalRules[i].key, key, sizeof(key));
    if (!containsPhrase(text, key)) continue;
    int score = (int)strlen(key);
    if (score > bestScore) {
      bestScore = score;
      bestPosition = (int)(strstr(text, key) - text);
      copyText(bestKey, sizeof(bestKey), key);
      copyText(bestReply, sizeof(bestReply), internalRules[i].reply);
    }
  }

  if (bestScore >= 0) {
    copyText(lastTopic, sizeof(lastTopic), bestKey);
    char *star = strchr(bestReply, '*');
    if (star && bestPosition >= 0) {
      char reflected[INPUT_CAP];
      const char *remainder = text + bestPosition + bestScore;
      while (*remainder == ' ') ++remainder;
      if (!*remainder) remainder = text;
      reflectText(remainder, reflected, sizeof(reflected));
      char result[REPLY_CAP];
      size_t prefixLen = (size_t)(star - bestReply);
      if (prefixLen >= sizeof(result)) prefixLen = sizeof(result) - 1;
      memcpy(result, bestReply, prefixLen);
      result[prefixLen] = '\0';
      snprintf(reply, replyCap, "ELIZA: %.*s%s%s", (int)prefixLen, bestReply,
               reflected, star + 1);
    } else snprintf(reply, replyCap, "ELIZA: %s", bestReply);
    return;
  }

  if ((!strcmp(text, "mais") || !strcmp(text, "continuar") || containsPhrase(text, "e voce")) && *previousUserText) {
    snprintf(reply, replyCap, "ELIZA: Sobre '%.70s', o que e mais importante para voce nisso?", previousUserText);
  } else if (*lastTopic) {
    snprintf(reply, replyCap, "ELIZA: O que mais voce gostaria de explorar sobre %.60s?", lastTopic);
  } else copyText(reply, replyCap, "ELIZA: Pode me contar um pouco mais? O que isso significa para voce?");
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setBrightness(128);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);

  // 8-bit RGB332 sprite uses about half the RAM of the previous 16-bit sprite.
  canvas.setColorDepth(8);
  canvas.createSprite(M5Cardputer.Display.width() - 4, M5Cardputer.Display.height() - 48);
  canvas.setTextScroll(true);
  canvas.setTextFont(&fonts::Font2);
  canvas.setTextSize(1);

  SPI2.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  hasSD = SD.begin(SD_SPI_CS, SPI2);
  if (hasSD) {
    SD.mkdir("/pensamentos");
    SD.mkdir("/temas");
    canvas.setTextColor(COLOR_AI, COLOR_BG);
    canvas.println(F("Sistema inicializado."));
    canvas.println(F("Regras em streaming; buffers fixos."));
  } else {
    canvas.setTextColor(COLOR_SYS, COLOR_BG);
    canvas.println(F("AVISO: falha no cartao SD."));
    canvas.println(F("Regras internas ativas."));
  }
  canvas.pushSprite(2, 22);
  drawHeader();
  drawInput();
}

void loop() {
  M5Cardputer.update();
  if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) return;
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  for (auto key : status.word) {
    size_t n = strlen(inputBuffer);
    if (n + 1 < sizeof(inputBuffer)) { inputBuffer[n] = key; inputBuffer[n + 1] = '\0'; }
  }
  size_t inputLen = strlen(inputBuffer);
  if (status.del && inputLen > 2) inputBuffer[inputLen - 1] = '\0';

  if (status.enter) {
    char userText[INPUT_CAP];
    char reply[REPLY_CAP];
    copyText(userText, sizeof(userText), inputBuffer + 2);
    if (*userText) {
      canvas.setTextColor(COLOR_USER, COLOR_BG);
      canvas.print(F("\nVoce: "));
      canvas.println(userText);
      getResponse(userText, reply, sizeof(reply));
      char normalized[INPUT_CAP];
      normalize(userText, normalized, sizeof(normalized));
      if (normalized[0] != '/') logConversation(userText, reply);
      if (strncmp(reply, "SISTEMA:", 8) == 0) canvas.setTextColor(COLOR_SYS, COLOR_BG);
      else canvas.setTextColor(COLOR_AI, COLOR_BG);
      canvas.println(reply);
      canvas.pushSprite(2, 22);
    }
    copyText(inputBuffer, sizeof(inputBuffer), "> ");
  }
  drawInput();
}
