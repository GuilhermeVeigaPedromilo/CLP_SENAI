#include <WiFiManager.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <esp_ota_ops.h>

WebServer server(80);
#define SD_CS 2  // Pino do SD Card

// 📌 Função para carregar arquivos do SD
void handleFileRequest() {
    String path = server.uri();
    if (path == "/") path = "/index.html";

    File file = SD.open(path);
    if (!file) {
        server.send(404, F("text/plain"), F("1"));//não encontrado
        return;
    }

    // Define o tipo de conteúdo baseado na extensão
    String contentType = F("text/plain");
    if (path.endsWith(".html")) contentType = F("text/html");
    else if (path.endsWith(".css")) contentType = F("text/css");
    else if (path.endsWith(".js")) contentType = F("application/javascript");

    server.streamFile(file, contentType);
    file.close();
}

// 📌 Alterna para a partição OTA
void handleSwitch() {
    const esp_partition_t *ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (ota_partition) {
        esp_ota_set_boot_partition(ota_partition);
        server.send(200, F("text/html"), F("<h1>Alternando Firmware...</h1>"));
        delay(1000);
        esp_restart();
    } else {
        server.send(500, F("text/html"), F("<h1>OTA não encontrada.</h1>"));
    }
}

// 📌 Atualiza o firmware a partir do SD
void handleUpdateFromSD() {
    String firmwarePath = "/firmware.bin";
    File firmwareFile = SD.open(firmwarePath);
    if (!firmwareFile) {
        server.send(500, F("text/plain"), F("Erro: firmware não encontrado."));
        return;
    }

    size_t fileSize = firmwareFile.size();
    if (!Update.begin(fileSize)) {
        server.send(500, F("text/plain"), F("Erro atualização."));
        return;
    }

    size_t written = Update.writeStream(firmwareFile);
    firmwareFile.close();

    if (written != fileSize || !Update.end() || !Update.isFinished()) {
        server.send(500, F("text/plain"), F("Erro atualização."));
        return;
    }

    server.send(200, F("text/html"), F("<h1>Atualização concluída! </h1>"));
    delay(1000);
    ESP.restart();
}

// 📌 Atualização via OTA
void handleUpload() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Serial.println("Erro atualização!");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Serial.println("Erro firmware!");
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.println("Atualização concluída!");
        else Serial.println("Erro atualização!");
    }
}

// 📌 Configuração inicial
void setup() {
    Serial.begin(115200);

    // Inicializa o cartão SD
    if (!SD.begin(SD_CS)) {
        Serial.println("Falhacartão SD");
        return;
    }
    Serial.println("Cartão inicializado!");

    // Configuração do Wi-Fi via WiFiManager
    WiFiManager wm;
    if (!wm.autoConnect("ESP32_Config")) {
        delay(3000);
        ESP.restart();
    }

    // Configuração do servidor web
    server.onNotFound(handleFileRequest);
    server.on("/switch", handleSwitch);
    server.on("/update_sd", handleUpdateFromSD);
    server.on("/upload", HTTP_POST, []() { server.send(200, F("text/plain"), F("OK")); }, handleUpload);

    server.begin();

    // Se o ESP32 estiver rodando OTA, volte para o firmware principal
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY && factory_partition) {
        esp_ota_set_boot_partition(factory_partition);
        esp_restart();
    }
}

void loop() {
    server.handleClient();
}
