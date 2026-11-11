#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

M5Canvas canvas(&M5Cardputer.Display);
String inputBuffer = "> ";

// Pinos do Cartao SD no M5Cardputer
#define SD_SPI_SCK 40
#define SD_SPI_MISO 39
#define SD_SPI_MOSI 14
#define SD_SPI_CS 12
SPIClass SPI2(HSPI);

bool hasSD = false;
String lastTopic = "nada";
String brainFile = "/regras.txt";
String previousUserText = "";
String recentUserText[3];

// ---------------------------------------------------------
// PALETA DE CORES
// ---------------------------------------------------------
#define COLOR_BG      BLACK
#define COLOR_HEADER  M5Cardputer.Display.color565(20, 25, 30)
#define COLOR_AI      M5Cardputer.Display.color565(0, 255, 180)
#define COLOR_USER    WHITE
#define COLOR_SYS     M5Cardputer.Display.color565(255, 120, 0)
#define COLOR_ERROR   RED

struct MinimalRule { const char* keyword; const char* response; };
const MinimalRule internalRules[] = {
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
  {"nao sei", "Tudo bem nao saber ainda. O que voce consegue perceber sobre isso?"}
};
const size_t INTERNAL_RULE_COUNT = sizeof(internalRules) / sizeof(internalRules[0]);

// ---------------------------------------------------------
// INTERFACE
// ---------------------------------------------------------
void drawHeader() {
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

void drawInput() {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 24,
                               M5Cardputer.Display.width(), 24, COLOR_BG);
  M5Cardputer.Display.setTextColor(WHITE, COLOR_BG);
  M5Cardputer.Display.drawString(inputBuffer + "_", 4,
                                 M5Cardputer.Display.height() - 20);
}

// ---------------------------------------------------------
// NORMALIZACAO: acentos, caixa e pontuacao
// A normalizacao permite comparar palavras sem depender do teclado usado.
// ---------------------------------------------------------
String normalizeText(const String &text) {
  String s = text;
  s.toLowerCase();

  // UTF-8: converte acentos comuns em portugues para ASCII.
  s.replace("á", "a"); s.replace("à", "a"); s.replace("â", "a"); s.replace("ã", "a"); s.replace("ä", "a");
  s.replace("é", "e"); s.replace("è", "e"); s.replace("ê", "e"); s.replace("ë", "e");
  s.replace("í", "i"); s.replace("ì", "i"); s.replace("î", "i"); s.replace("ï", "i");
  s.replace("ó", "o"); s.replace("ò", "o"); s.replace("ô", "o"); s.replace("õ", "o"); s.replace("ö", "o");
  s.replace("ú", "u"); s.replace("ù", "u"); s.replace("û", "u"); s.replace("ü", "u");
  s.replace("ç", "c");

  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ')) {
      s.setCharAt(i, ' ');
    }
  }
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  return s;
}

// Busca frase inteira, evitando falsos positivos (ex.: "oi" dentro de "coisa").
bool containsPhrase(const String &text, const String &phrase) {
  if (phrase.length() == 0) return false;
  String paddedText = " " + text + " ";
  String paddedPhrase = " " + phrase + " ";
  return paddedText.indexOf(paddedPhrase) >= 0;
}

// ---------------------------------------------------------
// INVERSAO DE PERSPECTIVA PARA RESPOSTAS COM ASTERISCO
// ---------------------------------------------------------
String reflect(const String &text) {
  String s = " " + normalizeText(text) + " ";
  s.replace(" eu sou ", " voce e ");
  s.replace(" eu estou ", " voce esta ");
  s.replace(" eu tenho ", " voce tem ");
  s.replace(" eu quero ", " voce quer ");
  s.replace(" eu posso ", " voce pode ");
  s.replace(" eu gosto ", " voce gosta ");
  s.replace(" eu ", " voce ");
  s.replace(" me ", " te ");
  s.replace(" meu ", " seu ");
  s.replace(" minha ", " sua ");
  s.replace(" meus ", " seus ");
  s.replace(" minhas ", " suas ");
  s.replace(" comigo ", " com voce ");
  s.replace(" sou ", " e ");
  s.replace(" estou ", " esta ");
  s.replace(" tenho ", " tem ");
  s.trim();
  return s;
}

void rememberContext(const String &userText) {
  previousUserText = userText;
  for (int i = 2; i > 0; --i) recentUserText[i] = recentUserText[i - 1];
  recentUserText[0] = userText;
}

