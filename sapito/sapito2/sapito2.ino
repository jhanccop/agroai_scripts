#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HX711.h>
#include <ArduinoJson.h>

// === CONFIGURACIÓN ===
const char* WIFI_SSID = "sapito";
const char* WIFI_PASS = "12345678";
const char* MQTT_SERVER = "192.168.4.1";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "usuario";
const char* MQTT_PASS = "password";
const char* TOPIC_PUB = "esp32/datos";
const char* TOPIC_SUB = "esp32/config";

// === PINES ===
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define PIN_RES1 25
#define PIN_RES2 26
#define PIN_HUM 27
#define PIN_DESHUM 14
#define HX711_DOUT 32
#define HX711_SCK 33

// === INTERVALOS ===
const unsigned long INTERVALO_LECTURA = 1000;   // 1 segundo
const unsigned long INTERVALO_ENVIO = 1000;     // 1 segundo

// === OBJETOS ===
DHT dht(DHT_PIN, DHT_TYPE);
HX711 balanza;
WiFiClient espClient;
PubSubClient client(espClient);

// === ESTRUCTURA DE DATOS ===
struct {
  // Sensores
  float temperatura = 0;
  float humedad = 0;
  float peso = 0;
  
  // Estado de actuadores (estado REAL actual)
  bool res1 = false;
  bool res2 = false;
  bool hum = false;
  bool deshum = false;
  
  // Estado deseado en modo manual (comandos recibidos)
  bool res1Deseado = false;
  bool res2Deseado = false;
  bool humDeseado = false;
  bool deshumDeseado = false;
  
  // Control
  bool sistemaActivo = true;
  bool modoAuto = true;
  float tempSetpoint = 25.0;
  float humSetpoint = 60.0;
  float tempHisteresis = 1.0;
  float humHisteresis = 5.0;
  
  // Temporizadores
  unsigned long lastLectura = 0;
  unsigned long lastEnvio = 0;
} sys;

// === PROTOTIPOS ===
void conectarWiFi();
void conectarMQTT();
void procesarMensaje(char* topic, byte* payload, unsigned int length);
void leerSensores();
void controlAutomatico();
void controlManual();
void actualizarActuador(int pin, bool &estado, bool activar);
void apagarTodo();
void enviarEstado();

// ===========================================
// SETUP
// ===========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Control Temp/Humedad ===");
  
  // Configurar pines (Relays activos en LOW)
  pinMode(PIN_RES1, OUTPUT);
  pinMode(PIN_RES2, OUTPUT);
  pinMode(PIN_HUM, OUTPUT);
  pinMode(PIN_DESHUM, OUTPUT);
  apagarTodo();
  
  // Inicializar sensores
  dht.begin();
  balanza.begin(HX711_DOUT, HX711_SCK);
  balanza.set_scale(2280.f);
  balanza.tare();
  
  // Conectar red
  conectarWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(procesarMensaje);
  
  Serial.println("Sistema listo - Modo AUTOMÁTICO");
}

// ===========================================
// LOOP PRINCIPAL
// ===========================================
void loop() {
  if (!client.connected()) conectarMQTT();
  client.loop();
  
  unsigned long ahora = millis();
  
  // Leer sensores
  if (ahora - sys.lastLectura >= INTERVALO_LECTURA) {
    sys.lastLectura = ahora;
    leerSensores();
    
    if (sys.sistemaActivo) {
      if (sys.modoAuto) {
        controlAutomatico();
      } else {
        controlManual();
      }
    }
  }
  
  // Enviar estado por MQTT
  if (ahora - sys.lastEnvio >= INTERVALO_ENVIO) {
    sys.lastEnvio = ahora;
    enviarEstado();
  }
}

// ===========================================
// CONEXIÓN WIFI
// ===========================================
void conectarWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi OK - IP: " + WiFi.localIP().toString());
}

// ===========================================
// CONEXIÓN MQTT
// ===========================================
void conectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando MQTT...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("OK");
      client.subscribe(TOPIC_SUB);
    } else {
      Serial.printf("Error %d. Reintento en 5s\n", client.state());
      delay(5000);
    }
  }
}

