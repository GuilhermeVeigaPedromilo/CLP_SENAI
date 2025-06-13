# 🚀 CLP-SENAI – CLP de Baixo Custo
<img src="./public/media/Circuito com Potênciometro - CLP Monitorando dados.jpeg">

---
Projeto desenvolvido como trabalho final do Curso Técnico em Eletroeletrônica da Escola SENAI “Prof. João Baptista Salles da Silva” – Americana/SP.

## 📘 Resumo

Este projeto propõe o desenvolvimento de um **Controlador Lógico Programável (CLP)** de baixo custo baseado no microcontrolador **ESP32**, com foco educacional para o ensino técnico em automação industrial. O sistema implementa funcionalidades práticas como interface web responsiva, suporte a protocolos industriais (Modbus RTU e MQTT) e atualização remota de firmware (OTA), possibilitando uma experiência realista e interativa.

## 🎯 Objetivo

Oferecer uma solução acessível, modular e escalável que simule as funções essenciais de um CLP industrial, viabilizando o aprendizado prático da automação industrial em instituições educacionais com orçamento limitado.

### Objetivos Específicos

- Configurar o ESP32 para operar como um CLP com ciclo de scan.
- Desenvolver uma interface web responsiva.
- Implementar comunicação via **MQTT** e suporte ao protocolo **Modbus RTU**.
- Criar uma **placa de circuito impresso (PCI)** modular.
- Incluir funcionalidade de atualização OTA do firmware.
- Validar o desempenho com testes práticos.

## 🛠️ Tecnologias e Componentes

- **ESP32 DevKit V1**
- **MQTT (Mosquitto)**
- **Modbus RTU**
- **Arduino IDE + Bibliotecas:**
  - WiFiManager
  - ESPAsyncWebServer
  - PubSubClient
  - ModbusMaster
  - ArduinoOTA
- **Interface Web:** HTML, CSS, JavaScript
- **Componentes eletrônicos:** relés, optoacopladores, reguladores de tensão, etc.

## 🚀 Funcionalidades do Sistema

- Interface gráfica para controle via navegador
- Comunicação com dispositivos IoT via MQTT
- Comunicação padrão industrial com sensores e atuadores via Modbus RTU
- Atualização remota de firmware via OTA
- Monitoramento e controle de entradas/saídas digitais e analógicas

## 🧠 Público-Alvo

Estudantes e professores de cursos técnicos em automação e eletrônica que buscam uma alternativa prática, acessível e moderna para aprendizagem de CLPs.

## 👨‍💻 Equipe

- Eduardo Trevisan Fernandes  
- Guilherme Veiga Pedromilo  
- Paulo Ricardo Fondello Satelis  
- Florivaldo Antonio Romera Garcia  

**Orientadores:**  
- Caio Cesar Alves dos Santos  
- Rafael Olenk Zani  
- Rogério Aparecido Silva  

## 🗂️ Estrutura de Documentação

O repositório inclui os seguintes diretórios com informações adicionais:

- [`public/docs`](./public/docs) – Documentação técnica, esquemas e relatórios do projeto.
- [`public/media`](./public/media) – Imagens e mídias relacionadas ao desenvolvimento do projeto, incluindo a construção da PCI, interface web, entre outros.

## 📎 Link do Projeto

🔗 [Repositório GitHub](https://github.com/GuilhermeVeigaPedromilo/CLP_SENAI.git)

---

> Projeto desenvolvido em 2025 – Americana/SP – Curso Técnico em Eletroeletrônica – SENAI