void clearRecentContext() {
  previousUserText = "";
  for (int i = 0; i < 3; ++i) recentUserText[i] = "";
}

// ---------------------------------------------------------
// MEMORIA EM SD E BUSCA DE LEMBRANCAS
// ---------------------------------------------------------
void logPensamento(const String &userText, const String &aiText) {
  rememberContext(userText);
  if (!hasSD) return;

  File logFile = SD.open("/pensamentos/log.txt", FILE_APPEND);
  if (logFile) {
    logFile.print(F("V: ")); logFile.println(userText);
    logFile.print(F("E: ")); logFile.println(aiText);
    logFile.close();
  }
}

String buscarPensamentos(const String &term) {
  if (term.length() == 0) return String(F("ELIZA: Diga uma palavra ou assunto para eu procurar."));
  if (!hasSD) return String(F("SISTEMA: Sem cartao SD, a memoria persistente esta indisponivel."));

  File logFile = SD.open("/pensamentos/log.txt", FILE_READ);
  if (!logFile) return String(F("ELIZA: Ainda nao tenho lembrancas salvas."));

  String normalizedTerm = normalizeText(term);
  String latestMatch = "";
  while (logFile.available()) {
    yield();
    String line = logFile.readStringUntil('\n');
    String normalizedLine = normalizeText(line);
    if (containsPhrase(normalizedLine, normalizedTerm)) {
      line.trim();
      if (line.startsWith("V: ")) line.remove(0, 3);
      latestMatch = line;
    }
  }
  logFile.close();

  if (latestMatch.length() > 0) {
    return String(F("ELIZA: Encontrei uma lembranca: \"")) + latestMatch + "\"";
  }
  return String(F("ELIZA: Nao encontrei lembrancas sobre '")) + term + "'.";
}

// ---------------------------------------------------------
// RESPOSTAS POR INTENCAO: pequenas regras locais, sem modelo pesado
// ---------------------------------------------------------
String getIntentResponse(const String &text) {
  if (text == "oi" || text == "ola" || text == "bom dia" ||
      text == "boa tarde" || text == "boa noite" || text == "e ai") {
    return String(F("Oi! O que voce gostaria de conversar hoje?"));
  }
  if (containsPhrase(text, "quem e voce") || containsPhrase(text, "o que voce e")) {
    return String(F("Sou a Eliza, uma assistente offline no Cardputer. Reconheco temas, sigo regras locais e posso consultar lembrancas salvas no cartao SD."));
  }
  if (containsPhrase(text, "o que voce pode fazer") || containsPhrase(text, "ajuda")) {
    return String(F("Posso conversar por temas, usar regras do cartao SD e buscar lembrancas. Comandos: /ajuda, /tema NOME, /tema geral, /memoria TERMO e /limpar."));
  }
  if (containsPhrase(text, "como voce esta") || containsPhrase(text, "tudo bem")) {
    return String(F("Estou funcionando e pronta para conversar. E voce, como esta se sentindo?"));
  }
  if (containsPhrase(text, "obrigado") || containsPhrase(text, "obrigada") || containsPhrase(text, "valeu")) {
    return String(F("De nada! O que mais posso fazer por voce?"));
  }
  if (containsPhrase(text, "tchau") || containsPhrase(text, "ate mais") || containsPhrase(text, "adeus")) {
    return String(F("Ate mais. Quando quiser continuar, estarei aqui."));
  }
  if (containsPhrase(text, "triste") || containsPhrase(text, "deprimido") || containsPhrase(text, "deprimida")) {
    return String(F("Sinto muito que esteja passando por isso. Quer me contar o que aconteceu?"));
  }
  if (containsPhrase(text, "ansioso") || containsPhrase(text, "ansiosa") || containsPhrase(text, "preocupado") || containsPhrase(text, "preocupada")) {
    return String(F("Parece que isso esta pesando para voce. Qual e a principal preocupacao agora?"));
  }
  if (containsPhrase(text, "feliz") || containsPhrase(text, "contente") || containsPhrase(text, "alegre")) {
    return String(F("Que bom! O que aconteceu para voce se sentir assim?"));
  }
  if (containsPhrase(text, "meu nome e") || containsPhrase(text, "me chamo")) {
    return String(F("Prazer em conhecer voce. Como prefere que eu te chame?"));
  }
  if (containsPhrase(text, "nao sei") || containsPhrase(text, "nao tenho certeza")) {
    return String(F("Tudo bem ainda nao ter certeza. Que parte voce consegue esclarecer primeiro?"));
  }
  if (containsPhrase(text, "porque") || containsPhrase(text, "por que")) {
    return String(F("O que voce imagina que pode estar por tras disso?"));
  }
  if (containsPhrase(text, "codigo") || containsPhrase(text, "programacao") || containsPhrase(text, "arduino") || containsPhrase(text, "esp32")) {
    return String(F("Posso ajudar a pensar em logica e projetos para Arduino ou ESP32. Qual problema voce esta tentando resolver?"));
  }
  if (containsPhrase(text, "como funciona") || containsPhrase(text, "me explique")) {
    return String(F("Posso tentar explicar. Qual parte ou conceito voce quer entender melhor?"));
  }
  return "";
}

