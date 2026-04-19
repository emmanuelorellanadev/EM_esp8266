/*
  humedadSueloK8.ino
  ──────────────────────────────────────────────────────────────────
  Monitor de humedad de suelo con servidor web y riego temporizado.

  Hardware objetivo:
    • ESP8266 NodeMCU V3  (seleccionar "NodeMCU 1.0 (ESP-12E Module)")
    • Sensor analógico K8 / C11 (AO → A0, DO → D5/GPIO14)
    • Módulo relé 5 V active-low (GPIO HIGH = inactivo, GPIO LOW = activo)
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
ESP8266WebServer servidor(80);

// ----------------------------------------------------------------
// Máquina de estados para el control del riego
// ----------------------------------------------------------------
enum EstadoRele {
  INACTIVO,          // esperando: sensor seco → transición a EN_RIEGO
  EN_RIEGO,          // válvula abierta durante RELAY_ON_TIME_MS
  EN_ENFRIAMIENTO    // espera COOLDOWN_MS antes de permitir otro riego
};

EstadoRele    estadoRele       = INACTIVO;
unsigned long inicioRiegoMs    = 0;   // marca de tiempo al iniciar riego
unsigned long inicioCooldownMs = 0;   // marca de tiempo al iniciar enfriamiento

// ----------------------------------------------------------------
// Últimos valores leídos (actualizados en segundo plano)
// ----------------------------------------------------------------
float         ultimoPorcentaje = 0.0f;
int           ultimoRaw        = 0;
unsigned long ultimoMuestreoMs = 0;

// ================================================================
// leerADC()
// Toma ANALOG_SAMPLES lecturas con ANALOG_DELAY_MS entre ellas
// y devuelve el promedio. Promediar reduce el ruido del ADC del
// ESP8266, que puede oscilar ±5 unidades por interferencia del Wi-Fi.
// ================================================================
int leerADC() {
  long suma = 0;
  for (int i = 0; i < ANALOG_SAMPLES; i++) {
    suma += analogRead(A0);
    delay(ANALOG_DELAY_MS);
  }
  return (int)(suma / ANALOG_SAMPLES);
}

// ================================================================
// rawAPorcentaje(raw)
// Convierte la lectura cruda del ADC a porcentaje de humedad usando
// los puntos de calibración definidos en config.h:
//   RAW_DRY → 0 %  (sensor en el aire)
//   RAW_WET → 100 % (sensor en agua)
// El resultado se recorta al rango [0, 100] para evitar valores
// fuera de escala si el sensor se coloca fuera del rango calibrado.
// ================================================================
float rawAPorcentaje(int raw) {
  float pct = (float)(RAW_DRY - raw) / (float)(RAW_DRY - RAW_WET) * 100.0f;
  if (pct < 0.0f)   pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

// ================================================================
// actualizarRele(pct)
// Evalúa la máquina de estados y acciona el relé y el LED
// según el porcentaje de humedad actual.
//
// Lógica del pin de relé (active-low directo):
//   GPIO HIGH → relé INACTIVO → válvula CERRADA  (estado seguro al arranque)
//   GPIO LOW  → relé ACTIVO   → válvula ABIERTA
//
// LED interno (active-low):
//   LOW  = LED encendido  (suelo seco, hay que regar)
//   HIGH = LED apagado    (suelo húmedo o en proceso de riego)
// ================================================================
void actualizarRele(float pct) {
  unsigned long ahora = millis();

  switch (estadoRele) {

    case INACTIVO:
      if (pct < ON_THRESHOLD_PERCENT) {
        // Suelo seco: abre la válvula e inicia el temporizador de riego
        digitalWrite(PIN_RELAY, LOW);   // LOW → relé activo → válvula abierta
        inicioRiegoMs = ahora;
        estadoRele    = EN_RIEGO;
        Serial.printf("[RIEGO] Iniciado. Humedad: %.1f%%\n", pct);
      }
      break;

    case EN_RIEGO:
      if (ahora - inicioRiegoMs >= RELAY_ON_TIME_MS) {
        // Tiempo de riego completado: cierra la válvula y entra en enfriamiento
        digitalWrite(PIN_RELAY, HIGH);  // HIGH → relé inactivo → válvula cerrada
        inicioCooldownMs = ahora;
        estadoRele       = EN_ENFRIAMIENTO;
        Serial.println("[RIEGO] Terminado. Iniciando enfriamiento.");
      }
      break;

    case EN_ENFRIAMIENTO:
      // Espera que el suelo absorba el agua antes de evaluar si hay que regar de nuevo
      if (ahora - inicioCooldownMs >= COOLDOWN_MS) {
        estadoRele = INACTIVO;
        Serial.println("[RIEGO] Enfriamiento finalizado. Listo para siguiente ciclo.");
      }
      break;
  }

  // LED: encendido solo cuando el suelo está seco (y no está regando)
  bool seco = (pct < ON_THRESHOLD_PERCENT);
  digitalWrite(PIN_LED, seco ? LOW : HIGH);
}

// ================================================================
// Manejadores del servidor web
// ================================================================

// GET /  → página HTML con estado visual del sensor y el riego
void manejarRaiz() {
  String estado;
  if      (estadoRele == EN_RIEGO)          estado = "REGANDO";
  else if (estadoRele == EN_ENFRIAMIENTO)   estado = "ENFRIAMIENTO";
  else if (ultimoPorcentaje < ON_THRESHOLD_PERCENT) estado = "SECO";
  else    estado = "HUMEDO";

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
    "<div class='card'><div class='val'>" + String(ultimoPorcentaje, 1) + " %</div>"
    "<div>Humedad del suelo</div></div>"
    "<div class='card'>ADC Raw: <b>" + String(ultimoRaw) + "</b></div>"
    "<div class='card'>Estado: <b>" + estado + "</b></div>"
    "<p><a href='/json'>Ver JSON</a></p>"
    "</body></html>";

  servidor.send(200, "text/html; charset=UTF-8", html);
}

// GET /json  → respuesta JSON (útil para integración con otros sistemas)
void manejarJson() {
  String estado;
  if      (estadoRele == EN_RIEGO)          estado = "REGANDO";
  else if (estadoRele == EN_ENFRIAMIENTO)   estado = "ENFRIAMIENTO";
  else if (ultimoPorcentaje < ON_THRESHOLD_PERCENT) estado = "SECO";
  else    estado = "HUMEDO";

  String json = "{";
  json += "\"raw\":"          + String(ultimoRaw)                                           + ",";
  json += "\"porcentaje\":"   + String(ultimoPorcentaje, 1)                                 + ",";
  json += "\"regando\":"      + String(estadoRele == EN_RIEGO        ? "true" : "false")    + ",";
  json += "\"enfriamiento\":" + String(estadoRele == EN_ENFRIAMIENTO ? "true" : "false")    + ",";
  json += "\"estado\":\""     + estado + "\"";
  json += "}";

  servidor.send(200, "application/json", json);
}

// ================================================================
// setup()
// Se ejecuta una sola vez al arrancar. Inicializa pines, Wi-Fi
// y el servidor web, y realiza la primera lectura del sensor.
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[INICIO] humedadSueloK8");

  // Configurar pines
  pinMode(PIN_DO,    INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED,   OUTPUT);

  // Estado seguro al arranque: válvula cerrada y LED apagado
  digitalWrite(PIN_RELAY, HIGH);  // HIGH → relé inactivo → válvula cerrada
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

  // Registrar rutas del servidor web y arrancarlo
  servidor.on("/",     manejarRaiz);
  servidor.on("/json", manejarJson);
  servidor.begin();
  Serial.println("[WEB] Servidor iniciado en puerto 80");

  // Primera lectura del sensor para tener datos disponibles de inmediato
  ultimoRaw        = leerADC();
  ultimoPorcentaje = rawAPorcentaje(ultimoRaw);
  ultimoMuestreoMs = millis();
  Serial.printf("[ADC] Raw: %d | Humedad: %.1f%%\n", ultimoRaw, ultimoPorcentaje);
}

// ================================================================
// loop()
// Se ejecuta continuamente. Atiende peticiones web, muestrea el
// sensor periódicamente y controla el relé.
// ================================================================
void loop() {
  // Atender peticiones del servidor web
  servidor.handleClient();

  unsigned long ahora = millis();

  // Muestreo periódico del sensor cada BACKGROUND_SAMPLE_MS milisegundos
  if (ahora - ultimoMuestreoMs >= BACKGROUND_SAMPLE_MS) {
    ultimoMuestreoMs = ahora;
    ultimoRaw        = leerADC();
    ultimoPorcentaje = rawAPorcentaje(ultimoRaw);
    Serial.printf("[ADC] Raw: %d | Humedad: %.1f%%\n", ultimoRaw, ultimoPorcentaje);
  }

  // Evaluar y actualizar el relé y el LED según el porcentaje actual
  actualizarRele(ultimoPorcentaje);
}
