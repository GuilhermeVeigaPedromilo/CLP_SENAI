#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>  // Biblioteca para processar JSON
#include <ModbusMaster.h>
#include <SD.h>
#include "FS.h"
#include <ArduinoOTA.h>


SemaphoreHandle_t sdMutex;

AsyncWebServer server(80);

#define PINO_SD 5  // Pino onde o SD está conectado
//Pino 19 VSPI_MISO
//Pino 18 VSPI_CLk (cartão de memória  SCK (Clock)
//Pino 23 SCL

#define RE_DE_PIN 0

// Pinos das saidas digitais
#define SAIDA1 32
#define SAIDA2 33
#define SAIDA3 25
#define SAIDA4 17
#define SAIDA5 16
#define SAIDA6 4

//factory
#define FACTORY_RESET_PIN 21
// Pinos das entradas analogicas
#define ANAL1 36
#define ANAL2 39
#define ANAL3 35
#define ANAL4 34
// Pinos das entradas digitais
#define DIGI2 26
#define DIGI3 27
#define DIGI4 22
#define DIGI5 14
#define DIGI6 13

byte soma = 0; // Variável que armazena os estados das saídas
String ultimoEstadoSaida1 = ""; // Armazena o último estado da saídas
String ultimoEstadoSaida2 = ""; // Armazena o último estado do json
// Variáveis de configuração
String mqttServer;
uint16_t mqttPort;
String clientID;
String topicANAL1;
String topicANAL2;
String topicANAL3;
String topicANAL4;
String topicDIGI2;
String topicDIGI3;
String topicDIGI4;
String topicDIGI5;
String topicDIGI6;
String topicsaida1;
String topicsaida2;
String topicsaida3;
String topicsaida4;
String topicsaida5;
String topicsaida6;
String topicsaida7;
String topicsaida8;
String topicsaida11;
String topicsaida12;
String topicsaida13;
String topicsaida14;
String topicsaida15;
String topicsaida16;
String topicsaida17;
String topicsaida18;
 // Flag para controlar o envio de dados
bool ContrDIGI2 = false;
bool ContrDIGI3 = false;
bool ContrDIGI4 = false;
bool ContrDIGI5 = false;
bool ContrDIGI6 = false;

Preferences preferences;
//modbus
HardwareSerial RS485(1);
struct Inversor {
  int id;
  int baud;
};
ModbusMaster node;

const char* mqtt_server = "192.168.0.199";
String my_payload;
String my_topic;

WiFiClient Cozinha_cafeteira;
PubSubClient client(Cozinha_cafeteira);



std::vector<ModbusMaster> nodes;
std::vector<Inversor> inversores;
int inversorAtual = 0;
unsigned long ultimoTempo = 0;

//carregar factory

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
//base
void startUpdateTask(void *parameter) {
  ArduinoOTA.end();
  delay(100);
  updateFromFS(SD,"/base.bin");
  vTaskDelete(NULL);
}
void base(){
  Serial.println("carregou o base");
  // Fechar todas as conexões antes da 
  delay(100);
  Serial.printf("Endereço de startUpdateTask: %p\n", (void *)startUpdateTask);
  xTaskCreatePinnedToCore(
    startUpdateTask, "UpdateTask", 8192, NULL, 1, NULL, 0);
}

void factory(){
  // Verifica se o botão de reset foi pressionado na inicialização
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {  // Pressionado (GND)
    Serial.println("🔄 Botão de reset de fábrica pressionado!");
  base();
  }
}// fim base// fim factory


void carregarInversores() {
  preferences.begin("inversores", true);
  String json = preferences.getString("json", "[]");
  preferences.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, json);

  inversores.clear();
  for (JsonObject obj : doc.as<JsonArray>()) {
    Inversor inv;
    inv.id = obj["idInversor"].as<int>();
    inv.baud = obj["baud"].as<int>();
    inversores.push_back(inv);
  }
}

void setupModbus() {
  // Configura o pino RE/DE para controle de direção
  pinMode(RE_DE_PIN, OUTPUT);
  digitalWrite(RE_DE_PIN, LOW);  // Modo Recepção
  if (inversores.empty()) return;
  
  Inversor inv = inversores[0];
  RS485.begin(inv.baud, SERIAL_8N1, 16, 17); // RX, TX

  node.begin(inv.id, RS485);
  Serial.printf("Inicializado inversor ID=%d, Baud=%d\n", inv.id, inv.baud);
}


//modbus




