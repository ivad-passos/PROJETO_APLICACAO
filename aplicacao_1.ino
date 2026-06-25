#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ── Configurações de rede e backend ──────────────────────────────────────────
#define WIFI_SSID     "SEU_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"
#define BACKEND_URL   "http://192.168.1.100:5000"   // IP da sua máquina na rede local

// ── Pinos ─────────────────────────────────────────────────────────────────────
#define LED_R      21
#define LED_G      19
#define LED_B      18
#define PIN_BOTAO  23
#define PIN_POT    35
#define PIN_LDR    34
#define PIN_DHT    15
#define PIN_BUZZER 27

#define TIPO_DHT DHT22

// ── Limites ───────────────────────────────────────────────────────────────────
#define LIMITE_ESCURO          2000
#define LIMITE_TEMPERATURA_ALTA 25

// ── Estado global ─────────────────────────────────────────────────────────────
DHT dht(PIN_DHT, TIPO_DHT);

bool lampadaLigada    = false;
bool sensorTempAtivo  = true;
bool detectorLuzAtivo = true;
bool buzzerHabilitado = true;

String corManual       = "";
unsigned long corManualAte = 0;

volatile bool botaoPressionado = false;
unsigned long ultimoBotao  = 0;
unsigned long ultimoEnvio  = 0;
unsigned long ultimoPrint  = 0;

#define INTERVALO_ENVIO_MS 5000   // envia dados ao backend a cada 5 s

// ── ISR do botão ──────────────────────────────────────────────────────────────
void IRAM_ATTR tratarBotao() {
  botaoPressionado = true;
}

// ── Controle do LED ──────────────────────────────────────────────────────────
void ligarCor(int r, int g, int b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}
void desligarLed() { ligarCor(0, 0, 0); }

void aplicarCorPeloPotenciometro(int valorPot) {
  if      (valorPot < 1365) ligarCor(255, 0, 0);
  else if (valorPot < 2730) ligarCor(255, 255, 0);
  else                      ligarCor(0, 0, 255);
}

void aplicarCorNome(String cor) {
  if      (cor == "vermelho") ligarCor(255, 0, 0);
  else if (cor == "amarelo")  ligarCor(255, 255, 0);
  else if (cor == "azul")     ligarCor(0, 0, 255);
  else aplicarCorPeloPotenciometro(analogRead(PIN_POT));
}

String identificarCor(int valorPot) {
  if      (valorPot < 1365) return "Vermelho";
  else if (valorPot < 2730) return "Amarelo";
  else                      return "Azul";
}

// ── Buzzer ───────────────────────────────────────────────────────────────────
void ligarBuzzer()   { if (buzzerHabilitado) tone(PIN_BUZZER, 1000); }
void desligarBuzzer(){ noTone(PIN_BUZZER); }

// ── Lógica de sensores ───────────────────────────────────────────────────────
bool ambienteEscuro(int valorLdr) {
  return !detectorLuzAtivo || (valorLdr > LIMITE_ESCURO);
}

bool temperaturaPerigosa(float temp) {
  return sensorTempAtivo && (temp < 0 || temp > LIMITE_TEMPERATURA_ALTA);
}

// ── Processa comando recebido do backend ─────────────────────────────────────
void processarComando(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd == "") return;

  Serial.print("[CMD] ");
  Serial.println(cmd);

  if      (cmd == "ligar")            lampadaLigada = true;
  else if (cmd == "desligar")       { lampadaLigada = false; desligarLed(); desligarBuzzer(); }
  else if (cmd == "vermelho" || cmd == "amarelo" || cmd == "azul") {
    corManual    = cmd;
    corManualAte = millis() + 1000;
  }
  else if (cmd == "ativar_temp")     sensorTempAtivo  = true;
  else if (cmd == "desativar_temp")  sensorTempAtivo  = false;
  else if (cmd == "ativar_ldr")      detectorLuzAtivo = true;
  else if (cmd == "desativar_ldr")   detectorLuzAtivo = false;
  else if (cmd == "ativar_buzzer")   buzzerHabilitado = true;
  else if (cmd == "desativar_buzzer"){ buzzerHabilitado = false; desligarBuzzer(); }
  else Serial.println("[CMD] Desconhecido.");
}

