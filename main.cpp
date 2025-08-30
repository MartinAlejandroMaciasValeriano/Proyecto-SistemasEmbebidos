#include <Arduino.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"
#include <limits.h>

#include "apwifieeprommode.h"  // expone: server, intentoconexion(), loopAP(), detenerAP()
#include "paginacajon.h"       // UI web (HTML + JS)

#ifdef LED_BUILTIN
#define USE_BUILTIN_LED 1
#else
#define USE_BUILTIN_LED 0
#endif

#define SEC(x)  ((unsigned long)(x) * 1000UL)
#define MIN_(x) ((unsigned long)(x) * 60UL * 1000UL)

#define NUM_FILAS     3
#define NUM_COLUMNAS  4
#define NUM_CAJONES   (NUM_FILAS * NUM_COLUMNAS)

#define LEDS_POR_CAJON   15
#define CAJONES_POR_TIRA 4
#define LEDS_POR_TIRA    (LEDS_POR_CAJON * CAJONES_POR_TIRA)

#define DATA_PIN_1 15
#define DATA_PIN_2 16
#define DATA_PIN_3 4
#define DATA_PIN_4 5

#define BRILLO 10
#define REPETICIONES_PARPADEO 4
#define RETARDO_PARPADEO 250

// ===== Sleep / Wake por pulso HIGH =====
#define PIN_WAKE      GPIO_NUM_13       // botón a 3V3 → pulso HIGH
#define PIN_WAKE_ARD  13

// const unsigned long IDLE_SLEEP_MS = SEC(10);
// const unsigned long IDLE_SLEEP_MS = SEC(30);
// const unsigned long IDLE_SLEEP_MS = MIN_(1);
const unsigned long IDLE_SLEEP_MS = MIN_(2);

static unsigned long t_last_activity = 0;

volatile bool wakePulse = false;
void IRAM_ATTR onWakePulse() { wakePulse = true; }