String gerarPagina() {
  preferences.begin("inversores", true);
  String jsonSalvo = preferences.getString("json", "[]");
  preferences.end();

  Serial.println("JSON carregado da NVS:");
  Serial.println(jsonSalvo);


  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonSalvo);



  String index_html = "";
  index_html = "  <!DOCTYPE html>";
  index_html += "  <html lang='pt-BR'>";
  index_html += "  <head>";
  index_html += "  <meta charset='UTF-8'>";
  index_html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  index_html += "  <title>Configurações MQTT</title>";
  index_html += "    <link rel=\"shortcut icon\" href=\"logo-senai-1.png\"/>";
  index_html += "  <link rel=\"stylesheet\" href=\"style.css\" />";
  index_html += "    <link rel=\"stylesheet\" href=\"mqtt.css\" />";

  index_html += "  </head>";
  index_html += "  <body>";
  index_html += "        <div id=\"header-placeholder\" data-basepath=\"../\"></div> ";
  index_html += "        <nav class=\"mqtt-page-navbar\"> ";
  index_html += "           <ul>";
  index_html += "             <li class=\"active\" onclick=\"openMainTab('portas', this)\">Portas</li>";
  index_html += "             <li onclick=\"openMainTab('inv', this)\">Inversores</li>";
  index_html += "           </ul>";
  index_html += "       </nav>";
  index_html += "      <main>";
  index_html += "     <section id=\"portas\" class=\"main-tab\">";
  index_html += "      <form  class=\"mqtt-page-form\" action='/setConfig' method='POST'>";
  index_html += "       <h2>Configurações Gerais</h2>";



  index_html += "        <label>Servidor: <input type='text' name='server' value='" + mqttServer + "'></label>";
  index_html += "        <label>Porta: <input type='number' name='port' value='" + String(mqttPort) + "'></label>";
  index_html += "        <label>Client ID: <input type='text' name='clientID' value='" + clientID + "'></label>";
  
  
  
  
  
  
  index_html += "        <h2>Configurações de Entradas e Saídas</h2>";
  index_html += "        <div class=\"mqtt-page-tabs\">";
  index_html += "        <button type='button' class='mqtt-page-tab-button' onclick=\"openTab('analogica')\">Entradas Analógicas</button>";
  index_html += "        <button type='button' class='mqtt-page-tab-button' onclick=\"openTab('digital')\">Entradas Digitais</button>";
  index_html += "        <button type='button' class='mqtt-page-tab-button' onclick=\"openTab('saidas')\">Saídas Digitais</button>";
  index_html += "         </div>";
  
  
  index_html += "        <div id='analogica' class='mqtt-page-tab'>";
  index_html += "          <h3>Entradas Analógicas</h3>";
  index_html += "          <label>Tópico Entrada Analógica 1: <input type='text' name='topicANAL1' value='" + topicANAL1 + "'></label>";
  index_html += "          <label>Tópico Entrada Analógica 2: <input type='text' name='topicANAL2' value='" + topicANAL2 + "'></label>";
  index_html += "          <label>Tópico Entrada Analógica 3: <input type='text' name='topicANAL3' value='" + topicANAL3 + "'></label>";
  index_html += "          <label>Tópico Entrada Analógica 4: <input type='text' name='topicANAL4' value='" + topicANAL4 + "'></label>";
  index_html += "        </div>";


  index_html += "        <div id='digital' class='mqtt-page-tab'>";
  index_html += "          <h3>Entradas Digitais</h3>";
  index_html += "          <label>Tópico Entrada Digital 2: <input type='text' name='topicDIGI2' value='" + topicDIGI2 + "'></label>";
  index_html += "          <label>Tópico Entrada Digital 3: <input type='text' name='topicDIGI3' value='" + topicDIGI3 + "'></label>";
  index_html += "          <label>Tópico Entrada Digital 4: <input type='text' name='topicDIGI4' value='" + topicDIGI4 + "'></label>";
  index_html += "          <label>Tópico Entrada Digital 5: <input type='text' name='topicDIGI5' value='" + topicDIGI5 + "'></label>";
  index_html += "          <label>Tópico Entrada Digital 6: <input type='text' name='topicDIGI6' value='" + topicDIGI6 + "'></label>";
  index_html += "        </div>";


index_html += "        <div id='saidas' class='mqtt-page-tab'>";
index_html += "          <h3>Saídas Digitais</h3>";

index_html += "          Saída 1: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida1' value='" + topicsaida1 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida11' value='" + topicsaida11 + "'></label><br>";

index_html += "          Saída 2: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida2' value='" + topicsaida2 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida12' value='" + topicsaida12 + "'></label><br>";

index_html += "          Saída 3: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida3' value='" + topicsaida3 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida13' value='" + topicsaida13 + "'></label><br>";

index_html += "          Saída 4: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida4' value='" + topicsaida4 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida14' value='" + topicsaida14 + "'></label><br>";

index_html += "          Saída 5: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida5' value='" + topicsaida5 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida15' value='" + topicsaida15 + "'></label><br>";

index_html += "          Saída 6: ";
index_html += "          <label>Ligado <input type='text' name='topicsaida6' value='" + topicsaida6 + "' style='margin-right:10px;'></label>";
index_html += "          <label>Desligado <input type='text' name='topicsaida16' value='" + topicsaida16 + "'></label><br>";