// ── Envia leituras para o backend e recebe comando pendente ──────────────────
void enviarDados(float temp, int ldr, int pot, String status) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(BACKEND_URL "/dados");
  http.addHeader("Content-Type", "application/json");

  String cor = identificarCor(pot);
  String json = "{";
  json += "\"temperatura\":"   + String(temp, 1)           + ",";
  json += "\"luminosidade\":"  + String(ldr)               + ",";
  json += "\"lampada_ligada\":" + (lampadaLigada ? "true" : "false") + ",";
  json += "\"status_sistema\":\"" + status                 + "\",";
  json += "\"cor_ativa\":\""      + cor                    + "\"";
  json += "}";

  int httpCode = http.POST(json);
  if (httpCode == 200) {
    String resposta = http.getString();
    // Verifica se há comando pendente na resposta
    int idx = resposta.indexOf("\"comando\":\"");
    if (idx != -1) {
      int inicio = idx + 11;
      int fim    = resposta.indexOf("\"", inicio);
      if (fim != -1) {
        String cmd = resposta.substring(inicio, fim);
        processarComando(cmd);
      }
    }
  } else {
    Serial.print("[HTTP] Erro: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  pinMode(LED_R,      OUTPUT);
  pinMode(LED_G,      OUTPUT);
  pinMode(LED_B,      OUTPUT);
  pinMode(PIN_BOTAO,  INPUT_PULLUP);
  pinMode(PIN_POT,    INPUT);
  pinMode(PIN_LDR,    INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  desligarLed();
  desligarBuzzer();

  attachInterrupt(digitalPinToInterrupt(PIN_BOTAO), tratarBotao, FALLING);

  Serial.print("[WiFi] Conectando a ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
  Serial.println("[BACKEND] " BACKEND_URL);
  Serial.println("====================================");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Botão físico (debounce por software)
  if (botaoPressionado && millis() - ultimoBotao > 300) {
    ultimoBotao    = millis();
    botaoPressionado = false;
    lampadaLigada  = !lampadaLigada;
    Serial.println(lampadaLigada ? "[BTN] LIGADO" : "[BTN] DESLIGADO");
    if (!lampadaLigada) { desligarLed(); desligarBuzzer(); }
  }

  int   valorPot   = analogRead(PIN_POT);
  int   valorLdr   = analogRead(PIN_LDR);
  float leituraTemp = dht.readTemperature();
  bool  erroTemp   = isnan(leituraTemp);
  if (erroTemp) leituraTemp = 0;

  bool escuro    = ambienteEscuro(valorLdr);
  bool perigo    = !erroTemp && temperaturaPerigosa(leituraTemp);

  String statusSistema;

  if (!lampadaLigada) {
    desligarLed();
    desligarBuzzer();
    statusSistema = "DESLIGADO";
  } else if (perigo) {
    desligarLed();
    ligarBuzzer();
    statusSistema = "PERIGO TEMP | LED OFF | BUZZER ON";
  } else if (escuro) {
    desligarBuzzer();
    if (millis() < corManualAte && corManual != "") {
      aplicarCorNome(corManual);
      statusSistema = "NORMAL | COR CLOUD: " + corManual;
    } else {
      corManual = "";
      aplicarCorPeloPotenciometro(valorPot);
      statusSistema = "NORMAL | COR POT: " + identificarCor(valorPot);
    }
  } else {
    desligarLed();
    desligarBuzzer();
    statusSistema = "AMBIENTE CLARO | LED OFF";
  }

  // Monitor Serial a cada 1 s
  if (millis() - ultimoPrint >= 1000) {
    ultimoPrint = millis();
    Serial.println("------------------------------------");
    Serial.print("Lampada: ");  Serial.println(lampadaLigada ? "ON" : "OFF");
    Serial.print("Temp:    ");
    if (erroTemp) Serial.println("Erro DHT");
    else { Serial.print(leituraTemp); Serial.println(" C"); }
    if (perigo || (!erroTemp && (leituraTemp < 0 || leituraTemp > LIMITE_TEMPERATURA_ALTA)))
      Serial.println("*** Perigo! Desligar! ***");
    Serial.print("LDR:     "); Serial.println(valorLdr);
    Serial.print("Pot:     "); Serial.println(valorPot);
    Serial.print("Status:  "); Serial.println(statusSistema);
  }

  // Envia dados ao backend a cada INTERVALO_ENVIO_MS
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = millis();
    enviarDados(leituraTemp, valorLdr, valorPot, statusSistema);
  }
}