// ===========================================
// PROCESAR MENSAJES MQTT
// ===========================================
void procesarMensaje(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.println("❌ Error JSON: " + String(error.c_str()));
    return;
  }
  
  Serial.println("\n📩 MQTT recibido:");
  serializeJsonPretty(doc, Serial);
  Serial.println();
  
  // Sistema ON/OFF
  if (doc.containsKey("sistemaActivo")) {
    sys.sistemaActivo = doc["sistemaActivo"];
    Serial.println(sys.sistemaActivo ? "✅ Sistema ACTIVADO" : "⏸️  Sistema DETENIDO");
    if (!sys.sistemaActivo) apagarTodo();
  }
  
  // Reset
  if (doc.containsKey("reset") && doc["reset"]) {
    Serial.println("🔄 RESET solicitado...");
    apagarTodo();
    delay(1000);
    ESP.restart();
  }
  
  if (!sys.sistemaActivo) return;
  
  // Modo AUTO/MANUAL
  if (doc.containsKey("autoMode")) {
    bool nuevoModo = doc["autoMode"];
    if (sys.modoAuto != nuevoModo) {
      sys.modoAuto = nuevoModo;
      Serial.println(sys.modoAuto ? "🤖 Modo AUTOMÁTICO activado" : "🎮 Modo MANUAL activado");
      apagarTodo();
      // Resetear estados deseados al cambiar a manual
      if (!sys.modoAuto) {
        sys.res1Deseado = false;
        sys.res2Deseado = false;
        sys.humDeseado = false;
        sys.deshumDeseado = false;
      }
    }
  }
  
  // Setpoints
  if (doc.containsKey("tempSetpoint")) {
    sys.tempSetpoint = doc["tempSetpoint"];
    Serial.printf("🌡️  TempSetpoint: %.1f°C\n", sys.tempSetpoint);
  }
  if (doc.containsKey("humSetpoint")) {
    sys.humSetpoint = doc["humSetpoint"];
    Serial.printf("💧 HumSetpoint: %.1f%%\n", sys.humSetpoint);
  }
  if (doc.containsKey("tempHisteresis")) sys.tempHisteresis = doc["tempHisteresis"];
  if (doc.containsKey("humHisteresis")) sys.humHisteresis = doc["humHisteresis"];
  
  // Control manual - SOLO aplicar si viene el comando explícito
  if (!sys.modoAuto) {
    Serial.println("🎮 Procesando comandos manuales...");
    
    if (doc.containsKey("res1")) {
      bool nuevoEstado = doc["res1"];
      sys.res1Deseado = nuevoEstado;
      Serial.printf("   RES1: Estado anterior=%d, Nuevo=%d\n", sys.res1, nuevoEstado);
      digitalWrite(PIN_RES1, nuevoEstado ? LOW : HIGH);
      sys.res1 = nuevoEstado;
      Serial.printf("   RES1: PIN %d = %s, Estado guardado=%d\n", 
                    PIN_RES1, nuevoEstado ? "LOW" : "HIGH", sys.res1);
    } else {
      Serial.printf("   RES1: No hay comando, manteniendo estado=%d\n", sys.res1);
    }
    
    if (doc.containsKey("res2")) {
      bool nuevoEstado = doc["res2"];
      sys.res2Deseado = nuevoEstado;
      Serial.printf("   RES2: Estado anterior=%d, Nuevo=%d\n", sys.res2, nuevoEstado);
      digitalWrite(PIN_RES2, nuevoEstado ? LOW : HIGH);
      sys.res2 = nuevoEstado;
      Serial.printf("   RES2: PIN %d = %s, Estado guardado=%d\n", 
                    PIN_RES2, nuevoEstado ? "LOW" : "HIGH", sys.res2);
    } else {
      Serial.printf("   RES2: No hay comando, manteniendo estado=%d\n", sys.res2);
    }
    
    if (doc.containsKey("hum")) {
      bool nuevoEstado = doc["hum"];
      sys.humDeseado = nuevoEstado;
      Serial.printf("   HUM: Estado anterior=%d, Nuevo=%d\n", sys.hum, nuevoEstado);
      digitalWrite(PIN_HUM, nuevoEstado ? LOW : HIGH);
      sys.hum = nuevoEstado;
      Serial.printf("   HUM: PIN %d = %s, Estado guardado=%d\n", 
                    PIN_HUM, nuevoEstado ? "LOW" : "HIGH", sys.hum);
    } else {
      Serial.printf("   HUM: No hay comando, manteniendo estado=%d\n", sys.hum);
    }
    
    if (doc.containsKey("deshum")) {
      bool nuevoEstado = doc["deshum"];
      sys.deshumDeseado = nuevoEstado;
      Serial.printf("   DESHUM: Estado anterior=%d, Nuevo=%d\n", sys.deshum, nuevoEstado);
      digitalWrite(PIN_DESHUM, nuevoEstado ? LOW : HIGH);
      sys.deshum = nuevoEstado;
      Serial.printf("   DESHUM: PIN %d = %s, Estado guardado=%d\n", 
                    PIN_DESHUM, nuevoEstado ? "LOW" : "HIGH", sys.deshum);
    } else {
      Serial.printf("   DESHUM: No hay comando, manteniendo estado=%d\n", sys.deshum);
    }
    
    Serial.printf("📊 Estados finales: R1=%d R2=%d H=%d D=%d\n", 
                  sys.res1, sys.res2, sys.hum, sys.deshum);
  }
}

