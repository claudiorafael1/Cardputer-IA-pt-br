# 🧠 Eliza Neural OS — Assistente IA Autônomo para M5Stack Cardputer

O **Eliza Neural OS** é um sistema conversacional inteligente, leve e de código aberto projetado especificamente para rodar no **M5Stack Cardputer** (baseado no ESP32-S3). 

Diferente de modelos de linguagem pesados que exigem nuvem ou GPUs potentes, este projeto resgata a clássica arquitetura de reconhecimento de padrões e inversão dinâmica inspirada no algoritmo **ELIZA**, turbinada com um sistema avançado de **Big Data no Edge** que lê e grava dados diretamente em um Cartão MicroSD.

---

## ✨ Principais Funcionalidades

- **💾 Armazenamento Externo Dinâmico (`/regras.txt`):** O "cérebro" da IA não ocupa a memória RAM do chip. As regras de conversação ficam em arquivos de texto no cartão SD, permitindo bases de conhecimento gigantescas.
- **🔄 Inversão Dinâmica de Pronomes:** A IA transforma a perspectiva da frase do usuário em tempo real (ex: de *"eu estou"* para *"você está"*), gerando interações muito mais naturais.
- **📂 Sistema de Pensamentos Autônomo (`/pensamentos/log.txt`):** O Cardputer registra automaticamente o histórico de conversas em segundo plano. Você pode consultar memórias antigas usando comandos como *"lembra de [assunto]"*.
- **⚡ Big Data em Microcontroladores:** Motor de busca reversa em blocos de 1KB (`Chunking`) otimizado para vasculhar gigabytes de texto em milissegundos sem causar estouro de pilha (*Kernel Panic*).
- **🔀 Multi-Cérebros (Personas Dinâmicas):** Troque o foco do assistente em tempo real usando comandos de terminal (`/tema rpg`, `/tema tech`, etc.), carregando arquivos específicos da pasta `/temas/`.
- **🎨 Interface Cyberpunk Otimizada:** Visual de terminal customizado com paleta de cores moderna, fontes limpas embutidas na Flash (sem desperdício de RAM) e cabeçalho com status do Cartão SD.
- **🚀 Compatível com M5Launcher:** Desenvolvido para inicializar perfeitamente e coexistir de forma limpa com gerenciadores de boot como o M5Launcher.

---

## 🛠️ Requisitos de Hardware

- **Dispositivo:** M5Stack Cardputer (ESP32-S3)
- **Armazenamento:** Cartão MicroSD formatado em **FAT32** (inserido no slot integrado do Cardputer).

---

## 📂 Estrutura de Pastas e Arquivos no Cartão SD

Para que todas as funcionalidades operem corretamente, prepare o seu cartão MicroSD com a seguinte estrutura na raiz:

```text
📁 (Raiz do Cartão SD)
 ├── 📄 regras.txt           # Cérebro de conversação padrão (obrigatório)
 ├── 📁 pensamentos/         # Criado automaticamente pelo sistema (armazena o log.txt)
 └── 📁 temas/               # Pasta opcional para múltiplos cérebros
      ├── 📄 tech.txt        # Exemplo: regras focadas em tecnologia/programação
      └── 📄 rpg.txt         # Exemplo: regras focadas em jogos de texto
