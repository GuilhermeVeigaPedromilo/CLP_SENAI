#include <SmartLadderEsp32.h>
#include <SD.h>
#include "FS.h"
#include <Update.h>
#include <ArduinoOTA.h>

#define PINO_SD 5              // Pino CS do cartão SD
#define FACTORY_RESET_PIN 15   // Botão de reset de fábrica

SemaphoreHandle_t sdMutex;
SmartLadderEsp32 smartLadder;

// Função para realizar a atualização OTA
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("✅ Gravação concluída com sucesso.");
    } else {
      Serial.println("⚠️ Gravação incompleta.");
    }

    if (Update.end()) {
      if (Update.isFinished()) {
        Serial.println("🚀 Atualização concluída! Reiniciando...");
        ESP.restart();
      } else {
        Serial.println("❌ Atualização não finalizada.");
      }
    } else {
      Serial.printf("❌ Erro de finalização da atualização: %d\n", Update.getError());
    }
  } else {
    Serial.println("❌ Sem espaço suficiente para OTA.");
  }
}

// Verifica e carrega o arquivo .bin do SD
void updateFromFS(fs::FS &fs, const String name) {
  Serial.println("📂 Verificando arquivo: " + name);

  if (xSemaphoreTake(sdMutex, portMAX_DELAY)) {
    File updateBin = fs.open(name);

    if (!updateBin || updateBin.isDirectory()) {
      Serial.println("❌ Arquivo inválido ou não encontrado.");
      xSemaphoreGive(sdMutex);
      return;
    }

    size_t updateSize = updateBin.size();
    if (updateSize > 0) {
      Serial.println("🔧 Iniciando atualização...");
      performUpdate(updateBin, updateSize);
    } else {
      Serial.println("⚠️ Arquivo está vazio.");
    }

    updateBin.close();
    xSemaphoreGive(sdMutex);
  }
}

// Tarefa OTA em segundo plano
void startUpdateTask(void *parameter) {
  ArduinoOTA.end(); // Encerra qualquer OTA ativa via Wi-Fi
  delay(100);
  updateFromFS(SD, "/base.bin");
  vTaskDelete(NULL);
}

// Inicia a task OTA
void base() {
  Serial.println("🧠 Iniciando atualização via SD...");
  xTaskCreatePinnedToCore(
    startUpdateTask, "UpdateTask", 8192, NULL, 1, NULL, 0
  );
}

void setup() {
  Serial.begin(115200);
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);

  sdMutex = xSemaphoreCreateMutex();

  if (!SD.begin(PINO_SD)) {
    Serial.println("❌ Falha ao iniciar o cartão SD.");
  } else {
    Serial.println("✅ Cartão SD iniciado com sucesso.");
  }

  smartLadder.setup();  // Inicializa a SmartLadderEsp32

}

void loop() {
  // Verifica se o botão de reset foi pressionado
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    Serial.println("🔄 Botão de reset de fábrica pressionado!");
    delay(300);  // Debounce simples
    base();      // Inicia a task de atualização
    while (true) delay(1000); // Espera indefinidamente
  }

}