// ===========================================
// LEER SENSORES
// ===========================================
void leerSensores() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (!isnan(temp) && !isnan(hum)) {
    sys.temperatura = temp;
    sys.humedad = hum;
  }
  
  if (balanza.is_ready()) {
    sys.peso = balanza.get_units(10);
  }
}

// ===========================================
// CONTROL MANUAL (MANTIENE ESTADOS)
// ===========================================
void controlManual() {
  // CRÍTICO: En modo manual NO se ejecuta ninguna lógica de control
  // Los estados se conservan exactamente como fueron configurados por MQTT
  // NO llamamos a actualizarActuador() para evitar cambios no deseados
  
  static unsigned long lastDebug = 0;
  unsigned long ahora = millis();
  
  if (ahora - lastDebug >= 10000) {
    lastDebug = ahora;
    Serial.println("\n🎮 === MODO MANUAL - ESTADOS CONSERVADOS ===");
    Serial.printf("📊 R1=%d (PIN%d=%s) R2=%d (PIN%d=%s) H=%d (PIN%d=%s) D=%d (PIN%d=%s)\n", 
                  sys.res1, PIN_RES1, sys.res1 ? "LOW" : "HIGH",
                  sys.res2, PIN_RES2, sys.res2 ? "LOW" : "HIGH",
                  sys.hum, PIN_HUM, sys.hum ? "LOW" : "HIGH",
                  sys.deshum, PIN_DESHUM, sys.deshum ? "LOW" : "HIGH");
                  
    // Verificar que los pines físicos coincidan con el estado guardado
    Serial.println("🔍 Verificando consistencia física:");
    Serial.printf("   PIN %d debe estar en %s\n", PIN_RES1, sys.res1 ? "LOW" : "HIGH");
    Serial.printf("   PIN %d debe estar en %s\n", PIN_RES2, sys.res2 ? "LOW" : "HIGH");
    Serial.printf("   PIN %d debe estar en %s\n", PIN_HUM, sys.hum ? "LOW" : "HIGH");
    Serial.printf("   PIN %d debe estar en %s\n", PIN_DESHUM, sys.deshum ? "LOW" : "HIGH");
  }
}