index_html += "        </div>";


  index_html += "       <input type=\"submit\" value=\"Salvar Configurações\" />";
  index_html += " <div style=\"position: fixed; bottom: 10px; right: 10px; font-size: 14px; color: #555;\">Trabalho de conclusão do curso de eletroeletrônica</div>";

  index_html += "       </form>";

  index_html += "     </section>";
  index_html += "   <section id=\"inv\" class=\"main-tab\" style=\"display: none;\">";

  index_html += "       <div  class=\"mqtt-page-inv\">";



  index_html +=       "<div id='jsonBox' class=\"mqtt-page-json-box\">";
  index_html +=       "<pre id='jsonTexto'>{\n";
  index_html += "  \"ref\": 1,              // Não alterar\n";
  index_html += "  \"id\": 5,               // Identificador do inversor\n";
  index_html += "  \"memoria\": 100,        // Memória a ser alterada do inversor\n";
  index_html += "  \"valor\": 1234          // Novo valor para alteração\n";
  index_html += "}</pre>";

  
  index_html += "<button onclick='copiarJSON()'>Copiar JSON</button>";
  index_html += "</div>";   // botao json


  index_html += "<h2>Configurar Inversores</h2>  <div id='inversores'>";

  for (JsonObject obj : doc.as<JsonArray>()) {
    String id = String((int)obj["idInversor"]);
    String baud = String((int)obj["baud"]);
    String topic = String((const char*)obj["topic"]);
    String parity = String((const char*)obj["parity"]);
    String stopBits = String((int)obj["stopBits"]);

  index_html += "<div class=\"mqtt-page-inv-actions\">";
  index_html += "ID: <input type='number' name='idInversor' value='" + id + "'>";
  index_html += "Baud rate: <input type='number' name='baud' value='" + baud + "'>";

    // Paridade
  index_html += "Paridade: <select name='parity'>";
  index_html += "<option value='None'" + (parity == "None" ? String(" selected") : "") + ">Nenhum (None)</option>";
  index_html += "<option value='Even'" + (parity == "Even" ? String(" selected") : "") + ">Par (Even)</option>";
  index_html += "<option value='Odd'" + (parity == "Odd" ? String(" selected") : "") + ">Ímpar (Odd)</option>";
  index_html += "</select>";

    // Bits de parada
     index_html += "Bits de parada: <select name='stopBits'>";
     index_html += "<option value='1'" + (stopBits == "1" ? String(" selected") : "") + ">1</option>";
     index_html += "<option value='2'" + (stopBits == "2" ? String(" selected") : "") + ">2</option>";
     index_html += "</select>";
    
     index_html += "Topic: <input type='text' name='topic' value='" + topic + "'>";  // Campo para o tópico
     index_html += "       <div class=\"mqtt-page-inv-actions\">";
     index_html += "<button onclick='addInversor()'>+</button>";
     index_html += "<button onclick='removeInversor(this)'>-</button>";
     index_html += "</div>";
     index_html += "</div>";
     index_html += "  </section>";
  }
  
  if (doc.size() == 0) {
     index_html += "<div>";
     index_html += "ID: <input type='number' name='idInversor'>";
     index_html += "Baud rate: <input type='number' name='baud' value='9600'>";
    
    // Paridade
     index_html += "Paridade: <select name='parity'>";
     index_html += "<option value='None'>Nenhum (None)</option>";
     index_html += "<option value='Even'>Par (Even)</option>";
     index_html += "<option value='Odd'>Ímpar (Odd)</option>";
     index_html += "</select>";
    
    // Bits de parada
     index_html += "Bits de parada: <select name='stopBits'>";
     index_html += "<option value='1'>1</option>";
     index_html += "<option value='2'>2</option>";
     index_html += "</select>";
    
     index_html += "Topic: <input type='text' name='topic'>";  // Campo para o tópico
    
     index_html += "<button onclick='addInversor()'>+</button>";
     index_html += "<button onclick='removeInversor(this)'>-</button>"; // <-- Adicionado botão de remover
     index_html += "</div>";
  }
  
   index_html += "</div class=\"mqtt-page-inv-actions\"><br><button onclick='salvar()'>Salvar</button>";




  index_html += " <div style=\"position: fixed; bottom: 10px; right: 10px; font-size: 14px; color: #555;\">Trabalho de conclusão do curso de eletroeletrônica</div>";

  index_html += "   </div>";

  index_html += "  <footer ></footer>";

  index_html += "<script src=\"consumir_header.js\" defer></script>";
  index_html += "<script>";

  //modbus
    index_html += "function toggleExemplo(){";
  index_html += "  let div = document.getElementById('jsonBox');";
  index_html += "  div.style.display = (div.style.display === 'none') ? 'block' : 'none';";
  index_html += "}";


  index_html += "function copiarJSON(){";
  index_html += "  const texto = document.getElementById('jsonTexto').innerText;";
  index_html += "  const temp = document.createElement('textarea');";
  index_html += "  temp.value = texto;";
  index_html += "  document.body.appendChild(temp);";
  index_html += "  temp.select();";
  index_html += "  document.execCommand('copy');";
  index_html += "  document.body.removeChild(temp);";
  index_html += "  alert('JSON copiado com sucesso!');";
  index_html += "}";//botao exemplo json



  index_html += "function addInversor(){";
  index_html += "let c = document.getElementById('inversores');";
  index_html += "let d = document.createElement('div');";
  index_html += "d.innerHTML = `ID: <input type='number' name='idInversor'> Baud rate: <input type='number' name='baud' value='9600'> Paridade: <select name='parity'><option value='None'>Nenhum (None)</option><option value='Even'>Par (Even)</option><option value='Odd'>Ímpar (Odd)</option></select> Bits de parada: <select name='stopBits'><option value='1'>1</option><option value='2'>2</option></select> Topic: <input type='text' name='topic'><button onclick='addInversor()'>+</button><button onclick='removeInversor(this)'>-</button>`;";
  index_html += "c.appendChild(d);}";
  index_html += "function removeInversor(btn){btn.parentElement.remove();}";
  index_html += "function salvar(){";
  index_html += "let lista=[];";
  index_html += "document.querySelectorAll('#inversores > div').forEach(d=>{";
  index_html += "let id=d.querySelector('input[name=\"idInversor\"]').value;";
  index_html += "let baud=d.querySelector('input[name=\"baud\"]').value;";
  index_html += "let parity=d.querySelector('select[name=\"parity\"]').value;";
  index_html += "let stopBits=d.querySelector('select[name=\"stopBits\"]').value;";
  index_html += "let topic=d.querySelector('input[name=\"topic\"]').value;";  // Coletando o valor do tópico
  index_html += "if(id)lista.push({idInversor:parseInt(id),baud:parseInt(baud),parity:parity,stopBits:parseInt(stopBits),topic:topic});});";
  index_html += "fetch('/salvar',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(lista)}).then(r=>{";
  index_html += "if(r.ok)alert('Salvo!');else alert('Erro');});}";
  //modbus



  index_html += "function openMainTab(tabId, element) {";
  index_html += "  document.querySelectorAll('.main-tab').forEach(tab => tab.style.display = 'none');";
  index_html += "  document.getElementById(tabId).style.display = 'block';";
  index_html += "  document.querySelectorAll('.mqtt-page-navbar li').forEach(li => li.classList.remove('active'));";
  index_html += "  element.classList.add('active');";
  index_html += "}";

  index_html += "function openTab(tabId, element) {";
  index_html += "  document.querySelectorAll('.mqtt-page-tab').forEach(tab => tab.style.display = 'none');";
  index_html += "  document.getElementById(tabId).style.display = 'block';";
  index_html += "  document.querySelectorAll('.mqtt-page-tab-button').forEach(btn => btn.classList.remove('active'));";
  index_html += "  element.classList.add('active');";
  index_html += "}";

  index_html += "document.addEventListener('DOMContentLoaded', () => {";
  index_html += "  document.querySelector(\".mqtt-page-navbar li.active\").click();";
  index_html += "  setTimeout(() => {";
  index_html += "    const analogicaBtn = document.querySelector(\"button[onclick*='analogica']\");";
  index_html += "    if (analogicaBtn) analogicaBtn.click();";
   // Seção de inversores: adiciona pelo menos um se estiver vazia
   index_html += "  const invSection = document.getElementById(\"inversores\");";
   index_html += "  if (invSection && invSection.children.length === 0) {";
   index_html += "    addInversor();";
   index_html += "  }";
  index_html += "  }, 100);";
  index_html += "});";

  index_html += "</script>";
  index_html += "   </body>";
  index_html += "   </html>";
  return index_html;
}

