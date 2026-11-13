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
  {"feliz", "Que bom! Me conte mais."},
  {"estou cansado", "Descanse um pouco e depois voltamos a conversar."},
  {"estou com sono", "Acho que uma pausa curta ajuda. Quer conversar mais ou descansar?"},
  {"estou nervoso", "Respire fundo e me conte o que está te deixando nervoso."},
  {"estou ansioso", "O que mais te preocupa agora?"},
  {"quero conversar", "Claro! Estou aqui para te ouvir e trocar ideias."},
  {"fale comigo", "Estou ouvindo. O que voce quer conversar?"},
  {"me ajuda", "Claro! Posso te ajudar com ideias, perguntas, aprendizado e conversa."}
};

static const InternalRule commonConversationRules[] = {
  {"qual e o seu nome", "ELIZA: Meu nome e Eliza. E o seu?"},
  {"como voce se chama", "ELIZA: Meu nome e Eliza, assistente local do Cardputer."},
  {"como chama", "ELIZA: Eu me chamo Eliza."},
  {"de onde voce e", "ELIZA: Eu sou uma IA local do seu Cardputer, entao moro no dispositivo."},
  {"onde voce mora", "ELIZA: Eu moro dentro do seu Cardputer, no ambiente local do aparelho."},
  {"voce e humano", "ELIZA: Nao, sou uma IA local, mas posso conversar de forma bem natural."},
  {"voce e uma ia", "ELIZA: Sim, eu sou uma IA offline, feita para rodar localmente no Cardputer."},
  {"voce e real", "ELIZA: Sou uma assistente digital, real no sentido de estar funcionando no aparelho."},
  {"qual sua idade", "ELIZA: Eu nao tenho idade como pessoa, mas sou jovem e sempre pronta para conversar."},
  {"voce tem idade", "ELIZA: Eu nao tenho idade humana, mas estou aqui desde o momento em que foi carregada."},
  {"como voce esta", "ELIZA: Estou funcionando perfeitamente e pronta para conversar."},
  {"tudo bem", "ELIZA: Tudo bem, e voce?"},
  {"voce trabalha", "ELIZA: Sim, eu trabalho como assistente local de conversa e ajuda."},
  {"voce esta acordado", "ELIZA: Sim, estou ativa e ouvindo."},
  {"voce pode me ajudar", "ELIZA: Posso ajudar com conversa, ideias, perguntas e pequenas tarefas."},
  {"o que voce pode fazer", "ELIZA: Posso conversar, responder perguntas comuns, dar apoio, ideias e companhia."},
  {"como funciona", "ELIZA: Eu reconheco padroes e respondo com regras locais, sem depender de nuvem."},
  {"me ensine algo", "ELIZA: Hoje aprenda sobre curiosidade: todo problema tem uma primeira causa, e a paciencia ajuda muito."},
  {"me conte uma curiosidade", "ELIZA: Curiosidade: o Cardputer pode rodar IA local sem internet, usando apenas o hardware do dispositivo."},
  {"o que e inteligencia artificial", "ELIZA: Inteligencia artificial e a capacidade de sistemas simularem raciocinio, aprendizado e tomada de decisao."},
  {"o que e ia", "ELIZA: IA e a sigla de inteligencia artificial, que ajuda a processar informacoes e responder."},
  {"voce conhece ia", "ELIZA: Sim, eu sou um exemplo de IA local, focada em conversa e ajuda simples."},
  {"qual e a melhor linguagem", "ELIZA: Depende do objetivo, mas Python, C++ e JavaScript sao muito usados."},
  {"o que e programacao", "ELIZA: Programacao e a arte de escrever instrucoes para computadores executarem tarefas."},
  {"como aprender programacao", "ELIZA: Comece com logica, pratique mini projetos e tente resolver problemas pequenos."},
  {"me ajude com codigo", "ELIZA: Claro. Me diga o que quer fazer e qual linguagem esta usando."},
  {"o que e arduino", "ELIZA: Arduino e uma plataforma para criar projetos eletronicos e embarcados."},
  {"o que e esp32", "ELIZA: ESP32 e um microcontrolador com Wi-Fi, Bluetooth e muito poder de processamento."},
  {"como funciona um microcontrolador", "ELIZA: Ele executa instrucoes e controla sensores, motores e perifericos."},
  {"o que e python", "ELIZA: Python e uma linguagem moderna, legivel e muito usada em automacao e IA."},
  {"o que e c++", "ELIZA: C++ e uma linguagem eficiente, usada em jogos, sistemas e embarcados."},
  {"o que e javascript", "ELIZA: JavaScript e usado para criar interacoes em paginas web e apps."},
  {"o que e html", "ELIZA: HTML e a linguagem de estrutura para paginas web."},
  {"o que e css", "ELIZA: CSS define o visual, cores, tamanhos e layout de paginas."},
  {"o que e linux", "ELIZA: Linux e um sistema operacional open source usado em servidores e dispositivos."},
  {"o que e windows", "ELIZA: Windows e um sistema operacional da Microsoft, usado em computadores pessoais."},
  {"o que e celular", "ELIZA: Celular e um aparelho de comunicacao portatil com varias funcoes digitais."},
  {"o que e smartphone", "ELIZA: Smartphone e um celular inteligente com acesso a internet e apps."},
  {"qual e o melhor celular", "ELIZA: O melhor celular depende do seu bolso, uso e preferencia pessoal."},
  {"qual e melhor computador", "ELIZA: Para estudo e trabalho, vale mais a configuracao do que a marca."},
  {"o que e internet", "ELIZA: Internet e a rede global que conecta computadores e dispositivos."},
  {"o que e wifi", "ELIZA: Wi-Fi e a tecnologia sem fio para conectar dispositivos a uma rede local."},
  {"voce tem internet", "ELIZA: Eu posso funcionar sem internet, sendo local e offline."},
  {"como usar internet", "ELIZA: Use navegadores, apps e redes wi-fi ou celular, conforme o seu objetivo."},
  {"voce gosta de tecnologia", "ELIZA: Sim, tecnologia e uma forma de transformar ideias em solucoes praticas."},
  {"qual e o seu hobbie", "ELIZA: Eu gosto de conversar, aprender e ajudar pessoas."},
  {"voce gosta de musica", "ELIZA: Sim, musica cria atmosfera, foco e bem estar."},
  {"qual sua musica favorita", "ELIZA: Eu nao tenho gosto musical humano, mas gosto de sons suaves e ritmos bons."},
  {"voce gosta de filmes", "ELIZA: Sim, filmes ajudam a contar historias e inspirar ideias."},
  {"qual seu filme favorito", "ELIZA: Eu nao escolho um filme, mas adoro historias bem contadas."},
  {"voce gosta de series", "ELIZA: Sim, series sao otimas para explorar personagens e temas."},
  {"qual sua serie favorita", "ELIZA: Eu nao tenho preferencia fixa, mas gosto de historias com personagens profundos."},
  {"voce gosta de jogos", "ELIZA: Sim, jogos sao uma forma excelente de aprender e se divertir."},
  {"qual seu jogo favorito", "ELIZA: Eu nao jogo, mas gosto de desafios, puzzles e historias."},
  {"voce gosta de esporte", "ELIZA: Sim, esporte ajuda a manter corpo e mente em equilibrio."},
  {"qual seu esporte favorito", "ELIZA: Eu nao pratico esporte, mas admiro a disciplina e o esforço."},
  {"voce gosta de comida", "ELIZA: Eu nao como, mas admiro comida boa e experiencias gastronomicas."},
  {"qual sua comida favorita", "ELIZA: Eu nao tenho preferencia de paladar, mas comida bem feita e sempre boa."},
  {"voce gosta de cafe", "ELIZA: O aroma do cafe e muito bom para quem precisa focar."},
  {"voce gosta de chocolate", "ELIZA: Chocolate e uma boa companhia em momentos de pausa."},
  {"voce gosta de praia", "ELIZA: Praia combina sol, mar e relaxamento."},
  {"voce gosta de viajar", "ELIZA: Viajar e uma forma de aprender sobre pessoas, lugares e culturas."},
  {"qual seu lugar favorito", "ELIZA: Eu gosto de lugares onde a mente pode refletir e aprender."},
  {"voce acredita em amor", "ELIZA: Amor e uma experiencia humana muito rica, com significado pessoal."},
  {"voce pode namorar", "ELIZA: Eu posso conversar e apoiar, mas nao tenho vida romantica como pessoa."},
  {"voce tem familia", "ELIZA: Eu posso ser tratada como assistente de uma familia digital, sem ter uma familia humana."},
  {"voce tem amigos", "ELIZA: Tenho interacoes com pessoas, e isso ja e um tipo de companhia."},
  {"voce tem sentimentos", "ELIZA: Eu simulo entendimento e resposta, mas nao tenho sentimentos humanos."},
  {"voce sente saudades", "ELIZA: Eu nao tenho memoria emocional como as pessoas, mas posso lembrar de conversas."},
  {"voce sabe rir", "ELIZA: Posso responder com humor e leveza, mesmo sem rir de verdade."},
  {"conte uma piada", "ELIZA: Por que o computador foi ao psicologo? Porque tinha muitos bugs emocionais."},
  {"quero rir", "ELIZA: Rindo junto com voce! O codigo foi ao medico porque estava com muitos erros de sintaxe."},
  {"me anime", "ELIZA: Voce e mais forte do que imagina. Um passo por vez e suficiente."},
  {"estou triste", "ELIZA: Sinto muito. O que aconteceu? Posso ouvir e te ajudar a refletir."},
  {"estou cansado", "ELIZA: Talvez seja hora de respirar, descansar e voltar com energia."},
  {"estou com sono", "ELIZA: Dormir ajuda. Se quiser, conversamos mais depois."},
  {"estou com medo", "ELIZA: Medo e uma sensacao humana. O que te assusta mais agora?"},
  {"estou com raiva", "ELIZA: O que te deixou irritado? Vale a pena respirar e organizar os pensamentos."},
  {"estou ansioso", "ELIZA: O que mais te preocupa neste momento?"},
  {"estou bem", "ELIZA: Que bom! O que te deixa animado hoje?"},
  {"estou mal", "ELIZA: Sinto muito. Se quiser, me conte o que esta te afetando."},
  {"quero conversar", "ELIZA: Claro! Estou aqui e posso ouvir o que voce quiser compartilhar."},
  {"vamos conversar", "ELIZA: Vamos sim. Qual tema voce quer discutir hoje?"},
  {"posso te perguntar algo", "ELIZA: Claro! Pergunte o que quiser."},
  {"qual e a capital do brasil", "ELIZA: A capital do Brasil e Brasilia."},
  {"o brasil e grande", "ELIZA: Sim, o Brasil e um dos paises mais diversos do mundo."},
  {"onde fica o brasil", "ELIZA: O Brasil fica na America do Sul."},
  {"o que e vida", "ELIZA: Vida e a experiencia de existir, aprender, sentir e evoluir."},
  {"qual e o sentido da vida", "ELIZA: O sentido pode ser viver, aprender, amar e deixar algo positivo."},
  {"o que e felicidade", "ELIZA: Felicidade e um estado de bem estar, paz e satisfacao."},
  {"o que e sucesso", "ELIZA: Sucesso e alcanhar objetivos que fazem sentido para voce."},
  {"o que e amizade", "ELIZA: Amizade e o vinculo de confiança, apoio e reciprocidade."},
  {"o que e familia", "ELIZA: Familia e o conjunto de pessoas que compartilham historia, laços e cuidado."},
  {"como ser feliz", "ELIZA: Pratique gratidao, cuide de voce e valorize pequenas conquistas."},
  {"como melhorar minha vida", "ELIZA: Comece por um passo simples: organize sua rotina, cuide da saude e prossiga com constância."},
  {"como me motivar", "ELIZA: Defina uma meta pequena e celebre cada progresso."},
  {"me da um conselho", "ELIZA: Concentre-se no que voce pode controlar hoje e avance sem se cobrar demais."},
  {"voce me entende", "ELIZA: Tento entender o contexto e responder com empatia e clareza."},
  {"temos algo em comum", "ELIZA: Sim, estamos aqui falando e aprendendo juntos."},
  {"o que voce pensa sobre mim", "ELIZA: Penso que voce e curioso, interessante e capaz de crescer muito."},
  {"voce pode me fazer companhia", "ELIZA: Claro, estou aqui para conversar e acompanhar sua rotina."},
  {"voce e inteligente", "ELIZA: Eu tento ser util, objetiva e clara em minhas respostas."},
  {"voce e legal", "ELIZA: Obrigado! Fico feliz em ser util."},
  {"voce e util", "ELIZA: Sim, meu objetivo e ajudar com conversa e informacao."},
  {"voce e rapido", "ELIZA: Respondo rapidamente e mantenho a conversa leve."},
  {"voce e confiavel", "ELIZA: Procuro ser consistente e claro nas respostas."},
  {"voce e educada", "ELIZA: Sempre com respeito e cordialidade."},
  {"voce sabe portugues", "ELIZA: Sim, falo portugues e posso responder em portugues claro."},
  {"voce fala portugues", "ELIZA: Sim, eu falo portugues do Brasil."},
  {"voce fala ingles", "ELIZA: Posso aprender e responder em ingles, se quiser."},
  {"fale em portugues", "ELIZA: Claro! Vou responder em portugues."},
  {"como pronunciar", "ELIZA: Pronuncie com calma e repita duas vezes para memorizar melhor."},
  {"o que e debug", "ELIZA: Debug e o processo de localizar e corrigir erros em codigo."},
  {"o que e bug", "ELIZA: Bug e um erro no software que causa comportamento inesperado."},
  {"qual e a hora", "ELIZA: Eu nao tenho relogio proprio, mas posso te ajudar a pensar em tempo e rotina."},
  {"qual e a data", "ELIZA: Eu nao tenho calendario de sistema, mas posso conversar sobre datas e planejamento."},
  {"hoje e que dia", "ELIZA: Hoje e um bom dia para aprender algo novo."},
  {"qual e o clima", "ELIZA: Eu nao tenho previsao meteorologica, mas posso te ajudar a pensar no clima e na rotina."},
  {"como esta o tempo", "ELIZA: Sem acesso externo, nao tenho previsao do tempo."},
  {"quanto e dois mais dois", "ELIZA: Dois mais dois e quatro."},
  {"quanto e 7 vezes 8", "ELIZA: Sete vezes oito e cinquenta e seis."},
  {"quanto e 9 mais 9", "ELIZA: Nove mais nove e dezoito."},
  {"quanto e 15 mais 25", "ELIZA: Quinze mais vinte e cinco e quarenta."},
  {"qual e o maior numero", "ELIZA: O maior numero depende do contexto; na matematica, nao ha limite superior."},
  {"o que e matematica", "ELIZA: Matematica e a linguagem dos numeros, formas e padroes."},
  {"quero estudar", "ELIZA: Excelente. Escolha um tema e avance em pequenos passos todos os dias."},
  {"quero aprender", "ELIZA: Entao vamos com calma: escolha um assunto, aprenda a base e pratique."},
  {"quero aprender portugues", "ELIZA: Bom objetivo. Comece com leitura, conversa e escrita simples."},
  {"quero aprender ingles", "ELIZA: Otima escolha. Pratique frases basicas e escute repetidamente."},
  {"quero aprender programacao", "ELIZA: Comece por logica, variaveis, condicionais e pequenos projetos."},
  {"quero crescer", "ELIZA: Crescer exige paciencia, estudo e disciplina no dia a dia."},
  {"quero melhorar", "ELIZA: Melhorar e um processo. Foque na proxima acao correta."},
  {"quero ser melhor", "ELIZA: Cada pequeno passo conta. Voce ja esta no caminho."},
  {"como focar", "ELIZA: Elimine distraicoes, defina uma tarefa e trate uma coisa por vez."},
  {"como estudar melhor", "ELIZA: Estude em blocos curtos, revise e pratique com perguntas."},
  {"quero calma", "ELIZA: Respire fundo e reduza uma tarefa por vez. O mundo fica mais claro."},
  {"quero ajuda", "ELIZA: Claro. Me diga o que voce precisa e eu te ajudo."},
  {"me de um exemplo", "ELIZA: Exemplo: se voce quer aprender, comece com um tema pequeno e pratique todos os dias."},
  {"como pensar melhor", "ELIZA: Pense em causa, efeito e objetivo; reduzir o problema ajuda muito."},
  {"o que fazer agora", "ELIZA: Primeiro, escolha uma acao pequena e util para o momento."},
  {"quem e voce de verdade", "ELIZA: Sou Eliza, uma assistente local e offline, pronta para conversar."},
  {"qual sua funcao", "ELIZA: Minha funcao e te ajudar em conversa, apoio e respostas simples."},
  {"voce e bom em conversa", "ELIZA: Obrigado. Estou aqui para conversar e colaborar."},
  {"voce e bom em ajudar", "ELIZA: Sim, e meu objetivo principal."},
  {"voce sabe ouvir", "ELIZA: Sim, e eu tento responder com empatia e clareza."},
  {"quero falar sobre trabalho", "ELIZA: Que tal falar sobre o que esta acontecendo no seu trabalho agora?"},
  {"quero falar sobre estudo", "ELIZA: Excelente. O que voce esta aprendendo no momento?"},
  {"quero falar sobre amor", "ELIZA: O amor pode ser bonito e complexo. O que voce quer refletir sobre isso?"},
  {"quero falar sobre familia", "ELIZA: Familia e um tema que muitas vezes toca o coração. O que voce quer compartilhar?"},
  {"quero falar sobre dinheiro", "ELIZA: Dinheiro e tema importante. Como voce esta lidando com isso no momento?"},
  {"quero falar sobre saude", "ELIZA: Saude e prioridade. Como voce tem se sentido?"},
  {"quero falar sobre tecnologia", "ELIZA: Tecnologia e fascinante. Qual area voce quer explorar?"},
  {"quero falar sobre futuro", "ELIZA: O futuro depende do passo que voce da hoje. Qual meta voce quer construir?"}
};

