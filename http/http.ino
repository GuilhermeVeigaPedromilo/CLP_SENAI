#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiManager.h>  // Biblioteca do Wi-Fi Manager

#define SD_CS 2  // Pino CS do cartão SD

const char* hostname = "meu-esp32";  // Nome para acesso via mDNS
WebServer server(80);

void setup() {
    Serial.begin(115200);

    // Iniciar cartão SD
    if (!SD.begin(SD_CS)) {
        Serial.println("Falha ao inicializar o SD!");
        return;
    }

    // Tentar conectar ao Wi-Fi
    WiFiManager wifiManager;
    if (WiFi.status() != WL_CONNECTED) {  
        Serial.println("Wi-Fi não conectado! Iniciando Wi-Fi Manager...");
        wifiManager.autoConnect("ESP32_Config");  // Cria um AP chamado "ESP32_Config"
    }

    // Após conectar, inicia o servidor web
    Serial.println("Wi-Fi conectado!");
    iniciarServidorWeb();
}

void loop() {
    server.handleClient();
}

// Função para iniciar o servidor web
void iniciarServidorWeb() {
    if (MDNS.begin(hostname)) {
        Serial.printf("mDNS iniciado! Acesse: http://%s.local\n", hostname);
    } else {
        Serial.println("Falha ao iniciar o mDNS!");
    }

    // Servir página inicial
    server.on("/", []() {
        servirArquivo("/index1.html", "text/html");
    });

    // Servir arquivos CSS e imagens dinamicamente
    server.onNotFound([]() {
        String path = server.uri();
        Serial.println("Requisição para: " + path);
        if (SD.exists(path)) {
            String contentType = getContentType(path);
            servirArquivo(path, contentType);
        } else {
            server.send(404, "text/plain", "Arquivo não encontrado!");
        }
    });

    // AVISO - Receber dados do site e salvar no cartão SD
    server.on("/salvar", HTTP_POST, []() {
        if (server.hasArg("texto")) {
            String texto = server.arg("texto");

            File file = SD.open("/dados.txt", FILE_APPEND);
            if (file) {
                file.println(texto);
                file.close();
                server.send(200, "text/plain", "Dados salvos com sucesso!");
                Serial.println("Dados recebidos e salvos: " + texto);
            } else {
                server.send(500, "text/plain", "Erro ao abrir o arquivo!");
            }
        } else {
            server.send(400, "text/plain", "Nenhum dado recebido!");
        }
    });
    // FIM DO AVISO

    server.begin();
    Serial.println("Servidor web iniciado!");
}

// Função para fornecer qualquer arquivo do SD
void servirArquivo(String path, String contentType) {
    File file = SD.open(path);
    if (!file) {
        server.send(404, "text/plain", "Arquivo não encontrado no SD!");
        return;
    }

    server.streamFile(file, contentType);
    file.close();
}

// Função para definir o tipo de conteúdo correto
String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    else return "text/plain";
}