bool primeiraMensagem = true;


void callback(char* topic, byte* payload, unsigned int length) {

  if (primeiraMensagem) {
    primeiraMensagem = false;
    Serial.println("Primeira mensagem ignorada.");
    return;
  }


  String conc_payload = "";
  for (int i = 0; i < length; i++) {
    conc_payload += ((char)payload[i]);
  }
  my_payload = conc_payload;
  my_topic = topic;
}
//conexao Mqtt
void reconnect() {
  while (!client.connected()) {
    factory();
    if (client.connect(clientID.c_str())) {
      client.subscribe(clientID.c_str());
      Serial.println("Conectado com ClientID: " + clientID);
    }else{
      Serial.print(".");
    }
  }
}
//Ddeclaraçao de variavel normal
void setup() {
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP); // Se o botão fecha para GND, usar pull-up
  Serial.begin(115200);
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

  pinMode(ANAL1, INPUT);
  pinMode(ANAL2, INPUT);
  pinMode(ANAL3, INPUT);
  pinMode(ANAL4, INPUT);

  pinMode(DIGI2, INPUT_PULLDOWN);
  pinMode(DIGI3, INPUT_PULLDOWN);
  pinMode(DIGI4, INPUT_PULLDOWN);
  pinMode(DIGI5, INPUT_PULLDOWN);
  pinMode(DIGI6, INPUT_PULLDOWN);

  // Configura os pinos como saída
  pinMode(SAIDA1, OUTPUT);
  pinMode(SAIDA2, OUTPUT);
  pinMode(SAIDA3, OUTPUT);
  pinMode(SAIDA4, OUTPUT);
  pinMode(SAIDA5, OUTPUT);
  pinMode(SAIDA6, OUTPUT);

  // Inicializa todas as saídas como desligadas
  digitalWrite(SAIDA1, LOW);
  digitalWrite(SAIDA2, LOW);
  digitalWrite(SAIDA3, LOW);
  digitalWrite(SAIDA4, LOW);
  digitalWrite(SAIDA5, LOW);
  digitalWrite(SAIDA6, LOW);

  // Recupera configurações salvas
  preferences.begin("config", false);
  clientID = preferences.getString("mqttClientID", "ESP32_Default");
  mqttServer = preferences.getString("server", "IP>endereço"); // Valor padrão
  mqttPort = preferences.getUInt("port", 1883); // Valor padrão
  topicANAL1 = preferences.getString("topicANAL1", "analog1");
  topicANAL2 = preferences.getString("topicANAL2", "analog2");
  topicANAL3 = preferences.getString("topicANAL3", "analog3");
  topicANAL4 = preferences.getString("topicANAL4", "analog4");
  topicDIGI2 = preferences.getString("topicDIGI2", "digital2");
  topicDIGI3 = preferences.getString("topicDIGI3", "digital3");
  topicDIGI4 = preferences.getString("topicDIGI4", "digital4");
  topicDIGI5 = preferences.getString("topicDIGI5", "digital5");
  topicDIGI6 = preferences.getString("topicDIGI6", "digital6");
  topicsaida1 = preferences.getString("topicsaida1", "Ligado1");
  topicsaida2 = preferences.getString("topicsaida2", "Ligado2");
  topicsaida3 = preferences.getString("topicsaida3", "Ligado3");
  topicsaida4 = preferences.getString("topicsaida4", "Ligado4");
  topicsaida5 = preferences.getString("topicsaida5", "Ligado5");
  topicsaida6 = preferences.getString("topicsaida6", "Ligado6");
  topicsaida7 = preferences.getString("topicsaida7", "Ligado7");
  topicsaida8 = preferences.getString("topicsaida8", "Ligado8");
  topicsaida11 = preferences.getString("topicsaida11", "Desligado1");
  topicsaida12 = preferences.getString("topicsaida12", "Desligado2");
  topicsaida13 = preferences.getString("topicsaida13", "Desligado3");
  topicsaida14 = preferences.getString("topicsaida14", "Desligado4");
  topicsaida15 = preferences.getString("topicsaida15", "Desligado5");
  topicsaida16 = preferences.getString("topicsaida16", "Desligado6");
  topicsaida17 = preferences.getString("topicsaida17", "Desligado7");
  topicsaida18 = preferences.getString("topicsaida18", "Desligado8");

  preferences.end();

  server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest * request) {

  request->send(200,  "text/html", gerarPagina());
  });

   // Rota para salvar configurações
  server.on("/setConfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("server", true) &&
        request->hasParam("port", true) &&
        request->hasParam("clientID", true) &&
        request->hasParam("topicANAL1", true) &&
        request->hasParam("topicANAL2", true) &&
        request->hasParam("topicANAL3", true) &&
        request->hasParam("topicANAL4", true) &&
        request->hasParam("topicDIGI2", true) &&
        request->hasParam("topicDIGI3", true) &&
        request->hasParam("topicDIGI4", true) &&
        request->hasParam("topicDIGI5", true) &&
        request->hasParam("topicDIGI6", true) &&
        request->hasParam("topicsaida1", true) &&
        request->hasParam("topicsaida2", true) &&
        request->hasParam("topicsaida3", true) &&
        request->hasParam("topicsaida4", true) &&
        request->hasParam("topicsaida5", true) &&
        request->hasParam("topicsaida6", true) &&
        request->hasParam("topicsaida11", true) &&
        request->hasParam("topicsaida12", true) &&
        request->hasParam("topicsaida13", true) &&
        request->hasParam("topicsaida14", true) &&
        request->hasParam("topicsaida15", true) &&
        request->hasParam("topicsaida16", true)) {

      mqttServer = request->getParam("server", true)->value();
      mqttPort = request->getParam("port", true)->value().toInt();
      clientID = request->getParam("clientID", true)->value();
      topicANAL1 = request->getParam("topicANAL1", true)->value();
      topicANAL2 = request->getParam("topicANAL2", true)->value();
      topicANAL3 = request->getParam("topicANAL3", true)->value();
      topicANAL4 = request->getParam("topicANAL4", true)->value();
      topicDIGI2 = request->getParam("topicDIGI2", true)->value();
      topicDIGI3 = request->getParam("topicDIGI3", true)->value();
      topicDIGI4 = request->getParam("topicDIGI4", true)->value();
      topicDIGI5 = request->getParam("topicDIGI5", true)->value();
      topicDIGI6 = request->getParam("topicDIGI6", true)->value();
      topicsaida1 = request->getParam("topicsaida1", true)->value();
      topicsaida2 = request->getParam("topicsaida2", true)->value();
      topicsaida3 = request->getParam("topicsaida3", true)->value();
      topicsaida4 = request->getParam("topicsaida4", true)->value();
      topicsaida5 = request->getParam("topicsaida5", true)->value();
      topicsaida6 = request->getParam("topicsaida6", true)->value();
      topicsaida11 = request->getParam("topicsaida11", true)->value();
      topicsaida12 = request->getParam("topicsaida12", true)->value();
      topicsaida13 = request->getParam("topicsaida13", true)->value();
      topicsaida14 = request->getParam("topicsaida14", true)->value();
      topicsaida15 = request->getParam("topicsaida15", true)->value();
      topicsaida16 = request->getParam("topicsaida16", true)->value();


      // Salvar na memória flash
      preferences.begin("config", false);
      preferences.putString("server", mqttServer);
      preferences.putUInt("port", mqttPort);
      preferences.putString("mqttClientID", clientID);
      preferences.putString("topicANAL1", topicANAL1);
      preferences.putString("topicANAL2", topicANAL2);
      preferences.putString("topicANAL3", topicANAL3);
      preferences.putString("topicANAL4", topicANAL4);
      preferences.putString("topicDIGI2", topicDIGI2);
      preferences.putString("topicDIGI3", topicDIGI3);
      preferences.putString("topicDIGI4", topicDIGI4);
      preferences.putString("topicDIGI5", topicDIGI5);
      preferences.putString("topicDIGI6", topicDIGI6);
      preferences.putString("topicsaida1", topicsaida1);
      preferences.putString("topicsaida2", topicsaida2);
      preferences.putString("topicsaida3", topicsaida3);
      preferences.putString("topicsaida4", topicsaida4);
      preferences.putString("topicsaida5", topicsaida5);
      preferences.putString("topicsaida6", topicsaida6);
      preferences.putString("topicsaida11", topicsaida11);
      preferences.putString("topicsaida12", topicsaida12);
      preferences.putString("topicsaida13", topicsaida13);
      preferences.putString("topicsaida14", topicsaida14);
      preferences.putString("topicsaida15", topicsaida15);
      preferences.putString("topicsaida16", topicsaida16);

      preferences.end();

      request->send(200, "text/html",gerarPagina());
      ESP.restart();
    } else {
        for (int i = 0; i < request->params(); i++) {
      const AsyncWebParameter* p = request->getParam(i);
      Serial.println(p->name() + ": " + p->value());
    }

      request->send(400, "text/html", "Erro: Faltam parâmetros.");
    }
  });
  //server.serveStatic("/", SD, "/");
  server.begin();
  // Configurar MQTT
  client.setServer(mqttServer.c_str(), mqttPort);
  client.setCallback(callback);
    //carregar inversores
  carregarInversores();
  setupModbus();
  sdMutex = xSemaphoreCreateMutex();

    if (!SD.begin(PINO_SD)) {
        Serial.println("Falha ao iniciar o cartão SD");
    } else {
        Serial.println("Cartão SD iniciado com sucesso");
    }

  server.serveStatic("/style.css", SD, "/style.css");
  server.serveStatic("/mqtt.css", SD, "/mqtt.css");
  server.serveStatic("/logo-senai-1.png", SD, "/logo-senai-1.png");
  server.serveStatic("/consumir_header.js", SD, "/consumir_header.js");
  server.serveStatic("/header.html", SD, "/header.html");
  server.serveStatic("/vars.css", SD, "/vars.css");

  //fim carregar inversores
    //salvar json inversores
  server.on("/salvar", HTTP_POST, [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      String json = "";
      for (size_t i = 0; i < len; i++) json += (char)data[i];
      preferences.begin("inversores", false);
      preferences.putString("json", json);
      preferences.end();
      req->send(200, "text/plain", "OK");
    });

}
//inversor
void interpretarComando(String& my_payload) {

  String erroinversores = "";
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, my_payload);

  int ref = doc["ref"];
  int id = doc["id"];
  int reg = doc["memoria"];
  int valor = doc["valor"];
  // Verifica se os campos obrigatórios estão presentes
  if (id == 0 || !doc.containsKey("memoria")) {
    client.publish("alerta", "❌ Dados incompletos no comando");
    my_payload = "";
    return;
  }
  if(ref == 1){
  // Buscar o inversor correspondente
  for (Inversor& inv : inversores) {
    if (inv.id == id) {
      RS485.updateBaudRate(inv.baud);
      node.begin(inv.id, RS485);
      digitalWrite(RE_DE_PIN, HIGH);
      uint8_t result = node.writeSingleRegister(reg, valor);
      if (result == node.ku8MBSuccess) {
        Serial.println("✅ Escrita com sucesso!");
      } else {
        String erroinversores = " ❌ Erro na leitura do registrador. Código: " + String(result) + "\n";
        client.publish("alerta", erroinversores.c_str());
      }
      my_payload = "";
      return;
    }
  }}
  erroinversores = "ID " + String(id) + " não encontrado entre os inversores cadastrados\n";
  client.publish("alerta", erroinversores.c_str());
  my_payload = "";
}