// Pulldown en dominio digital y en RTC para evitar falsos HIGH
static void configWakePinActiveHigh() {
  // Dominio digital (mientras el ESP32 está despierto)
  pinMode(PIN_WAKE_ARD, INPUT_PULLDOWN);     // <-- ANTES era INPUT (flotaba)

  // Dominio RTC (en deep sleep)
  rtc_gpio_deinit(PIN_WAKE);
  rtc_gpio_init(PIN_WAKE);
  rtc_gpio_set_direction(PIN_WAKE, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en(PIN_WAKE);            // mantiene LOW estable en sueño
  rtc_gpio_pullup_dis(PIN_WAKE);
}

// ========= Firebase =========
const char* FB_DB_URL =
  "https://inventario-f73ee-default-rtdb.firebaseio.com/drawers.json"; // AJUSTA A TU PROYECTO
unsigned long t_last_sync = 0;
const unsigned long SYNC_EVERY_MS = 15000;

// ========= LEDs / estado =========
CRGB leds1[LEDS_POR_TIRA];
CRGB leds2[LEDS_POR_TIRA];
CRGB leds3[LEDS_POR_TIRA];
CRGB leds4[LEDS_POR_TIRA];
CRGB* tiras[] = { leds1, leds2, leds3, leds4 };

#define STATUS_MINALL 1
#define STATUS_MINPOS 2
#define STATUS_SUM    3
#ifndef LED_STATUS_MODE
#define LED_STATUS_MODE STATUS_MINALL
#endif

int componentes[NUM_CAJONES] = {0};      // arranque sin “heredar” valores

CRGB colorPorComponentes(int cantidad) {
  if (cantidad == 0) return CRGB::Red;
  else if (cantidad < 5) return CRGB::Yellow;
  else return CRGB::Green;
}

void apagarTodo() {
  for (int t = 0; t < 4; t++) fill_solid(tiras[t], LEDS_POR_TIRA, CRGB::Black);
  FastLED.show();
}

void mostrarEstadoInicial() {
  Serial.println("Mostrando estado (métrica por cajón → color)...");
  for (int fila = 0; fila < NUM_FILAS; fila++) {
    for (int col = 0; col < NUM_COLUMNAS; col++) {
      int cajon = fila * NUM_COLUMNAS + col;
      CRGB color = colorPorComponentes(componentes[cajon]);
      int inicio = col * LEDS_POR_CAJON;
      for (int i = 0; i < LEDS_POR_CAJON; i++) tiras[fila][inicio + i] = color;
    }
  }
  FastLED.show();
}

void iluminarCajon(int numero) {
  if (numero < 1 || numero > NUM_CAJONES) return;
  t_last_activity = millis();

#if USE_BUILTIN_LED
  for (int k = 0; k < 3; ++k) { digitalWrite(LED_BUILTIN, HIGH); delay(80); digitalWrite(LED_BUILTIN, LOW); delay(80); }
#endif

  int index = numero - 1;
  int fila = index / NUM_COLUMNAS;
  int col  = index % NUM_COLUMNAS;

  Serial.printf("Iluminando cajón %d (fila=%d, col=%d), valor=%d\n", numero, fila, col, componentes[index]);
  apagarTodo();

  if (componentes[index] == 0) {
    for (int r = 0; r < REPETICIONES_PARPADEO; r++) {
      for (int t = 0; t < 4; t++) fill_solid(tiras[t], LEDS_POR_TIRA, CRGB::Red);
      FastLED.show(); delay(RETARDO_PARPADEO);
      apagarTodo();   delay(RETARDO_PARPADEO);
    }
  } else {
    int inicio = col * LEDS_POR_CAJON;
    int tira1 = fila;
    int tira2 = (fila == 2) ? 3 : fila + 1;
    for (int r = 0; r < REPETICIONES_PARPADEO; r++) {
      for (int i = 0; i < LEDS_POR_CAJON; i++) {
        tiras[tira1][inicio + i] = CRGB::Blue;
        tiras[tira2][inicio + i] = CRGB::Blue;
      }
      FastLED.show(); delay(RETARDO_PARPADEO);
      for (int i = 0; i < LEDS_POR_CAJON; i++) {
        tiras[tira1][inicio + i] = CRGB::Black;
        tiras[tira2][inicio + i] = CRGB::Black;
      }
      FastLED.show(); delay(RETARDO_PARPADEO);
    }
  }

  mostrarEstadoInicial();
}

// ------- helpers métrica -------
static int toIntSafe(JsonVariantConst v) {
  if (v.is<int>())         return v.as<int>();
  if (v.is<long>())        return (int)v.as<long>();
  if (v.is<unsigned>())    return (int)v.as<unsigned>();
  if (v.is<double>())      return (int)v.as<double>();
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s) return 0;
    char* end = nullptr;
    long val = strtol(s, &end, 10);
    if (end != s) return (int)val;
  }
  return 0;
}
static int metric_sum(JsonObject comps) {
  int s = 0; for (JsonPair kv : comps) { int q = toIntSafe(kv.value()); if (q > 0) s += q; } return s;
}
static int metric_minpos(JsonObject comps) {
  int minpos = INT_MAX; bool found = false;
  for (JsonPair kv : comps) { int q = toIntSafe(kv.value()); if (q > 0) { found = true; if (q < minpos) minpos = q; } }
  return found ? minpos : 0;
}
static int metric_minall(JsonObject comps) {
  bool any = false; int m = INT_MAX;
  for (JsonPair kv : comps) { int q = toIntSafe(kv.value()); m = (q < m) ? q : m; any = true; }
  if (!any) return 0;
  return m < 0 ? 0 : m;
}

