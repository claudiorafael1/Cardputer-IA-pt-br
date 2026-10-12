#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

M5Canvas canvas(&M5Cardputer.Display);
String inputBuffer = "> ";

// Pinos do Cartão SD no M5Cardputer
#define SD_SPI_SCK 40
#define SD_SPI_MISO 39
#define SD_SPI_MOSI 14
#define SD_SPI_CS 12
SPIClass SPI2(HSPI);

bool hasSD = false;
String lastTopic = "nada";
String brainFile = "/regras.txt";

// ---------------------------------------------------------
// PALETA DE CORES CYBERPUNK / IA
// ---------------------------------------------------------
#define COLOR_BG      BLACK
#define COLOR_HEADER  M5Cardputer.Display.color565(20, 25, 30)
#define COLOR_AI      M5Cardputer.Display.color565(0, 255, 180) // Ciano/Menta
#define COLOR_USER    WHITE
#define COLOR_SYS     M5Cardputer.Display.color565(255, 120, 0) // Laranja Néon
#define COLOR_ERROR   RED

// Regras de sobrevivência em caso de falha do SD
struct MinimalRule { const char* keyword; const char* response; };
const MinimalRule internalRules[] = {
  {"eu sou", "Por que voce diz que e *?"},
  {"triste", "O que te deixa assim?"},
  {"feliz", "Que bom! Me conte mais."}
};

// ---------------------------------------------------------
// DESENHA O CABEÇALHO (BARRA SUPERIOR)
// ---------------------------------------------------------
void drawHeader() {
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), 20, COLOR_HEADER);
  M5Cardputer.Display.drawLine(0, 20, M5Cardputer.Display.width(), 20, COLOR_AI);
  
  M5Cardputer.Display.setTextColor(COLOR_AI, COLOR_HEADER);
  M5Cardputer.Display.drawString("ELIZA NEURAL OS", 5, 2);
  
  // Indicador de status do SD Card no canto direito
  if (hasSD) {
    M5Cardputer.Display.setTextColor(GREEN, COLOR_HEADER);
    M5Cardputer.Display.drawString("[SD OK]", M5Cardputer.Display.width() - 65, 2);
  } else {
    M5Cardputer.Display.setTextColor(COLOR_ERROR, COLOR_HEADER);
    M5Cardputer.Display.drawString("[NO SD]", M5Cardputer.Display.width() - 65, 2);
  }
}

// ---------------------------------------------------------
// DESENHA A LINHA DE ENTRADA (TECLADO)
// ---------------------------------------------------------
void drawInput() {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 24, M5Cardputer.Display.width(), 24, COLOR_BG);
  M5Cardputer.Display.setTextColor(WHITE, COLOR_BG);
  // Adiciona um cursor visual "_" no final da digitação
  M5Cardputer.Display.drawString(inputBuffer + "_", 4, M5Cardputer.Display.height() - 20);
}

// ---------------------------------------------------------
// INVERSÃO DE PRONOMES COM BAIXO CONSUMO DE RAM
// ---------------------------------------------------------
String reflect(const String &text) {
  String padded;
  padded.reserve(text.length() + 20);
  padded = " " + text + " ";
  padded.replace(" eu ", " voce ");
  padded.replace(" me ", " te ");
  padded.replace(" meu ", " seu ");
  padded.replace(" minha ", " sua ");
  padded.replace(" sou ", " e ");
  padded.replace(" estou ", " esta ");
  padded.trim();
  return padded;
}

// ---------------------------------------------------------
// GRAVAÇÃO DE CONTEXTO CONTÍNUO
// ---------------------------------------------------------
void logPensamento(const String &userText, const String &aiText) {
  if (!hasSD) return;
  File logFile = SD.open("/pensamentos/log.txt", FILE_APPEND);
  if (logFile) {
    logFile.print(F("V: ")); logFile.println(userText);
    logFile.print(F("E: ")); logFile.println(aiText);
    logFile.close();
  }
}

