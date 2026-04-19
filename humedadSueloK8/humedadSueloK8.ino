/*
  humedadSueloK8.ino
  ──────────────────────────────────────────────────────────────────
  Monitor de humedad de suelo con servidor web y riego temporizado.

  Hardware objetivo:
    • ESP8266 NodeMCU V3  (seleccionar "NodeMCU 1.0 (ESP-12E Module)")
    • Sensor analógico K8 / C11 (AO → A0, DO → D5/GPIO14)
    • Módulo relé 5 V active-high (GPIO LOW = inactivo, GPIO HIGH = activo)
    • Electroválvula 12 V DC (contactos NO/COM del relé + diodo flyback)

  Cómo usar:
    1. Copia humedadSueloK8/config.example.h → humedadSueloK8/config.h
    2. Edita config.h con tu SSID, contraseña y calibración.
    3. Compila y sube en Arduino IDE.

  Endpoints web:
    GET /       → página HTML con estado actual
    GET /json   → respuesta JSON para integración con otros sistemas
  ──────────────────────────────────────────────────────────────────
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "config.h"   // ← crea este archivo desde config.example.h

// ----------------------------------------------------------------
// Servidor web en el puerto 80
// ----------------------------------------------------------------
ESP8266WebServer server(80);

// ----------------------------------------------------------------
// Máquina de estados para el control del riego
// ----------------------------------------------------------------
enum RelayState {
  IDLE,       // esperando: sensor seco → transición a WATERING
  WATERING,   // válvula abierta durante RELAY_ON_TIME_MS
  COOLDOWN    // espera COOLDOWN_MS antes de permitir otro riego
};

RelayState    relayState      = IDLE;
unsigned long relayStartMs    = 0;   // marca de tiempo al iniciar riego
unsigned long cooldownStartMs = 0;   // marca de tiempo al iniciar cooldown
unsigned long lastWaterEndMs  = 0;   // marca de tiempo al terminar el último riego (0 = nunca)

// ----------------------------------------------------------------
// Últimos valores leídos (actualizados en segundo plano)
// ----------------------------------------------------------------
float         lastPercent  = 0.0f;
int           lastRaw      = 0;
unsigned long lastSampleMs = 0;

// ================================================================
// readADC()
// Lee ANALOG_SAMPLES muestras con ANALOG_DELAY_MS entre ellas
// y devuelve el promedio.  Esto reduce el ruido del ADC del ESP8266.
// ================================================================
int readADC() {
  long sum = 0;
  for (int i = 0; i < ANALOG_SAMPLES; i++) {
    sum += analogRead(A0);
    delay(ANALOG_DELAY_MS);
  }
  return (int)(sum / ANALOG_SAMPLES);
}

// ================================================================
// rawToPercent(raw)
// Convierte la lectura cruda del ADC a porcentaje de humedad.
//   RAW_DRY → 0 %,  RAW_WET → 100 %
// El resultado se recorta al rango [0, 100].
// ================================================================
float rawToPercent(int raw) {
  float pct = (float)(RAW_DRY - raw) / (float)(RAW_DRY - RAW_WET) * 100.0f;
  if (pct < 0.0f)   pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

// ================================================================
// updateRelay(pct)
// Evalúa la máquina de estados y acciona el relé y el LED.
//
// Lógica del pin de relé (active-high):
//   GPIO LOW  → relé INACTIVO → válvula CERRADA  (estado seguro)
//   GPIO HIGH → relé ACTIVO   → válvula ABIERTA
//
// LED interno (active-low):
//   LOW  = LED encendido  (relé activo → regando)
//   HIGH = LED apagado    (relé inactivo)
// ================================================================
void updateRelay(float pct) {
  unsigned long now = millis();

  switch (relayState) {

    case IDLE:
      if (pct < ON_THRESHOLD_PERCENT) {
        // Suelo seco: abre la válvula
        digitalWrite(PIN_RELAY, HIGH);  // HIGH → relé activo → válvula abierta
        relayStartMs = now;
        relayState   = WATERING;
        Serial.printf("[RIEGO] Iniciado. Humedad: %.1f%%\n", pct);
      }
      break;

    case WATERING:
      if (now - relayStartMs >= RELAY_ON_TIME_MS) {
        // Tiempo de riego completado: cierra la válvula
        digitalWrite(PIN_RELAY, LOW);   // LOW → relé inactivo → válvula cerrada
        lastWaterEndMs  = now;
        cooldownStartMs = now;
        relayState      = COOLDOWN;
        Serial.println("[RIEGO] Terminado. Iniciando cooldown.");
      }
      break;

    case COOLDOWN:
      if (now - cooldownStartMs >= COOLDOWN_MS) {
        relayState = IDLE;
        Serial.println("[RIEGO] Cooldown finalizado. Listo para siguiente ciclo.");
      }
      break;
  }

  // LED: encendido solo cuando el relé está activo (regando)
  digitalWrite(PIN_LED, (relayState == WATERING) ? LOW : HIGH);
}

// ================================================================
// Handlers del servidor web
// ================================================================

// GET /  → página HTML
void handleRoot() {
  String estado;
  if      (relayState == WATERING) estado = "REGANDO";
  else if (relayState == COOLDOWN) estado = "COOLDOWN";
  else if (lastPercent < ON_THRESHOLD_PERCENT) estado = "SECO";
  else    estado = "HUMEDO";

  String ultimoRiego;
  if (lastWaterEndMs == 0) {
    ultimoRiego = "Sin riego registrado";
  } else {
    unsigned long segs = (millis() - lastWaterEndMs) / 1000UL;
    if (segs < 60) {
      ultimoRiego = String(segs) + " seg";
    } else if (segs < 3600) {
      ultimoRiego = String(segs / 60) + " min " + String(segs % 60) + " seg";
    } else {
      ultimoRiego = String(segs / 3600) + " h " + String((segs % 3600) / 60) + " min";
    }
  }

  String html =
    "<!DOCTYPE html>"
    "<html lang='es'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Humedad Suelo</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem}"
    "h1{color:#2a7a2a}"
    ".card{background:#f4f4f4;border-radius:8px;padding:1rem;margin:.5rem 0}"
    ".val{font-size:2rem;font-weight:bold;color:#1a5c1a}"
    "</style>"
    "</head><body>"
    "<h1>🌱 Monitor de Humedad</h1>"
    "<div class='card'><div class='val'>" + String(lastPercent, 1) + " %</div>"
    "<div>Humedad del suelo</div></div>"
    "<div class='card'>ADC Raw: <b>" + String(lastRaw) + "</b></div>"
    "<div class='card'>Estado: <b>" + estado + "</b></div>"
    "<div class='card'>Último riego: <b>" + ultimoRiego + "</b></div>"
    "<p><a href='/json'>Ver JSON</a></p>"
    "</body></html>";

  server.send(200, "text/html; charset=UTF-8", html);
}

// GET /json  → respuesta JSON
void handleJson() {
  String estado;
  if      (relayState == WATERING) estado = "WATERING";
  else if (relayState == COOLDOWN) estado = "COOLDOWN";
  else if (lastPercent < ON_THRESHOLD_PERCENT) estado = "DRY";
  else    estado = "WET";

  long secsAgo = (lastWaterEndMs == 0) ? -1L : (long)((millis() - lastWaterEndMs) / 1000UL);

  String json = "{";
  json += "\"raw\":"              + String(lastRaw)                                + ",";
  json += "\"percent\":"          + String(lastPercent, 1)                         + ",";
  json += "\"watering\":"         + String(relayState == WATERING ? "true":"false") + ",";
  json += "\"cooldown\":"         + String(relayState == COOLDOWN ? "true":"false") + ",";
  json += "\"state\":\""          + estado                                         + "\",";
  json += "\"last_watered_sec\":" + String(secsAgo);
  json += "}";

  server.send(200, "application/json", json);
}

// ================================================================
// setup()
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[INICIO] humedadSueloK8");

  // Configurar pines
  pinMode(PIN_DO,    INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED,   OUTPUT);

  // Estado seguro al arranque: válvula cerrada, LED apagado
  digitalWrite(PIN_RELAY, LOW);   // LOW → relé inactivo → válvula cerrada
  digitalWrite(PIN_LED,   HIGH);  // active-low → LED apagado

  // Conectar a Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WIFI] Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WIFI] Conectado. IP: ");
  Serial.println(WiFi.localIP());

  // Registrar rutas del servidor web
  server.on("/",     handleRoot);
  server.on("/json", handleJson);
  server.begin();
  Serial.println("[WEB] Servidor iniciado en puerto 80");

  // Primera lectura
  lastRaw     = readADC();
  lastPercent = rawToPercent(lastRaw);
  lastSampleMs = millis();
  Serial.printf("[ADC] Raw: %d | Humedad: %.1f%%\n", lastRaw, lastPercent);
}

// ================================================================
// loop()
// ================================================================
void loop() {
  // Atender peticiones web
  server.handleClient();

  unsigned long now = millis();

  // Muestreo en segundo plano cada BACKGROUND_SAMPLE_MS
  if (now - lastSampleMs >= BACKGROUND_SAMPLE_MS) {
    lastSampleMs = now;
    lastRaw      = readADC();
    lastPercent  = rawToPercent(lastRaw);
    Serial.printf("[ADC] Raw: %d | Humedad: %.1f%%\n", lastRaw, lastPercent);
  }

  // Actualizar relé y LED según el estado actual
  updateRelay(lastPercent);
}
