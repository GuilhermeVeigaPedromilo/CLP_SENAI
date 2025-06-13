#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include "freertos/semphr.h" 
SemaphoreHandle_t sdMutex;

#define PINO_SD 5  // Pino onde o SD está conectado

AsyncWebServer server(80);

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Gravação : " + String(written) + " Foi um sucesso");
    } else {
      Serial.println("Foi escrito apenas : " + String(written) + "/" + String(updateSize) + ". Tentar novamente?");
    }
    if (Update.end()) {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Carregado com sucesso");
        ESP.restart();
      } else {
        Serial.println("Atualização não finalizada? Algo deu errado!");
      }
    } else {
      Serial.println("Ocorreu um erro. Erro nº: " + String(Update.getError()));
    }
  } else {
    Serial.println("Não há espaço suficiente para iniciar o OTA");
  }

}

// check given FS for valid update.bin and perform update if available
void updateFromFS(fs::FS &fs, const String name) {
  Serial.println("Nome do arquivo a ser carregado " + name);
  if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
  File updateBin = fs.open(name);
  if (updateBin) {
    if (updateBin.isDirectory()) {
      Serial.println("Erro, " + name + "Não foi encontrado");
      updateBin.close();
      xSemaphoreGive(sdMutex); // Libera o SD
      return;
    }
    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Serial.println("Tentando iniciar a atualização");
      performUpdate(updateBin, updateSize);
    } else {
      Serial.println("Erro, arquivo vazio");
    }
    updateBin.close();
    // when finished remove the binary from sd card to indicate end of the process
  } else {
    Serial.println("Não foi possivel carregar " + name + " do SD");
  }
  xSemaphoreGive(sdMutex); // Libera o SD
  }
}
//ladder
void startUpdateTask(void *parameter) {
  ArduinoOTA.end();
  delay(100);
  updateFromFS(SD,"/ladder.bin");
  vTaskDelete(NULL);
}
void ladder(){
  Serial.println("carregou o ladder");
  // Fechar todas as conexões antes da 
  delay(100);
  Serial.printf("Endereço de startUpdateTask: %p\n", (void *)startUpdateTask);
  xTaskCreatePinnedToCore(
    startUpdateTask, "UpdateTask", 8192, NULL, 1, NULL, 0);
}// fim ladder
//mqtt
void startUpdateTask2(void *parameter) {
  ArduinoOTA.end();
  delay(100);
  updateFromFS(SD,"/mqtt.bin");
  vTaskDelete(NULL);
}
void mqtt(){
  Serial.println("carregou o mqtt");
  // Fechar todas as conexões antes da atualização
  delay(100);
  Serial.printf("Endereço de startUpdateTask2: %p\n", (void *)startUpdateTask2);
  xTaskCreatePinnedToCore(
    startUpdateTask2, "UpdateTask", 8192, NULL, 1, NULL, 0);
}//fim mqtt

void setup() {
  Serial.begin(115200);
  sdMutex = xSemaphoreCreateMutex();
    if (!SD.begin(PINO_SD)) {
        Serial.println("Falha ao iniciar o cartão SD");
    } else {
        Serial.println("Cartão SD iniciado com sucesso");
    }
  WiFiManager gerenciadorWiFi;
    if (!gerenciadorWiFi.autoConnect("CLP-Config")) {
        Serial.println("Falha ao conectar ao Wi-Fi. Reiniciando...");
        ESP.restart();
    }
    Serial.println("Conectado ao Wi-Fi!");
     if (MDNS.begin("clp")) {
        Serial.println("mDNS configurado. Acesse: http://clp.local");
    } else {
        Serial.println("Falha ao configurar mDNS");
    }

   ArduinoOTA.setHostname("Clp");
   ArduinoOTA.setPassword("admin");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
  ArduinoOTA.begin();

    // Enviar index.html como página html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    // Disponibiliza a página /index.html
    request->send(SD, "/index.html");
  });


  server.on("/ladder", HTTP_POST, [](AsyncWebServerRequest *request){
    // Envia a resposta HTTP imediatamente
    request->send(200, "text/html", "ok");

    ladder();
  });
  //server.on("/ladder", HTTP_POST, ladder);
  server.on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.print("mqtt");
      // Envia a resposta HTTP imediatamente
    request->send(200, "text/html", "ok");
    mqtt();
  });
//atualização via http
  
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", Update.hasError() ? "Falha na atualização!" : "Atualização bem-sucedida! Reiniciando...");
        delay(100);
        ESP.restart();
    }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            Serial.printf("Recebendo: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.println("Atualização concluída!");
            } else {
                Update.printError(Serial);
            }
        }
    });
  server.serveStatic("/", SD, "/");
  server.begin();
  Serial.println("iniciado");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();
}