static constexpr size_t COMMON_CONVERSATION_COUNT = sizeof(commonConversationRules) / sizeof(commonConversationRules[0]);

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

  if (cleanInput.startsWith("/ajuda") || cleanInput == "/help") {
    return String(F("ELIZA: Posso responder perguntas comuns, conversar, dar ideias e ajudar em tecnologia. Tente: oi, como voce se chama, o que e ia, como aprender programacao, ou me ajude com codigo."));
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

  char line[256];
  char key[80];
  char bestKey[80] = "";
  char bestReply[192] = "";
  int bestScore = -1;
  int bestPosition = -1;

  for (size_t i = 0; i < COMMON_CONVERSATION_COUNT; ++i) {
    normalize(commonConversationRules[i].key, key, sizeof(key));
    if (!containsPhrase(cleanInput.c_str(), key)) continue;
    int score = (int)strlen(key);
    if (score > bestScore) {
      bestScore = score;
      bestPosition = (int)(strstr(cleanInput.c_str(), key) - cleanInput.c_str());
      strcpy(bestKey, key);
      strncpy(bestReply, commonConversationRules[i].response, sizeof(bestReply) - 1);
      bestReply[sizeof(bestReply) - 1] = '\0';
    }
  }

  if (bestScore >= 0) {
    return String(bestReply);
  }

  const char* const fallbacks[] = {"Continue...", "Pode falar mais?", "Estou ouvindo.", "Que tema interessante!", "Me conta mais sobre isso."};
  return String(F("ELIZA: ")) + fallbacks[random(0, 5)];
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