// ---------------------------------------------------------
// MOTOR BIG DATA: Busca de Alta Performance
// ---------------------------------------------------------
String buscarPensamentos(const String &keyword) {
  if (!hasSD) return String(F("SISTEMA: Sem cartao SD, acesso bloqueado."));
  File logFile = SD.open("/pensamentos/log.txt", FILE_READ);
  if (!logFile) return String(F("ELIZA: Meus pensamentos estao vazios."));

  uint32_t fileSize = logFile.size();
  if (fileSize == 0) { logFile.close(); return String(F("ELIZA: Nada escrito nos pensamentos.")); }

  const size_t BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE + 1];
  int32_t position = fileSize;
  int overlap = keyword.length();
  String foundLine = "";
  const char* searchWord = keyword.c_str();

  while (position > 0) {
    yield(); // Previne que o chip trave (Kernel Panic)
    int32_t bytesToRead = BUFFER_SIZE;
    if (position < (int32_t)BUFFER_SIZE) bytesToRead = position;
    position -= bytesToRead;

    logFile.seek(position);
    logFile.readBytes(buffer, bytesToRead);
    buffer[bytesToRead] = '\0';

    char* match = strcasestr(buffer, searchWord);
    if (match != nullptr) {
      char* lineStart = match;
      while (lineStart > buffer && *(lineStart - 1) != '\n') lineStart--;
      char* lineEnd = match;
      while (*lineEnd != '\0' && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;
      *lineEnd = '\0';
      foundLine = String(lineStart);
      break;
    }
    if (position > 0) position += overlap;
  }
  logFile.close();

  if (foundLine != "") {
    foundLine.replace("V: ", ""); foundLine.replace("E: ", "");
    return String(F("ELIZA: Lembro disso: '")) + foundLine + "'";
  }
  return String(F("ELIZA: Nao achei nada em minhas memorias sobre '")) + keyword + "'";
}

// ---------------------------------------------------------
// CÉREBRO PRINCIPAL DE RESPOSTAS E COMANDOS
// ---------------------------------------------------------
String getElizaResponse(const String &input) {
  String cleanInput = input;
  cleanInput.reserve(input.length() + 5);
  cleanInput.toLowerCase();

  if (cleanInput.startsWith("/tema ")) {
    String newTheme = cleanInput.substring(6);
    newTheme.trim();

    if (newTheme == "geral" || newTheme == "padrao") {
      brainFile = "/regras.txt";
      return String(F("SISTEMA: Retornando ao cerebro geral."));
    }

    String themePath = "/temas/" + newTheme + ".txt";
    if (hasSD && SD.exists(themePath)) {
      brainFile = themePath;
      return String(F("SISTEMA: Modulo carregado! Tema: [")) + newTheme + "].";
    } else {
      return String(F("SISTEMA: Erro. O arquivo '")) + themePath + "' nao existe.";
    }
  }

  cleanInput.replace(".", ""); cleanInput.replace(",", ""); cleanInput.replace("?", "");

  if (cleanInput.startsWith("lembra de ")) {
    String term = cleanInput.substring(10);
    term.trim();
    return buscarPensamentos(term);
  }

  if (hasSD && SD.exists(brainFile)) {
    File file = SD.open(brainFile, FILE_READ);
    while (file && file.available()) {
      yield();
      String line = file.readStringUntil('\n');
      line.trim();

      if (line.length() == 0 || line.startsWith("#")) continue;

      int firstPipe = line.indexOf('|');
      if (firstPipe != -1) {
        String keyword = line.substring(0, firstPipe);
        int pos = cleanInput.indexOf(keyword);

        if (pos != -1) {
          lastTopic = keyword;
          String responsesStr = line.substring(firstPipe + 1);
          String responses[5];
          int respCount = 0;

          while (responsesStr.length() > 0 && respCount < 5) {
            int nextPipe = responsesStr.indexOf('|');
            if (nextPipe == -1) { responses[respCount++] = responsesStr; break; }
            else { responses[respCount++] = responsesStr.substring(0, nextPipe); responsesStr = responsesStr.substring(nextPipe + 1); }
          }

          if (respCount == 0) continue;
          String finalResponse = responses[random(0, respCount)];
          if (finalResponse.indexOf("*") != -1) {
            String remainder = cleanInput.substring(pos + keyword.length());
            remainder = reflect(remainder);
            if (remainder.length() == 0) remainder = "isso";
            finalResponse.replace("*", remainder);
          }
          file.close();
          return String(F("ELIZA: ")) + finalResponse;
        }
      }
    }
    if (file) file.close();
  } 
  else {
    for (int i = 0; i < 3; i++) {
      int pos = cleanInput.indexOf(internalRules[i].keyword);
      if (pos != -1) {
        String resp = internalRules[i].response;
        if (resp.indexOf("*") != -1) {
          String rem = cleanInput.substring(pos + String(internalRules[i].keyword).length());
          resp.replace("*", reflect(rem));
        }
        return String(F("ELIZA: ")) + resp;
      }
    }
  }

  const char* const fallbacks[] = {"Continue...", "Pode falar mais?", "Estou ouvindo."};
  return String(F("ELIZA: ")) + fallbacks[random(0, 3)];
}

