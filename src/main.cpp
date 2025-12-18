#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ESPmDNS.h> 

// Constantes Globais
#define ONE_SECOND 1000

// Configurações do Hardware
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define RELE_PIN 18

// Configurações Wi-Fi
const char* ssid = "Connect+_Rodolfo_E_Telma";
const char* password = "12345678";
const char* hostname = "controle";

// Variáveis Globais
float tempAtual = 0.0;
float humAtual = 0.0;
float tempAlvo = 28.0;
bool fanStatus = false;
unsigned int lastMeasure = 0;

// Timer
bool timerAtivo = false;
unsigned int timerInicio = 0;
unsigned int timerDuracaoMS = 0;
int ultimoMinutoSalvo = 30;

const char* TEMP_FILE = "/config.txt";
const char* TIMER_FILE = "/timer.txt";

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// Utilitarios

void loadConfig() {
  if (LittleFS.exists(TEMP_FILE)) {
    File file = LittleFS.open(TEMP_FILE, "r");
    float tempLida = file.readString().toFloat();
    if (tempLida > 10 && tempLida < 50) tempAlvo = tempLida;
    file.close();
  }

  if (LittleFS.exists(TIMER_FILE)) {
    File file = LittleFS.open(TIMER_FILE, "r");
    int minLido = file.readString().toInt();
    if (minLido > 0) ultimoMinutoSalvo = minLido;
    file.close();
  }
}

void saveConfig(const char* path, String value) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Erro ao salvar configuração");
    return;
  }
  file.print(value);
  file.close();
  Serial.println("Configuração salva: " + String(value));
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  if (!LittleFS.exists(path)) return false;

  File file = LittleFS.open(path, "r");
  server.streamFile(file, getContentType(path));

  file.close();
  return true;
}

// Endpoints Handlers

void handleStatus() {
  int segundosRestantes = 0;
  if (timerAtivo) {
    int decorrido = millis() - timerInicio;
    if (decorrido < timerDuracaoMS) {
      segundosRestantes = (timerDuracaoMS - decorrido) / ONE_SECOND;
    }
    else {
      timerAtivo = false;
    }
  }

  String json = "{";
  json += "\"temperatura\":" + String(tempAtual);
  json += ",\"humidade\":" + String(humAtual);
  json += ",\"status\":" + String(fanStatus ? "true" : "false");
  json += ",\"temperaturaAlvo\":" + String(tempAlvo);
  json += ",\"timerAtivo\":" + String(timerAtivo ? "true" : "false");
  json += ",\"timerRestante\":" + String(segundosRestantes);
  json += ",\"ultimoTimer\":" + String(ultimoMinutoSalvo);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetTemp() {
  if (!server.hasArg("target")) {
    return server.send(400, "text/plain", "Falta argumento");
  }
  tempAlvo = server.arg("target").toFloat();
  saveConfig(TEMP_FILE, String(tempAlvo));
  server.send(200, "text/plain", "OK");
}

// Exemplo: /timer?cmd=start&min=60 ou /timer?cmd=stop
void handleSetTimer() {
  if (!server.hasArg("cmd")) {
    return server.send(400, "text/plain", "Falta argumento cmd");
  }

  String comando = server.hasArg("cmd") ? server.arg("cmd") : "start";
  
  if (comando == "stop") {
    timerAtivo = false;
    Serial.println("Timer cancelado");
    return server.send(200, "text/plain", "OK");
  }

  if (comando != "start" || !server.hasArg("min")) {
    return server.send(400, "text/plain", "Falta argumento minutos");
  }

  int minutos = server.arg("min").toInt();
  if (minutos > 0) {
    ultimoMinutoSalvo = minutos;
    saveConfig(TIMER_FILE, String(minutos));
  
    timerDuracaoMS = minutos * 60 * ONE_SECOND;
    timerInicio = millis();
    timerAtivo = true;
    Serial.println("Timer iniciado: " + String(minutos) + "min");
  }

  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  if (!LittleFS.exists("/notFound.html")) {
    return server.send(404, "text/plain", "404: Nao encontrado");
  }
  File file = LittleFS.open("/notFound.html", "r");
  server.streamFile(file, "text/html");
  file.close();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, LOW);

  dht.begin();

  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao iniciar LittleFS");
    return;
  }
  
  loadConfig();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  Serial.println(
    MDNS.begin(hostname)
    ? "Servidor iniciado! Acesse: http://" + String(hostname) + ".local"
    : "Erro ao configurar mDNS"
  );

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_GET, handleSetTemp);
  server.on("/timer", HTTP_GET, handleSetTimer);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) handleNotFound();
  });
  server.begin();
}

void loop() {
  server.handleClient();

  int tempoRestante = millis() - timerInicio;
  if (timerAtivo && (tempoRestante >= timerDuracaoMS)) {
    timerAtivo = false;
    Serial.println("Timer finalizado.");
  }

  if ((millis() - lastMeasure) < 2000) return;
  lastMeasure = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if (!isnan(t)) tempAtual = t;
  if (!isnan(h)) humAtual = h;

  bool deveLigar = false;

  if (tempAtual >= tempAlvo) deveLigar = true;
  else if (tempAtual > (tempAlvo - 0.5) && fanStatus) deveLigar = true;

  if (timerAtivo) deveLigar = true;

  // Aplica ao Relé
  if (deveLigar != fanStatus) {
    fanStatus = deveLigar;
    digitalWrite(RELE_PIN, fanStatus ? HIGH : LOW);
    Serial.println(fanStatus ? "Relé: ON" : "Relé: OFF");
  }
}