String chooseResponse(const String &responseList) {
  String choices[5];
  int count = 0;
  int start = 0;
  while (start <= (int)responseList.length() && count < 5) {
    int separator = responseList.indexOf('|', start);
    if (separator < 0) {
      String item = responseList.substring(start);
      item.trim();
      if (item.length() > 0) choices[count++] = item;
      break;
    }
    String item = responseList.substring(start, separator);
    item.trim();
    if (item.length() > 0) choices[count++] = item;
    start = separator + 1;
  }
  if (count == 0) return "";
  return choices[random(0, count)];
}

// ---------------------------------------------------------
// MOTOR DE CONVERSA
// Prioriza a regra mais especifica em vez da primeira linha que coincidir.
// Formato de regras.txt: palavra ou frase|resposta|resposta com * opcional
// ---------------------------------------------------------
String getElizaResponse(const String &input) {
  String text = normalizeText(input);
  if (text.length() == 0) return String(F("ELIZA: Escreva algo para iniciarmos a conversa."));

  if (text.startsWith("/tema ")) {
    String newTheme = text.substring(6);
    newTheme.trim();
    if (newTheme == "geral" || newTheme == "padrao") {
      brainFile = "/regras.txt";
      lastTopic = "nada";
      return String(F("SISTEMA: Retornando ao cerebro geral."));
    }
    String themePath = "/temas/" + newTheme + ".txt";
    if (hasSD && SD.exists(themePath)) {
      brainFile = themePath;
      lastTopic = newTheme;
      return String(F("SISTEMA: Modulo carregado! Tema: [")) + newTheme + "].";
    }
    return String(F("SISTEMA: O arquivo '")) + themePath + "' nao existe no cartao SD.";
  }

  if (text == "/ajuda" || text == "/help") {
    return String(F("ELIZA: Comandos: /ajuda, /tema NOME, /tema geral, /memoria TERMO e /limpar. Regras ficam em /regras.txt e /temas/."));
  }
  if (text.startsWith("/memoria ")) {
    return buscarPensamentos(text.substring(9));
  }
  if (text.startsWith("lembra de ")) {
    return buscarPensamentos(text.substring(10));
  }
  if (text == "/limpar") {
    clearRecentContext();
    return String(F("ELIZA: Contexto recente apagado. O arquivo de lembrancas no SD foi mantido."));
  }

  String intent = getIntentResponse(text);
  if (intent.length() > 0) return String(F("ELIZA: ")) + intent;

  // Carrega a melhor regra do arquivo: frase mais longa vence, evitando
  // que uma regra generica esconda uma correspondencia mais especifica.
  String bestResponses = "";
  String bestKeyword = "";
  int bestPosition = -1;
  int bestScore = -1;

  if (hasSD && SD.exists(brainFile)) {
    File file = SD.open(brainFile, FILE_READ);
    while (file && file.available()) {
      yield();
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() == 0 || line.startsWith("#")) continue;

      int firstPipe = line.indexOf('|');
      if (firstPipe <= 0) continue;
      String keyword = normalizeText(line.substring(0, firstPipe));
      if (keyword.length() == 0 || !containsPhrase(text, keyword)) continue;

      int score = keyword.length();
      if (score > bestScore) {
        bestScore = score;
        bestPosition = text.indexOf(keyword);
        bestKeyword = keyword;
        bestResponses = line.substring(firstPipe + 1);
      }
    }
    if (file) file.close();
  }

  // Regras internas continuam disponiveis mesmo se o SD existir mas nao
  // possuir uma regra para a frase recebida.
  for (size_t i = 0; i < INTERNAL_RULE_COUNT; ++i) {
    String keyword = normalizeText(internalRules[i].keyword);
    if (!containsPhrase(text, keyword)) continue;
    int score = keyword.length();
    if (score > bestScore) {
      bestScore = score;
      bestPosition = text.indexOf(keyword);
      bestKeyword = keyword;
      bestResponses = internalRules[i].response;
    }
  }

  if (bestScore >= 0) {
    lastTopic = bestKeyword;
    String answer = chooseResponse(bestResponses);
    if (answer.indexOf('*') >= 0 && bestPosition >= 0) {
      String remainder = text.substring(bestPosition + bestKeyword.length());
      remainder.trim();
      if (remainder.length() == 0) remainder = text;
      answer.replace("*", reflect(remainder));
    }
    return String(F("ELIZA: ")) + answer;
  }

  // Continuidade curta e contextual, sem copiar todo o historico para RAM.
  if (containsPhrase(text, "e voce") || text == "por exemplo" ||
      text == "mais" || text == "continuar" || text == "continua") {
    if (previousUserText.length() > 0) {
      return String(F("ELIZA: Sobre '")) + previousUserText + "', o que e mais importante para voce nisso?";
    }
  }

  if (lastTopic != "nada") {
    String followUps[] = {
      "Como isso se relaciona com " + lastTopic + "?",
      "O que mais voce gostaria de explorar sobre " + lastTopic + "?",
      "Pode me dar um exemplo relacionado a " + lastTopic + "?"
    };
    return String(F("ELIZA: ")) + followUps[random(0, 3)];
  }

  const char* const fallbacks[] = {
    "Pode me contar um pouco mais?",
    "O que isso significa para voce?",
    "Qual parte desse assunto e mais importante para voce?",
    "Entendi. O que aconteceu em seguida?",
    "Quer olhar para isso por outro angulo?"
  };
  return String(F("ELIZA: ")) + fallbacks[random(0, 5)];
}