// ---------------------------------------------------------
// SETUP (Configuração de Boot)
// ---------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  // Força o brilho
  M5Cardputer.Display.setBrightness(128);
  M5Cardputer.Display.setRotation(1);
  
  // APLICAÇÃO DA NOVA FONTE (16 pixels)
  M5Cardputer.Display.setTextFont(&fonts::Font2);
  M5Cardputer.Display.setTextSize(1);
  
  inputBuffer.reserve(128);

  // Redimensiona o Sprite (Canvas) para caber entre o cabeçalho e o input
  canvas.createSprite(M5Cardputer.Display.width() - 4, M5Cardputer.Display.height() - 48);
  canvas.setTextScroll(true);
  canvas.setTextFont(&fonts::Font2); // Mesma fonte no chat
  canvas.setTextSize(1);

  // Inicialização do SD (Robusto para M5Launcher)
  SPI2.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  if (SD.begin(SD_SPI_CS, SPI2)) {
    hasSD = true;
    SD.mkdir("/pensamentos");
    SD.mkdir("/temas");
    
    canvas.setTextColor(COLOR_AI, COLOR_BG);
    canvas.println(F("Sistema Inicializado."));
    canvas.println(F("Memorias Online."));
  } else {
    hasSD = false;
    canvas.setTextColor(COLOR_SYS, COLOR_BG);
    canvas.println(F("AVISO: Falha no SD Card."));
    canvas.println(F("Modo Seguranca Ativo."));
  }

  // Empurra o Sprite para a tela (começando logo abaixo da barra superior)
  canvas.pushSprite(2, 22);
  
  // Desenha os painéis estáticos
  drawHeader();
  drawInput();
}

// ---------------------------------------------------------
// LOOP DE EXECUÇÃO E TECLADO
// ---------------------------------------------------------
void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    for (auto i : status.word) inputBuffer += i;
    
    // CORREÇÃO: Apagar caracteres (Del/Backspace)
    if (status.del && inputBuffer.length() > 2) {
      inputBuffer.remove(inputBuffer.length() - 1);
    }

    if (status.enter) {
      String userInput = inputBuffer.substring(2);
      if (userInput.length() > 0) {
        
        // Renderiza texto do USUÁRIO (Branco)
        canvas.setTextColor(COLOR_USER, COLOR_BG);
        canvas.print(F("\nVoce: "));
        canvas.println(userInput);

        String response = getElizaResponse(userInput);

        if (!userInput.startsWith("/tema ")) {
          logPensamento(userInput, response);
        }

        // Renderiza texto da IA ou do SISTEMA (Ciano ou Laranja)
        if (response.startsWith("SISTEMA:")) {
          canvas.setTextColor(COLOR_SYS, COLOR_BG);
        } else {
          canvas.setTextColor(COLOR_AI, COLOR_BG);
        }
        
        canvas.println(response);
        
        // Atualiza apenas a área do chat
        canvas.pushSprite(2, 22);
      }
      inputBuffer = F("> ");
    }
    // Atualiza a linha do input dinamicamente
    drawInput();
  }
}