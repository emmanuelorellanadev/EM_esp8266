# EM_esp8266 — Monitor de Humedad con Riego Temporizado

Firmware para **ESP8266 NodeMCU V3** que:

- Lee la humedad del suelo con un sensor capacitivo **K8/C11** (salida analógica AO).
- Expone un **servidor web** (HTML + JSON) accesible desde la red local.
- Activa una **electroválvula 12 V DC** a través de un módulo relé 5 V cuando la humedad cae por debajo del umbral configurado, con enfriamiento para evitar riegos continuos.

---

## Tabla de contenidos

1. [Componentes y materiales](#1-componentes-y-materiales)
2. [Diagrama de cableado (ASCII)](#2-diagrama-de-cableado-ascii)
3. [Instalación y configuración](#3-instalación-y-configuración)
4. [Calibración del sensor (RAW_DRY / RAW_WET)](#4-calibración-del-sensor)
5. [Lógica de riego y parámetros](#5-lógica-de-riego-y-parámetros)
6. [Servidor web](#6-servidor-web)
7. [¿Qué es "active-low" y por qué usamos NPN?](#7-qué-es-active-low-y-por-qué-usamos-npn)
8. [Riesgos de compartir GND con fuentes separadas](#8-riesgos-de-compartir-gnd-con-fuentes-separadas)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Componentes y materiales

| Componente | Modelo / Especificación |
|---|---|
| Microcontrolador | ESP8266 NodeMCU V3 (ESP-12E) |
| Sensor de humedad | K8 / C11 (salida AO y DO) |
| Módulo relé | 5 V, active-low, 1 canal |
| Transistor NPN | 2N2222 o 2N3904 |
| Resistencia base | 1 kΩ |
| Resistencia pull-down | 100 kΩ (opcional, recomendada) |
| Electroválvula | 12 V DC, bobina inductiva |
| Diodo flyback | 1N4007 (en paralelo con la bobina) |
| Fuente principal | 5 V para NodeMCU (USB o regulador) |
| Fuente electroválvula | 12 V DC |

---

## 2. Diagrama de cableado (ASCII)

```
  ┌──────────────────────────────────────────────────────────────┐
  │                     ESP8266 NodeMCU V3                        │
  │                                                               │
  │  A0 ──────────────────────────── AO  (K8/C11 sensor)         │
  │  D5 (GPIO14) ─────────────────── DO  (K8/C11 sensor)         │
  │  3V3 ──────────────────────────── VCC (K8/C11 sensor)         │
  │  GND ──────────────────────────── GND (K8/C11 sensor)         │
  │                                                               │
  │  D1 (GPIO5) ──[1kΩ]── Base                                   │
  │                        │  2N2222 / 2N3904                     │
  │  GND ──────────────── Emisor                                  │
  │          [100kΩ]       │ (pull-down: base-emisor)              │
  │                       Colector ──── IN (módulo relé 5V)        │
  │                                                               │
  │  (GND del ESP y GND del módulo relé deben estar unidos)       │
  └──────────────────────────────────────────────────────────────┘

  Módulo relé 5V (active-low)
  ┌────────────────────────────┐
  │  VCC  ← 5V                 │
  │  GND  ← GND común           │
  │  IN   ← Colector NPN        │
  │                             │
  │  COM  ──────────── (+) 12V  │   ← fuente 12V
  │  NO   ──────────── Válvula  │
  │            Válvula ──── (-) 12V + diodo flyback │
  └────────────────────────────┘

  Diodo flyback (1N4007):
    Ánodo → terminal negativo de la bobina de la válvula
    Cátodo → terminal positivo de la bobina
    (suprime el pico inductivo al cortar el relé)
```

> **GND común:** conecta el GND de la fuente 5 V (NodeMCU) con el GND de la fuente 12 V.
> El GND del módulo relé debe estar en ese mismo nodo.

---

## 3. Instalación y configuración

### Requisitos

- **Arduino IDE 1.8+** (o 2.x)
- Soporte para ESP8266: en el Gestor de Placas agrega la URL  
  `http://arduino.esp8266.com/stable/package_esp8266com_index.json`  
  e instala el paquete **ESP8266 by ESP8266 Community**.
- Placa a seleccionar: **NodeMCU 1.0 (ESP-12E Module)**

### Pasos

```bash
# 1. Clona el repositorio
git clone https://github.com/emmanuelorellanadev/EM_esp8266.git
cd EM_esp8266/humedadSueloK8

# 2. Crea tu archivo de configuración local (nunca se sube al repo)
cp config.example.h config.h

# 3. Edita config.h con tu editor favorito
#    — Cambia WIFI_SSID y WIFI_PASS
#    — Ajusta RAW_DRY / RAW_WET con tus mediciones (ver sección 4)
```

4. Abre `humedadSueloK8/humedadSueloK8.ino` en Arduino IDE.  
5. Selecciona la placa, el puerto COM correcto y sube el firmware.  
6. Abre el Monitor Serie a **115200 baud** para ver la IP asignada.  
7. Navega a `http://<IP>/` en tu navegador.

---

## 4. Calibración del sensor

El sensor K8/C11 devuelve un valor ADC entre 0 y 1023.  
El firmware convierte ese valor a porcentaje con la fórmula:

```
porcentaje = (RAW_DRY - raw) / (RAW_DRY - RAW_WET) × 100
```

### Cómo obtener RAW_DRY y RAW_WET

1. **Seco:** deja el sensor en el aire (sin tocar tierra ni agua).  
   Lee el valor `Raw:` en el Monitor Serie → escríbelo en `RAW_DRY`.

2. **Mojado:** sumerge la parte metálica del sensor en agua limpia.  
   Lee el valor `Raw:` → escríbelo en `RAW_WET`.

> Los valores de ejemplo (`RAW_DRY 571`, `RAW_WET 336`) son solo una referencia.
> Cada sensor y cada placa pueden variar hasta ±50 unidades.

---

## 5. Lógica de riego y parámetros

El sistema usa una **máquina de 3 estados**:

```
  INACTIVO ──(porcentaje < ON_THRESHOLD)──► EN_RIEGO ──(RELAY_ON_TIME_MS)──► EN_ENFRIAMIENTO
      ▲                                                                              │
      └──────────────────────────(COOLDOWN_MS)──────────────────────────────────────┘
```

| Parámetro | Default | Descripción |
|---|---|---|
| `ON_THRESHOLD_PERCENT` | 35 % | Humedad mínima; si cae por debajo, riega |
| `RELAY_ON_TIME_MS` | 5 000 ms | Tiempo que la válvula permanece abierta |
| `COOLDOWN_MS` | 15 000 ms | Espera mínima entre dos riegos |
| `BACKGROUND_SAMPLE_MS` | 3 000 ms | Frecuencia de lectura en segundo plano |
| `ANALOG_SAMPLES` | 20 | Muestras promediadas por lectura |
| `ANALOG_DELAY_MS` | 5 ms | Retardo entre muestras (reduce ruido) |

**LED interno** (active-low): encendido cuando `porcentaje < ON_THRESHOLD_PERCENT`, apagado en caso contrario.

---

## 6. Servidor web

### `GET /`

Página HTML con:
- Porcentaje de humedad actual
- Valor ADC Raw
- Estado del sistema (SECO / HUMEDO / REGANDO / ENFRIAMIENTO)
- Enlace a `/json`

### `GET /json`

```json
{
  "raw": 480,
  "porcentaje": 30.2,
  "regando": false,
  "enfriamiento": false,
  "estado": "SECO"
}
```

Valores posibles de `estado`: `"SECO"`, `"HUMEDO"`, `"REGANDO"`, `"ENFRIAMIENTO"`.

---

## 7. ¿Qué es "active-low" y por qué usamos NPN?

Un relé **active-low** se activa cuando su pin `IN` recibe un nivel **BAJO (0 V)**.  
El ESP8266 opera a 3.3 V y sus pines no pueden suministrar suficiente corriente para activar directamente la bobina de un módulo relé de 5 V.

La solución es un **transistor NPN** como interruptor:

```
ESP GPIO (3.3V) ──[1kΩ]──► Base NPN
                              │ NPN ON  → Colector ≈ GND → Relé IN = LOW → Válvula ABIERTA
GND ─────────────────────── Emisor
```

- `GPIO HIGH (3.3 V)` → NPN saturado → Colector a GND → `Relé IN = LOW` → **válvula abierta**  
- `GPIO LOW (0 V)` → NPN corte → `Relé IN = HIGH` → **válvula cerrada**

La resistencia de **100 kΩ** entre base y emisor (pull-down) garantiza que el transistor permanezca apagado cuando el GPIO está en alta impedancia (p. ej., durante el boot del ESP8266).

---

## 8. Riesgos de compartir GND con fuentes separadas

Cuando se usan dos fuentes de alimentación (5 V para el ESP y 12 V para la válvula), **sus GND deben conectarse en un solo punto** para que las señales de control sean coherentes.

**Riesgos si NO se conectan:**
- El voltaje "GND" de una fuente puede flotar respecto a la otra, causando lecturas erróneas o daño al transistor.
- Se pueden crear bucles de tierra con corrientes de ruido que afectan al ADC.

**Buenas prácticas:**
- Une los GND en el punto más cercano posible al módulo relé.
- Usa un condensador de desacoplo (100 nF cerámico) entre VCC y GND del ESP si ves resets inesperados.
- El diodo flyback (1N4007) en la bobina de la válvula es **obligatorio** para absorber el pico de tensión inductiva al cortar el relé.

---

## 9. Troubleshooting

| Síntoma | Causa probable | Solución |
|---|---|---|
| ESP8266 se resetea al activar el relé | Pico de corriente o ruido en la línea de 3.3 V | Añadir capacitor 100 µF en la entrada de 3.3 V del ESP; verificar GND común |
| ADC lee valores muy inestables | Ruido en la línea A0 o interferencia del Wi-Fi | Aumentar `ANALOG_SAMPLES`; añadir capacitor 100 nF entre A0 y GND |
| El relé nunca activa | NPN no recibe señal de base | Verificar resistencia 1 kΩ, que GPIO5 esté configurado como OUTPUT, que GND sea común |
| El relé siempre activo al arrancar | GPIO5 flota durante boot del ESP | Añadir resistencia pull-down 100 kΩ en base del NPN |
| La válvula no cierra del todo | Corriente residual o relé defectuoso | Verificar que el relé use contactos NO/COM correctamente |
| Porcentaje fuera de rango (>100% o <0%) | RAW_DRY o RAW_WET mal calibrados | Recalibrar siguiendo la sección 4 |
| No aparece IP en Monitor Serie | Credenciales Wi-Fi incorrectas | Verificar SSID y PASS en config.h; reiniciar el ESP |

---

## Estructura del repositorio

```
EM_esp8266/
├── humedadSueloK8/
│   ├── humedadSueloK8.ino    ← Firmware principal
│   ├── config.example.h      ← Plantilla de configuración (versionar)
│   └── config.h              ← Tu configuración real (NO versionar, está en .gitignore)
├── .gitignore
├── LICENSE
└── README.md
```

---

## Licencia

MIT — ver [LICENSE](LICENSE).
