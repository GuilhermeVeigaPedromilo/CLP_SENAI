# 🚀 CLP-SENAI

Este projeto tem como objetivo desenvolver um **CLP (Controlador Lógico Programável) de baixo custo** utilizando o **ESP32**, permitindo automações **residenciais e industriais**. A interface homem-máquina (**IHM**) é baseada em uma aplicação **Web**, tornando a configuração e o monitoramento do sistema simples e acessível.

## 🛠️ Tecnologias Utilizadas

- **ESP32** – Microcontrolador com Wi-Fi e Bluetooth integrados
- **C/C++** – Programação do firmware (Arduino/ESP-IDF)
- **HTML, CSS, JavaScript** – Interface Web para controle e monitoramento
- **WebSockets/HTTP** – Comunicação entre a IHM e o ESP32

## 📌 Funcionalidades

✅ Controle remoto via IHM Web  
✅ Automação de dispositivos e processos industriais  
✅ Comunicação via Wi-Fi  
✅ Integração com sensores e atuadores  
✅ Atualizações OTA (Over-The-Air)  
✅ Suporte a múltiplas entradas e saídas  

## 🎛️ Como Funciona?

1. O **ESP32** atua como um servidor Web, disponibilizando uma interface para controle.  
2. Sensores e atuadores são conectados às GPIOs do ESP32.  
3. A comunicação entre a IHM e o ESP32 pode ser feita via **HTTP ou WebSockets**.  
4. O usuário pode acessar a interface de qualquer dispositivo conectado à mesma rede. 

## 📜 Licença

Este projeto é distribuído sob a licença **MIT**.  

---