//ler inversor
void lerEpublicarValor(String& my_payload) {
  
  String erroinversores = "";
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, my_payload);

  int ref = doc["ref"];
  int id = doc["id"];
  int reg = doc["memoria"];
  const char* topics = doc["topics"];
  Serial.printf("Ref: %d | ID: %d | Memória: %d | Tópico: %s\n", ref, id, reg, topics);
  // Verifica se os campos obrigatórios estão presentes
  if (id == 0 || reg == 0 || topics) {
    client.publish("alerta", "❌ Dados incompletos no comando");
    my_payload = "";
    return;
  }
  if(ref == 1){
  // Buscar o inversor correspondente
  for (Inversor& inv : inversores) {
    if (inv.id == id) {
      // Configura a comunicação RS485
      RS485.updateBaudRate(inv.baud);
      node.begin(inv.id, RS485);
      digitalWrite(RE_DE_PIN, LOW);  // Modo Recepção (LEITURA)

      // Realiza a leitura do registrador
      uint8_t result = node.readHoldingRegisters(reg, 1);
      if (result == node.ku8MBSuccess) {
        uint16_t valorLido = node.getResponseBuffer(0);  // Pega o valor do registrador
        // Publica o valor no tópico MQTT
        char payload[50];
        snprintf(payload, sizeof(payload), "%d", valorLido);  // Converte o valor para string
        client.publish(topics, payload);  // Publica no Node-RED
      } else {
          String erroinversores = " ❌ Erro na leitura do registrador. Código: " + String(result) + "\n";
          client.publish("alerta", erroinversores.c_str());
      }
      my_payload = "";
      return;
    }
  }
  erroinversores = "ID " + String(id) + " não encontrado entre os inversores cadastrados\n";
  client.publish("alerta", erroinversores.c_str());
  my_payload = "";

  }
}
void jsonreconhecer(){
   // Se não for JSON, trata como comando simples (ex: Saida)
  if (!my_payload.startsWith("{")) {
    return;
  }

  // Se for JSON, tenta analisar
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, my_payload);
  if (err) {
    client.publish("alerta", "❌ JSON inválido");
    my_payload = "";
    return;
  }

  // Agora decide qual função chamar, com base no campo "ref"
  if (!doc.containsKey("ref")) {
    client.publish("alerta", "❌ JSON sem campo 'ref'");
    my_payload = "";
    return;
  }

  int ref = doc["ref"];

  if (ref == 1 && doc.containsKey("valor")) {
    interpretarComando(my_payload);  // Escrita
  } else if (ref == 1 && doc.containsKey("topics")) {
    lerEpublicarValor(my_payload);   // Leitura
  } else {
    client.publish("alerta", "❌ JSON com estrutura desconhecida");
    my_payload = "";
  }
}
//inversor

