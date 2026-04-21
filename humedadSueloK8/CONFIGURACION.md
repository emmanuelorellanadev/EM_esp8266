# Guía de variables — `config.example.h`

Este archivo explica cada parámetro del archivo de configuración `config.example.h`.  
Para usarlo, cópialo como `config.h` y ajusta los valores según tu hardware.

---

## Wi-Fi

| Variable | Valor ejemplo | Qué es |
|----------|--------------|--------|
| `WIFI_SSID` | `"TU_RED_WIFI"` | El nombre de tu red Wi-Fi (lo que ves al conectarte desde el celular). |
| `WIFI_PASS` | `"TU_CONTRASENA"` | La contraseña de esa red. |

---

## Pines GPIO

Los pines se identifican por su número **GPIO real**, no por la etiqueta impresa en la placa (D1, D5, etc.).

| Variable | GPIO | Pin NodeMCU V3 | Qué hace |
|----------|------|----------------|----------|
| `PIN_DO` | `14` | D5 | Entrada digital del sensor K8/C11. Devuelve HIGH/LOW (seco/húmedo). Solo se usa de forma informativa; el control del riego usa el pin analógico A0. |
| `PIN_RELAY` | `5` | D1 | Controla el relé que abre/cierra la electroválvula. `HIGH` = válvula **cerrada** (estado seguro al arranque); `LOW` = válvula **abierta**. |
| `PIN_LED` | `LED_BUILTIN` | Pin interno | LED azul integrado en el NodeMCU. Se enciende mientras el sistema está regando (active-low: `LOW` = encendido). |

---

## Calibración ADC

El sensor K8/C11 genera un voltaje analógico que el ESP8266 convierte a un número entero entre **0 y 1023**.  
Ese número varía según la humedad del suelo: a mayor humedad, menor valor ADC.

| Variable | Valor ejemplo | Qué es |
|----------|--------------|--------|
| `RAW_DRY` | `571` | Valor ADC leído cuando el sensor está completamente **seco** (en el aire). Representa **0 % de humedad**. |
| `RAW_WET` | `336` | Valor ADC cuando el sensor está **sumergido en agua**. Representa **100 % de humedad**. |

> ⚠️ **Estos valores son específicos de cada sensor.** Dos sensores idénticos pueden dar lecturas diferentes.  
> Siempre mide los tuyos con el Monitor Serie de Arduino y reemplaza estos números.

**Fórmula de conversión usada en el código:**

```
porcentaje = (RAW_DRY - lecturaActual) / (RAW_DRY - RAW_WET) × 100
```

**Procedimiento de calibración:**
1. Con el sensor en el **aire** (completamente seco): anota el valor `Raw` que aparece en el Monitor Serie → ponlo en `RAW_DRY`.
2. **Sumerge el sensor en agua**: anota el valor `Raw` → ponlo en `RAW_WET`.

---

## Umbrales de humedad

| Variable | Valor ejemplo | Qué hace |
|----------|--------------|---------|
| `ON_THRESHOLD_PERCENT` | `35` | Si la humedad cae **por debajo de 35 %**, el sistema activa el riego. Es el umbral de "suelo seco". |
| `OFF_THRESHOLD_PERCENT` | `45` | Umbral informativo: si la humedad supera 45 %, se muestra `HUMEDO` en la web y el Monitor Serie. **No detiene el riego** — el riego siempre se detiene por tiempo (`RELAY_ON_TIME_MS`), no por este umbral. |

---

## Tiempos del riego

| Variable | Valor ejemplo | Qué hace |
|----------|--------------|---------|
| `RELAY_ON_TIME_MS` | `5000` (5 s) | Tiempo que permanece **abierta la válvula** por cada ciclo de riego. Ajusta según tu caudal, tipo de suelo y necesidades de la planta. |
| `COOLDOWN_MS` | `15000` (15 s) | Tiempo mínimo de **espera obligatoria** entre dos riegos consecutivos. Permite que el agua se absorba y que el sensor registre el cambio antes de decidir si vuelve a regar. Sin esto, el sistema podría regar en bucle. |

---

## Lectura ADC — Estabilidad de señal

El ADC del ESP8266 es ruidoso. Para obtener lecturas más estables se toman varias muestras y se calcula el promedio.

| Variable | Valor ejemplo | Qué hace |
|----------|--------------|---------|
| `ANALOG_SAMPLES` | `20` | Cantidad de lecturas por medición. Más muestras = mayor estabilidad, pero mayor tiempo por medición. |
| `ANALOG_DELAY_MS` | `5` (5 ms) | Pausa entre muestra y muestra. Con los valores de ejemplo: **20 muestras × 5 ms = 100 ms** por medición. |

---

## Muestreo en segundo plano

| Variable | Valor ejemplo | Qué hace |
|----------|--------------|---------|
| `BACKGROUND_SAMPLE_MS` | `3000` (3 s) | Cada cuánto tiempo el `loop()` lee el sensor y actualiza el control del riego. Determina la frecuencia de actualización de los datos mostrados en la web. |