// ========= SYNC (acepta OBJETO o ARRAY) =========
void syncDesdeFirebaseYMostrar() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000);

  if (!http.begin(client, FB_DB_URL)) {
    Serial.println("[FB] http.begin() falló");
    return;
  }

  int code = http.GET();
  Serial.printf("[FB] GET /drawers => code=%d\n", code);

  if (code == 200) {
    String payload = http.getString();
    Serial.printf("[FB] payload len=%d\n", payload.length());
    Serial.println(payload.substring(0, 512));

    if (payload == "null" || payload.length() == 0) {
      Serial.println("[FB] /drawers sin datos o sin permiso de lectura.");
      http.end(); return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("[FB] Error JSON: "); Serial.println(err.c_str());
      http.end(); return;
    }

    bool updated[NUM_CAJONES] = {false};

    if (doc.is<JsonObject>()) {
      JsonObject drawers = doc.as<JsonObject>();
      for (JsonPair kvDrawer : drawers) {
        const char* key = kvDrawer.key().c_str();
        int drawerNum = atoi(key);
        if (drawerNum < 1 || drawerNum > NUM_CAJONES) { Serial.printf("[FB] Clave '%s' fuera de rango\n", key); continue; }
        int idx = drawerNum - 1;

        JsonVariant d = kvDrawer.value();
        JsonVariant compsVar = d["components"];
        if (compsVar.isNull() || !compsVar.is<JsonObject>()) {
          componentes[idx] = 0; updated[idx] = true;
          Serial.printf("[FB] Drawer %d sin 'components' → 0\n", drawerNum);
          continue;
        }
        JsonObject comps = compsVar.as<JsonObject>();

        int val = 0;
        #if (LED_STATUS_MODE == STATUS_SUM)
          val = metric_sum(comps);
        #elif (LED_STATUS_MODE == STATUS_MINPOS)
          val = metric_minpos(comps);
        #else
          val = metric_minall(comps);
        #endif

        componentes[idx] = val; updated[idx] = true;
        Serial.printf("[FB] Drawer %d VAL=%d\n", drawerNum, val);
      }

    } else if (doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      for (size_t i = 0; i < arr.size(); ++i) {
        if (i == 0) continue;                    // índice 0 suele ser null
        int drawerNum = (int)i;
        if (drawerNum < 1 || drawerNum > NUM_CAJONES) continue;
        int idx = drawerNum - 1;

        JsonVariant el = arr[i];
        if (!el.is<JsonObject>()) { componentes[idx] = 0; updated[idx] = true; continue; }

        JsonVariant compsVar = el["components"];
        if (compsVar.isNull() || !compsVar.is<JsonObject>()) {
          componentes[idx] = 0; updated[idx] = true; continue;
        }
        JsonObject comps = compsVar.as<JsonObject>();

        int val = 0;
        #if (LED_STATUS_MODE == STATUS_SUM)
          val = metric_sum(comps);
        #elif (LED_STATUS_MODE == STATUS_MINPOS)
          val = metric_minpos(comps);
        #else
          val = metric_minall(comps);
        #endif

        componentes[idx] = val; updated[idx] = true;
        Serial.printf("[FB] Drawer %d VAL=%d (array)\n", drawerNum, val);
      }

    } else {
      Serial.println("[FB] La respuesta no es objeto ni array JSON.");
    }

    for (int i = 0; i < NUM_CAJONES; ++i) {
      if (!updated[i]) {
        Serial.printf("[FB] Drawer %d no vino en JSON; mantiene=%d\n", i+1, componentes[i]);
      }
    }

    mostrarEstadoInicial();
  } else {
    Serial.printf("[FB] HTTP GET falló, code=%d\n", code);
  }

  http.end();
}

// ========= Webserver =========
inline void noteActivity() { t_last_activity = millis(); }