void LED_conexao(){
  if(WiFi.status() != WL_CONNECTED){
    digitalWrite(2, LOW);   // Ativa saídas com valor correto 
  }else{
    digitalWrite(2,HIGH);
  }

}

void Saida(){

  if (my_payload == topicsaida1 && ultimoEstadoSaida1 != topicsaida1) {
    ultimoEstadoSaida1 = topicsaida1;
    digitalWrite(SAIDA2, HIGH);
    client.publish(topicsaida1.c_str(), "Saida 1 ligada");
  }
  if (my_payload == topicsaida11 && ultimoEstadoSaida1 != topicsaida11) {
    ultimoEstadoSaida1 = topicsaida11;
    digitalWrite(SAIDA1, LOW);
    client.publish(topicsaida11.c_str(), "Saida 1 desligada");
  }
  if (my_payload == topicsaida2 && ultimoEstadoSaida1 != topicsaida2) {
    ultimoEstadoSaida1 = topicsaida2;
    digitalWrite(SAIDA2, HIGH);
    client.publish(topicsaida2.c_str(), "Saida 2 ligada");
  }
  if (my_payload == topicsaida12 && ultimoEstadoSaida1 != topicsaida12) {
    ultimoEstadoSaida1 = topicsaida12;
    digitalWrite(SAIDA2, LOW);
    client.publish(topicsaida12.c_str(), "Saida 2 desligada");
  }
  if (my_payload == topicsaida3 && ultimoEstadoSaida1 != topicsaida3) {
    ultimoEstadoSaida1 = topicsaida3;
    digitalWrite(SAIDA3, HIGH);
    client.publish(topicsaida3.c_str(), "Saida 3 ligada");
  }
  if (my_payload == topicsaida13 && ultimoEstadoSaida1 != topicsaida13) {
    ultimoEstadoSaida1 = topicsaida13;
    digitalWrite(SAIDA3, LOW);
    client.publish(topicsaida13.c_str(), "Saida 3 desligada");
  }
  if (my_payload == topicsaida4 && ultimoEstadoSaida1 != topicsaida4) {
    ultimoEstadoSaida1 = topicsaida4;
    digitalWrite(SAIDA4, HIGH);
    client.publish(topicsaida4.c_str(), "Saida 4 ligada");
  }
  if (my_payload == topicsaida14 && ultimoEstadoSaida1 != topicsaida14) {
    ultimoEstadoSaida1 = topicsaida14;
    digitalWrite(SAIDA4, LOW);
    client.publish(topicsaida14.c_str(), "Saida 4 desligada");
  }
  if (my_payload == topicsaida5 && ultimoEstadoSaida1 != topicsaida5) {
    ultimoEstadoSaida1 = topicsaida5;
    digitalWrite(SAIDA5, HIGH);
    client.publish(topicsaida5.c_str(), "Saida 5 ligada");
  }
  if (my_payload == topicsaida15 && ultimoEstadoSaida1 != topicsaida15) {
    ultimoEstadoSaida1 = topicsaida15;
    digitalWrite(SAIDA5, LOW);
    client.publish(topicsaida15.c_str(), "Saida 5 desligada");
  }
  if (my_payload == topicsaida6 && ultimoEstadoSaida1 != topicsaida6) {
    ultimoEstadoSaida1 = topicsaida6;
    digitalWrite(SAIDA6, HIGH);
    client.publish(topicsaida6.c_str(), "Saida 6 ligada");
  }
  if (my_payload == topicsaida16 && ultimoEstadoSaida1 != topicsaida16) {
    digitalWrite(SAIDA6, LOW);
    client.publish(topicsaida16.c_str(), "Saida 6 desligada");
  }

}

