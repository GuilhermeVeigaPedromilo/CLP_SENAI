#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>

// Configurações do WiFi
const char* ssid = "Netvirtua";       // Substitua pelo nome da sua rede Wi-Fi
const char* password = "asd123!@#"; // Substitua pela senha da sua rede Wi-Fi

// Pino CS do cartão SD
#define SD_CS_PIN 2
// Cria um servidor web na porta 80
WebServer server(80);

void setup() {
  // Inicializa a comunicação serial para depuração
  Serial.begin(115200);

  // Inicializa o cartão SD
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Falha ao inicializar o cartão SD!");
    return;
  }
  Serial.println("Cartão SD inicializado com sucesso!");

  // Conecta-se à rede Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao Wi-Fi!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Define as rotas do servidor web
  server.on("/", handleRoot);              // Página inicial (lista arquivos)
  server.on("/upload", HTTP_POST, []() {}, handleFileUpload); // Upload de arquivo via drag-and-drop
  server.on("/read", handleReadFile);      // Ler arquivo
  server.on("/delete", handleDeleteFile);  // Apagar arquivo
  server.onNotFound(handleNotFound);       // Trata rotas desconhecidas

  // Inicia o servidor web
  server.begin();
  Serial.println("Servidor web iniciado!");
}

void loop() {
  // Manipula as requisições do servidor web
  server.handleClient();
}

// Rota principal: Lista os arquivos no cartão SD
void handleRoot() {
  String html = "<html><head><title>Gerenciador de Arquivos SD</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += ".drop-area { border: 2px dashed #ccc; padding: 20px; text-align: center; margin-bottom: 20px; }";
  html += ".file-list { list-style-type: none; padding: 0; }";
  html += ".file-item { margin: 10px 0; }";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>Gerenciador de Arquivos SD</h1>";

  // Área de upload com drag-and-drop
  html += "<div class='drop-area' id='dropArea'>";
  html += "<p>Arraste e solte arquivos aqui ou clique para selecionar</p>";
  html += "<input type='file' id='fileInput' multiple style='display: none;'>";
  html += "</div>";

  // Lista de arquivos
  html += "<h2>Arquivos no Cartão SD</h2>";
  html += "<ul class='file-list' id='fileList'>";

  File root = SD.open("/");
  if (!root) {
    html += "<li>Falha ao abrir diretório raiz.</li>";
  } else {
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;
      String fileName = entry.name();
      html += "<li class='file-item'>";
      html += fileName;
      html += " - <a href='/read?file=" + fileName + "' target='_blank'>Ler</a> | ";
      html += "<a href='/delete?file=" + fileName + "' onclick='return confirm(\"Tem certeza?\")'>Apagar</a>";
      html += "</li>";
      entry.close();
    }
    root.close();
  }

  html += "</ul>";

  // Script para drag-and-drop
  html += "<script>";
  html += "const dropArea = document.getElementById('dropArea');";
  html += "const fileInput = document.getElementById('fileInput');";
  html += "dropArea.addEventListener('click', () => fileInput.click());";
  html += "['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {";
  html += "  dropArea.addEventListener(eventName, preventDefaults, false);";
  html += "});";
  html += "function preventDefaults(e) {";
  html += "  e.preventDefault();";
  html += "  e.stopPropagation();";
  html += "}";
  html += "dropArea.addEventListener('drop', handleDrop, false);";
  html += "function handleDrop(e) {";
  html += "  const files = e.dataTransfer.files;";
  html += "  handleFiles(files);";
  html += "}";
  html += "fileInput.addEventListener('change', () => {";
  html += "  handleFiles(fileInput.files);";
  html += "});";
  html += "function handleFiles(files) {";
  html += "  [...files].forEach(uploadFile);";
  html += "}";
  html += "function uploadFile(file) {";
  html += "  const formData = new FormData();";
  html += "  formData.append('file', file);";
  html += "  fetch('/upload', { method: 'POST', body: formData })";
  html += "    .then(response => response.text())";
  html += "    .then(message => alert(message))";
  html += "    .catch(error => console.error(error));";
  html += "}";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Rota para upload de arquivo
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.printf("Iniciando upload: %s\n", filename.c_str());

    File file = SD.open(filename.c_str(), FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Erro ao abrir arquivo para escrita.");
      Serial.printf("Erro ao abrir arquivo: %s\n", filename.c_str());
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File file = SD.open(upload.filename.c_str(), FILE_APPEND);
    if (!file) {
      server.send(500, "text/plain", "Erro ao gravar arquivo.");
      Serial.printf("Erro ao gravar arquivo: %s\n", upload.filename.c_str());
      return;
    }
    file.write(upload.buf, upload.currentSize);
    file.close();
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("Upload concluído: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
    server.send(200, "text/plain", "Arquivo enviado com sucesso!");
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    server.send(500, "text/plain", "Upload cancelado.");
    Serial.printf("Upload cancelado: %s\n", upload.filename.c_str());
  }
}

// Rota para ler arquivo
void handleReadFile() {
  String fileName = server.arg("file");
  File file = SD.open(fileName.c_str());

  if (!file) {
    server.send(404, "text/plain", "Arquivo não encontrado.");
    Serial.printf("Arquivo não encontrado: %s\n", fileName.c_str());
    return;
  }

  String fileContent = "";
  while (file.available()) {
    fileContent += (char)file.read();
  }
  file.close();

  server.send(200, "text/plain", fileContent);
}

// Rota para apagar arquivo
void handleDeleteFile() {
  String fileName = server.arg("file");

  if (SD.remove(fileName.c_str())) {
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.printf("Arquivo apagado: %s\n", fileName.c_str());
  } else {
    server.send(500, "text/plain", "Erro ao apagar o arquivo.");
    Serial.printf("Erro ao apagar arquivo: %s\n", fileName.c_str());
  }
}

// Trata rotas desconhecidas
void handleNotFound() {
  server.send(404, "text/plain", "Página não encontrada.");
}