// ---------------------------------------------------------
// INICIALIZACAO
// ---------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setBrightness(128);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);

  inputBuffer.reserve(128);
  canvas.createSprite(M5Cardputer.Display.width() - 4, M5Cardputer.Display.height() - 48);
  canvas.setTextScroll(true);
  canvas.setTextFont(&fonts::Font2);
  canvas.setTextSize(1);

  SPI2.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  if (SD.begin(SD_SPI_CS, SPI2)) {
    hasSD = true;
    SD.mkdir("/pensamentos");
    SD.mkdir("/temas");
    canvas.setTextColor(COLOR_AI, COLOR_BG);
    canvas.println(F("Sistema inicializado."));
    canvas.println(F("Regras locais e memoria online."));
  } else {
    hasSD = false;
    canvas.setTextColor(COLOR_SYS, COLOR_BG);
    canvas.println(F("AVISO: Falha no cartao SD."));
    canvas.println(F("Usando regras internas."));
  }

  canvas.pushSprite(2, 22);
  drawHeader();
  drawInput();
}

// ---------------------------------------------------------
// LOOP E TECLADO
// ---------------------------------------------------------
void loop() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    for (auto key : status.word) {
      if (inputBuffer.length() < 126) inputBuffer += key;
    }

    if (status.del && inputBuffer.length() > 2) {
      inputBuffer.remove(inputBuffer.length() - 1);
    }

    if (status.enter) {
      String userInput = inputBuffer.substring(2);
      userInput.trim();
      if (userInput.length() > 0) {
        canvas.setTextColor(COLOR_USER, COLOR_BG);
        canvas.print(F("\nVoce: "));
        canvas.println(userInput);

        String response = getElizaResponse(userInput);
        String normalizedInput = normalizeText(userInput);
        bool isCommand = normalizedInput.startsWith("/");
        if (!isCommand) logPensamento(userInput, response);
        else if (normalizedInput != "/limpar" && !normalizedInput.startsWith("/memoria ")) {
          // Comandos de tema nao entram na memoria conversacional.
        }

        if (response.startsWith("SISTEMA:")) canvas.setTextColor(COLOR_SYS, COLOR_BG);
        else canvas.setTextColor(COLOR_AI, COLOR_BG);
        canvas.println(response);
        canvas.pushSprite(2, 22);
      }
      inputBuffer = F("> ");
    }
    drawInput();
  }
}