//funçao analogica vai ate controle analogico

// Variáveis para armazenar as leituras anteriores e atuais
int ultimaLeitura1 = 0;
int ultimaLeitura2 = 0;
int ultimaLeitura3 = 0;
int ultimaLeitura4 = 0;

// Função para fazer a leitura e média
int lerMediaAnalogica(int pino, int quantidadeLeituras) {
  long soma = 0;
  for (int i = 0; i < quantidadeLeituras; i++) {
    soma += analogRead(pino);
  }
  int media = soma / quantidadeLeituras;
  return media;
}

void  ControleAnalogico(){
   //Realizando a leitura do pino ANAL4 e calculando a média
   int leituraAnalogica1 = lerMediaAnalogica(ANAL1, 10);  // Média de 10 leituras
   int leituraAnalogica2 = lerMediaAnalogica(ANAL2, 10);  // Média de 10 leituras
   int leituraAnalogica3 = lerMediaAnalogica(ANAL3, 10);  // Média de 10 leituras
   int leituraAnalogica4 = lerMediaAnalogica(ANAL4, 10);  // Média de 10 leituras
  
  // Verificando se a leitura atual é diferente da última
  if (abs(abs(leituraAnalogica1 - ultimaLeitura1)) > 30) {
     // Atualizando a última leitura
    ultimaLeitura1 = leituraAnalogica1;
    String leiAnalogica1 = String(leituraAnalogica1);
    // Publicando no MQTT apenas se o valor foi alterado
    client.publish(topicANAL1.c_str(), leiAnalogica1.c_str());  // Publicando no tópico MQTT
  }
  

  if (abs(leituraAnalogica2 - ultimaLeitura2)> 30) {
     // Atualizando a última leitura
    ultimaLeitura2 = leituraAnalogica2;
    String leiAnalogica2 = String(leituraAnalogica2);
    // Publicando no MQTT apenas se o valor foi alterado
    client.publish(topicANAL2.c_str(), leiAnalogica2.c_str());  // Publicando no tópico MQTT
  }

  if (abs(leituraAnalogica3 - ultimaLeitura3)> 30) {
     // Atualizando a última leitura
    ultimaLeitura3 = leituraAnalogica3;
    String leiAnalogica3 = String(leituraAnalogica3);
    // Publicando no MQTT apenas se o valor foi alterado
    client.publish(topicANAL3.c_str(), leiAnalogica3.c_str());  // Publicando no tópico MQTT
  }


  if (abs(leituraAnalogica4 - ultimaLeitura4) > 30) {
     // Atualizando a última leitura
    ultimaLeitura4 = leituraAnalogica4;
    String leiAnalogica4 = String(leituraAnalogica4);
    // Publicando no MQTT apenas se o valor foi alterado
    client.publish(topicANAL4.c_str(), leiAnalogica4.c_str());  // Publicando no tópico MQTT
  }
}