void iniciarServidorWebPrincipal() {
  server.on("/", [](){ noteActivity(); PaginaWeb::handlePagina(server); });

  server.on("/cajon", [](){ noteActivity(); PaginaWeb::handleCajon(server, iluminarCajon); });

  server.on("/api/cajon", [](){
    noteActivity();
    if (server.hasArg("numero")) {
      int n = server.arg("numero").toInt();
      Serial.printf("[API] Ubicar cajón %d\n", n);
      iluminarCajon(n);
      server.send(200, "application/json", String("{\"ok\":true,\"numero\":") + n + "}");
    } else {
      server.send(400, "application/json", "{\"ok\":false,\"err\":\"faltanumero\"}");
    }
  });

  server.onNotFound([](){ noteActivity(); PaginaWeb::handlePagina(server); });
  server.on("/favicon.ico", [](){ server.send(204); });
  server.on("/generate_204", [](){ server.send(204); });
  server.on("/fwlink", [](){ server.send(204); });

  server.begin();
  Serial.println("Servidor web principal iniciado correctamente");
}

void detenerAP() {
  Serial.println("Deteniendo portal AP y limpiando WebServer...");
  server.stop(); server.close(); delay(50);
  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); delay(50);
  Serial.println("Portal AP desactivado y WebServer limpio.");
}

// ========= Deep Sleep (pulso HIGH, ruido mitigado) =========
void goToDeepSleep() {
  Serial.println("→ Apagando LEDs y entrando en Deep Sleep por inactividad...");
  apagarTodo(); FastLED.clear(true);
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);

  // Asegura configuración estable del pin
  configWakePinActiveHigh();

  // Quita la ISR para que un rebote no “reinicie” el temporizador
  detachInterrupt(digitalPinToInterrupt(PIN_WAKE_ARD));

  // Evita dormirse si el botón está presionado (HIGH) ahora mismo
  while (digitalRead(PIN_WAKE_ARD) == HIGH) { delay(5); }
  delay(50);  // antirrebote

  // Wake por HIGH en GPIO13
  const uint64_t mask = 1ULL << PIN_WAKE_ARD;
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);

  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  configWakePinActiveHigh();
  attachInterrupt(digitalPinToInterrupt(PIN_WAKE_ARD), onWakePulse, RISING);

#if USE_BUILTIN_LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  // Log de causa de wake
  esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
  if (c == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t st = esp_sleep_get_ext1_wakeup_status();
    Serial.printf("Wake EXT1, mask=0x%016llx\n", st);
  } else if (c != ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.printf("Wake cause: %d\n", (int)c);
  }

  intentoconexion("ESPMartin", "244466666");

  Serial.println("=======================================");
  if (WiFi.status() == WL_CONNECTED) {
    detenerAP();
    Serial.println("ESP32 conectado correctamente");
    Serial.print("Acceder a : http://"); Serial.println(WiFi.localIP());
  } else {
    Serial.println("No se pudo conectar a WiFi. Portal AP activo.");
    Serial.println("Conéctate al AP y configura credenciales.");
  }
  Serial.println("=======================================");

  FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds1, LEDS_POR_TIRA).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds2, LEDS_POR_TIRA).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds3, LEDS_POR_TIRA).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, DATA_PIN_4, GRB>(leds4, LEDS_POR_TIRA).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRILLO);

  Serial.println("Verificando funcionamiento de tiras...");
  fill_solid(leds1, LEDS_POR_TIRA, CRGB::Blue);
  fill_solid(leds2, LEDS_POR_TIRA, CRGB::Green);
  fill_solid(leds3, LEDS_POR_TIRA, CRGB::Red);
  fill_solid(leds4, LEDS_POR_TIRA, CRGB::White);
  FastLED.show(); delay(1500); apagarTodo();

  mostrarEstadoInicial();
  iniciarServidorWebPrincipal();
  syncDesdeFirebaseYMostrar();

  t_last_activity = millis();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    if (millis() - t_last_sync > SYNC_EVERY_MS) {
      t_last_sync = millis();
      syncDesdeFirebaseYMostrar();
    }
  } else {
    loopAP();
  }

  if (wakePulse) {
    wakePulse = false;
    t_last_activity = millis();
    Serial.println("Pulso HIGH detectado (mantener despierto)");
  }

  if (millis() - t_last_activity > IDLE_SLEEP_MS) {
    goToDeepSleep();
  }
}