// ===========================================
// CONTROL AUTOMÁTICO (PID SIMPLIFICADO)
// ===========================================
void controlAutomatico() {
  Serial.println("\n🤖 === CONTROL AUTOMÁTICO ===");
  Serial.printf("🌡️  Temp: %.1f°C (SP: %.1f°C ±%.1f°C)\n", 
                sys.temperatura, sys.tempSetpoint, sys.tempHisteresis);
  Serial.printf("💧 Hum: %.1f%% (SP: %.1f%% ±%.1f%%)\n", 
                sys.humedad, sys.humSetpoint, sys.humHisteresis);
  
  // === CONTROL DE TEMPERATURA ===
  float tempMin = sys.tempSetpoint - sys.tempHisteresis;
  float tempMax = sys.tempSetpoint + sys.tempHisteresis;
  
  if (sys.temperatura < tempMin) {
    // Muy frío -> 2 resistencias
    Serial.println("   ❄️  Muy frío → RES1 y RES2 ON");
    actualizarActuador(PIN_RES1, sys.res1, true);
    actualizarActuador(PIN_RES2, sys.res2, true);
  } 
  else if (sys.temperatura < sys.tempSetpoint) {
    // Frío moderado -> 1 resistencia
    Serial.println("   🌡️  Frío → RES1 ON, RES2 OFF");
    actualizarActuador(PIN_RES1, sys.res1, true);
    actualizarActuador(PIN_RES2, sys.res2, false);
  } 
  else if (sys.temperatura > tempMax) {
    // Caliente -> apagar
    Serial.println("   🔥 Caliente → Resistencias OFF");
    actualizarActuador(PIN_RES1, sys.res1, false);
    actualizarActuador(PIN_RES2, sys.res2, false);
  }
  else {
    Serial.println("   ✅ Temp en rango → Sin cambios");
  }
  
  // === CONTROL DE HUMEDAD ===
  float humMin = sys.humSetpoint - sys.humHisteresis;
  float humMax = sys.humSetpoint + sys.humHisteresis;
  
  if (sys.humedad < humMin) {
    // Muy seco -> humidificar
    Serial.println("   🏜️  Muy seco → HUM ON");
    actualizarActuador(PIN_HUM, sys.hum, true);
    actualizarActuador(PIN_DESHUM, sys.deshum, false);
  } 
  else if (sys.humedad > humMax) {
    // Muy húmedo -> deshumidificar
    Serial.println("   💦 Muy húmedo → DESHUM ON");
    actualizarActuador(PIN_HUM, sys.hum, false);
    actualizarActuador(PIN_DESHUM, sys.deshum, true);
  } 
  else {
    // En rango -> apagar
    Serial.println("   ✅ Hum en rango → HUM y DESHUM OFF");
    actualizarActuador(PIN_HUM, sys.hum, false);
    actualizarActuador(PIN_DESHUM, sys.deshum, false);
  }
  
  Serial.printf("📊 Estado actual: R1=%d R2=%d H=%d D=%d\n", 
                sys.res1, sys.res2, sys.hum, sys.deshum);
}

// ===========================================
// ACTUALIZAR ACTUADOR
// ===========================================
void actualizarActuador(int pin, bool &estado, bool activar) {
  if (estado != activar) {
    digitalWrite(pin, activar ? LOW : HIGH);
    estado = activar;
    Serial.printf("⚡ PIN %d → %s\n", pin, activar ? "ACTIVO (LOW)" : "INACTIVO (HIGH)");
  }
}

// ===========================================
// APAGAR TODO
// ===========================================
void apagarTodo() {
  digitalWrite(PIN_RES1, HIGH);
  digitalWrite(PIN_RES2, HIGH);
  digitalWrite(PIN_HUM, HIGH);
  digitalWrite(PIN_DESHUM, HIGH);
  sys.res1 = sys.res2 = sys.hum = sys.deshum = false;
}

// ===========================================
// ENVIAR ESTADO POR MQTT
// ===========================================
void enviarEstado() {
  StaticJsonDocument<512> doc;
  
  // Sensores
  doc["temperatura"] = round(sys.temperatura * 10) / 10.0;
  doc["humedad"] = round(sys.humedad * 10) / 10.0;
  doc["peso"] = round(sys.peso * 100) / 100.0;
  
  // Actuadores (estado REAL actual)
  doc["res1"] = sys.res1;
  doc["res2"] = sys.res2;
  doc["humidificador"] = sys.hum;
  doc["deshumidificador"] = sys.deshum;
  
  // Config
  doc["tempSetpoint"] = sys.tempSetpoint;
  doc["humSetpoint"] = sys.humSetpoint;
  doc["autoMode"] = sys.modoAuto;
  doc["sistemaActivo"] = sys.sistemaActivo;
  
  // Sistema
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  if (client.publish(TOPIC_PUB, buffer)) {
    // Debug reducido - solo mostrar cada 5 envíos
    static int contador = 0;
    if (++contador >= 5) {
      Serial.println("📤 Estado enviado (x5)");
      contador = 0;
    }
  } else {
    Serial.println("❌ Error enviando estado");
  }
}