void  ControleDigital(){
  if (digitalRead(DIGI2) == HIGH && ContrDIGI2 == false) {
  ContrDIGI2 = true;
  client.publish(topicDIGI2.c_str(), "1");

  }else if (digitalRead(DIGI2) == LOW && ContrDIGI2 == true){
  ContrDIGI2 = false;  // Resetando a flag quando o botão for solto
  client.publish(topicDIGI2.c_str(), "0");
  }

  if (digitalRead(DIGI3) == HIGH  && ContrDIGI3 == false) {
  ContrDIGI3 = true;
  client.publish(topicDIGI3.c_str(), "1");
  }else if (digitalRead(DIGI3) == LOW && ContrDIGI3 == true) {
    ContrDIGI3 = false;  // Resetando a flag quando o botão for solto
    client.publish(topicDIGI3.c_str(), "0");
  }

  if (digitalRead(DIGI4) == HIGH  && ContrDIGI4 == false) {
  ContrDIGI4 = true;
  client.publish(topicDIGI4.c_str(), "1");
  }else if (digitalRead(DIGI4) == LOW && ContrDIGI4 == true) {
    ContrDIGI4 = false;  // Resetando a flag quando o botão for solto
    client.publish(topicDIGI4.c_str(), "0");
  }

  if (digitalRead(DIGI5) == HIGH  && ContrDIGI5 == false) {
  ContrDIGI5 = true;
  client.publish(topicDIGI5.c_str(), "1");
  }else if (digitalRead(DIGI5) == LOW && ContrDIGI5 == true) {
    ContrDIGI5 = false;  // Resetando a flag quando o botão for solto
    client.publish(topicDIGI5.c_str(), "0");
  }

  if (digitalRead(DIGI6) == HIGH  && ContrDIGI6 == false) {
  ContrDIGI6 = true;
  client.publish(topicDIGI6.c_str(), "1");
  }else if (digitalRead(DIGI6) == LOW && ContrDIGI6 == true) {
    ContrDIGI6 = false;  // Resetando a flag quando o botão for solto
    client.publish(topicDIGI6.c_str(), "0");
  }
}

static int ultimaHeap = 0;
int heapAtual = ESP.getFreeHeap();

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  LED_conexao();
  ControleAnalogico();
  ControleDigital();
  Saida();
  factory();
  //jsonreconhecer();

  my_payload = "";
  my_topic = "";
}