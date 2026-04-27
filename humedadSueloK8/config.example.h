#pragma once
/*
  config.example.h — Plantilla de configuración para humedadSueloK8

  INSTRUCCIONES:
    1. Copia este archivo: cp config.example.h config.h
    2. Edita config.h con tu SSID y contraseña real.
    3. NUNCA subas config.h al repositorio (ya está en .gitignore).

  Todos los parámetros están comentados para que sepas qué ajustar
  sin necesidad de tocar el .ino principal.
*/

// ================================================================
// Wi-Fi
// ================================================================
#define WIFI_SSID "TU_RED_WIFI"      // Nombre de tu red (SSID)
#define WIFI_PASS "TU_CONTRASENA"    // Contraseña de tu red

// ================================================================
// Pines (numeración GPIO real, no la etiqueta D1/D5 de la placa)
// ================================================================

// Salida digital del sensor K8/C11:
//   D5 en NodeMCU V3 = GPIO 14
//   Solo se usa para lectura informativa; el control real viene del AO.
#define PIN_DO 14

// Pin de control del relé:
//   D1 en NodeMCU V3 = GPIO 5
//   Lógica active-low directa:
//     HIGH → relé INACTIVO → válvula CERRADA (estado seguro al arranque)
//     LOW  → relé ACTIVO   → válvula ABIERTA
#define PIN_RELAY 5

// LED interno del NodeMCU (active-low: LOW = encendido).
#define PIN_LED LED_BUILTIN

// ================================================================
// Calibración ADC  ← AJUSTA ESTO SEGÚN TU SENSOR
// ================================================================
// Procedimiento:
//   1. Con el sensor en aire/seco: lee el valor Raw en el Monitor Serie.
//      Escríbelo en RAW_DRY.
//   2. Sumerge el sensor en agua: lee el valor Raw.
//      Escríbelo en RAW_WET.
// Valores de ejemplo medidos con un K8/C11 genérico.
// ⚠ DEBES reemplazar estos valores con los tuyos propios mediante el
//   procedimiento de calibración descrito en README.md (sección 4).
//   Usar valores incorrectos genera lecturas erróneas y riegos indeseados.
#define RAW_DRY 571   // ADC cuando el sensor está completamente seco
#define RAW_WET 336   // ADC cuando el sensor está en agua

// ================================================================
// Umbrales de humedad  (porcentaje 0–100 %)
// ================================================================

// Si percent < ON_THRESHOLD_PERCENT → se considera "SECO" → activa riego
#define ON_THRESHOLD_PERCENT  35

// Umbral superior para mostrar estado "HUMEDO" en web/serial
// (no controla el relé directamente; el riego es siempre por tiempo)
#define OFF_THRESHOLD_PERCENT 45

// ================================================================
// Tiempos del riego
// ================================================================

// Tiempo que permanece abierta la electroválvula por ciclo (ms).
// ⚠ Ajusta según el caudal de tu válvula, tipo de suelo y necesidades de la planta.
//   Ejemplo: 5 s con goteo lento puede ser insuficiente; 30 s puede ser excesivo.
#define RELAY_ON_TIME_MS   5000UL   // 5 segundos (valor inicial de ejemplo)

// Tiempo mínimo entre dos riegos consecutivos (ms)
// Evita que el sistema riegue en bucle cuando el suelo tarda en absorber.
#define COOLDOWN_MS       15000UL   // 15 segundos

// ================================================================
// Lectura ADC — estabilidad de señal
// ================================================================

// Número de muestras que se promedian en cada lectura
#define ANALOG_SAMPLES   20

// Retardo entre muestras consecutivas (ms) — reduce el ruido ADC
#define ANALOG_DELAY_MS   5

// ================================================================
// Muestreo en segundo plano
// ================================================================

// Cada cuántos ms se lee el sensor y se actualiza el control
// cuando no hay petición web activa
#define BACKGROUND_SAMPLE_MS  3000UL   // 3 segundos

// ================================================================
// MQTT  — publicación y recepción de datos vía broker
// ================================================================
// Instala la librería "PubSubClient" de Nick O'Leary desde el
// Library Manager de Arduino IDE antes de compilar.
//
// MQTT (Message Queuing Telemetry Transport) es un protocolo de
// mensajería ligero basado en el patrón publicar/suscribir (pub/sub).
// Funciona sobre TCP/IP con un servidor central llamado "broker"
// (p. ej. Mosquitto corriendo en la Raspberry Pi).
//
// Ventajas de MQTT para IoT:
//   • Muy bajo consumo de ancho de banda (cabecera mínima de 2 bytes)
//   • Tolerante a redes inestables (reconexión automática)
//   • Desacopla productores y consumidores: el ESP8266 publica datos
//     sin saber quién los lee; la Raspberry Pi los recibe sin saber
//     desde cuántos dispositivos provienen.
//
// Flujo de este firmware:
//   PUBLICACIÓN  → el ESP8266 envía lecturas de sensor a MQTT_TOPICO
//                  cada BACKGROUND_SAMPLE_MS.
//                  Formato: {"raw":0,"percent":0.0,"state":"WET",
//                            "watering":false,"cooldown":false}
//
//   SUSCRIPCIÓN  → el ESP8266 escucha comandos en MQTT_TOPICO_CMD.
//                  La Raspberry Pi publica {"action":"water"} para
//                  activar el riego de forma remota desde el dashboard.
//
// Si no quieres usar MQTT, deja MQTT_SERVER vacío ("") y el firmware
// omitirá toda la lógica MQTT sin afectar el servidor web ni el riego.
//
// BROKER    : servidor central (p. ej. Mosquitto en la Raspberry Pi)
// TÓPICO    : cadena jerárquica que identifica un canal de mensajes
// QoS       : calidad de servicio (0 = fire-and-forget, 1 = al-menos-una-vez)
// CLIENT_ID : identifica de forma única a este dispositivo ante el broker;
//             si dos clientes usan el mismo ID, el broker desconecta al anterior.

// IP del broker (Raspberry Pi). Dejar "" para deshabilitar MQTT completamente.
// ⚠ Cambia esta IP por la dirección real de tu Raspberry Pi en la red local.
#define MQTT_SERVER      "192.168.1.2"         // IP de la Raspberry Pi (broker)
#define MQTT_PORT        1883                   // Puerto TCP estándar sin TLS
#define MQTT_CLIENT_ID   "esp8266-invernadero" // ID único por dispositivo

// Tópico de PUBLICACIÓN: el ESP8266 envía lecturas de sensor aquí.
// El servidor EM_server escucha "sensors/#" y almacena los datos.
#define MQTT_TOPICO      "sensors/esp8266"

// Tópico de SUSCRIPCIÓN: el ESP8266 escucha comandos aquí.
// La Raspberry Pi publica {"action":"water"} para activar el riego remoto.
#define MQTT_TOPICO_CMD  "commands/esp8266"

// Autenticación — dejar vacío si el broker no requiere usuario/contraseña
#define MQTT_USER        ""
#define MQTT_PASS_BROKER ""
