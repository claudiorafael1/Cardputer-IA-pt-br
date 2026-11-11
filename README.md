# Eliza Neural OS — assistente offline para M5Stack Cardputer

Projeto de conversa local inspirado na ELIZA, feito para o M5Stack Cardputer com ESP32-S3. O firmware reconhece frases por regras, mantém o contexto imediato e pode consultar o histórico no cartão microSD. **Não executa um LLM**: a qualidade da conversa vem das regras e respostas preparadas em texto.

## O que foi otimizado

- **Buffers de tamanho fixo:** o motor de conversa não usa `String` para transformar, comparar e concatenar mensagens. Isso evita alocações repetidas no heap e reduz o risco de fragmentação durante uso prolongado. A entrada é limitada a 127 bytes.
- **Leitura em streaming:** `regras.txt`, arquivos de tema e histórico são lidos linha por linha em buffers limitados; o arquivo inteiro nunca é carregado na RAM. Linhas de regra acima do limite são descartadas com segurança.
- **Seleção sem listas temporárias:** para cada regra coincidente, uma resposta é sorteada sem criar arrays dinâmicos de objetos `String`.
- **Regras mais específicas primeiro:** entre as regras coincidentes, vence a frase de maior comprimento. Isso permite ter regras específicas e gerais no mesmo arquivo.
- **Normalização UTF-8 para português:** acentos comuns são convertidos durante a cópia para um buffer ASCII, permitindo comparar `ação` e `acao` sem cadeias de `String.replace()`.
- **Sprite de 8 bits:** a área de conversa usa RGB332, consumindo aproximadamente metade da RAM do sprite anterior de 16 bits. O tamanho real depende da resolução e da biblioteca M5GFX.
- **Limites explícitos:** entrada 128 bytes, linha do arquivo 256 bytes, palavra-chave 80 bytes e resposta 192 bytes. Ajuste esses limites apenas se necessário.
- **Dados extensíveis no SD:** adicione conteúdo e personas sem recompilar o firmware, respeitando o formato abaixo.

A otimização prioriza **RAM e estabilidade do heap**, não a redução do arquivo binário. Textos fixos e regras de fallback continuam no firmware/flash; regras grandes devem ficar no cartão SD. O cartão SD tem bastante capacidade, mas as linhas e respostas precisam caber nos limites definidos no código.

## Hardware e compilação

- M5Stack Cardputer baseado em ESP32-S3.
- Cartão microSD FAT32 para regras, temas e histórico persistente.
- Arduino IDE ou PlatformIO com as bibliotecas M5Cardputer/M5GFX compatíveis.

Compile e grave `eliza.ino` como um sketch Arduino. O arquivo `eliza.ino.bin` existente no repositório é um binário pré-compilado e **não é regenerado automaticamente** quando o código-fonte muda; recompile-o para obter firmware com as alterações.

## Arquivos no cartão SD

```text
/
├── regras.txt
├── pensamentos/       (criada pelo firmware; contém log.txt)
└── temas/
    ├── tech.txt
    └── rpg.txt
```

As regras usam o formato `frase-chave|resposta 1|resposta 2`. Uma resposta pode usar `*` para inserir o trecho da entrada após a palavra-chave:

```text
# Comentarios comecam com #
estou triste|Sinto muito. O que aconteceu?|Quer me contar mais sobre isso?
eu gosto de|O que voce gosta em *?|Quando voce percebeu que gosta de *?
programacao|Qual projeto voce esta criando?|Que parte da programacao voce quer explorar?
```

Use frases distintas e específicas. O motor escolhe a correspondência de maior comprimento; pontuação e acentos são normalizados antes da comparação. Cada linha deve ter menos de 255 bytes. As respostas podem ser separadas por `|`; limite-as a cinco opções para compatibilidade com ferramentas de edição, embora o firmware sorteie entre todas as opções da linha que caibam no buffer de leitura.

## Comandos

- `/ajuda` — mostra comandos disponíveis.
- `/tema tech` — carrega `/temas/tech.txt` (somente letras, números, `_` e `-` no nome).
- `/tema geral` — retorna a `/regras.txt`.
- `/memoria termo` ou `lembra de termo` — procura a ocorrência mais recente nas falas do usuário no histórico do SD.
- `/limpar` — apaga o contexto imediato em RAM, sem apagar o arquivo de histórico.

Sem cartão SD, o assistente continua com as regras internas, mas não consegue carregar temas nem consultar/gravar histórico persistente.

## Limites e próximos passos

Este é um mecanismo simbólico embarcado, não uma rede neural generativa. Aumentar a lista de regras melhora a cobertura, não a capacidade de raciocínio geral. Escreva várias respostas naturais para cada intenção e teste sobreposições entre frases-chave.

A busca de memória é streaming e usa pouca RAM, mas percorre o log inteiro; o tempo cresce conforme o histórico aumenta. Se esse custo se tornar perceptível, uma próxima evolução seria rotacionar o log ou criar um índice compacto no SD. Não há alegação de economia medida de flash ou benchmark de velocidade: esses valores dependem da versão do core Arduino, M5GFX, configuração de compilação e cartão.
