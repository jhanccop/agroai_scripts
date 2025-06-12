#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <map>

// Configuración WiFi para mejor rendimiento ESP-NOW
#define WIFI_CHANNEL 1  // Canal WiFi fijo para mejor estabilidad

// Configuración del hardware
#define RELAY_PIN 2
#define LED_STATUS 16  // LED para indicar estado

// Estructura para rastrear solicitudes activas
struct RelayRequest {
  bool active;
  unsigned long timestamp;
  unsigned long duration;  // Duración en ms (0 = indefinido)
};

// Mapa para rastrear solicitudes por MAC del maestro
std::map<String, RelayRequest> activeRequests;

// Variables de control
bool relayState = false;
unsigned long lastCleanup = 0;
const unsigned long CLEANUP_INTERVAL = 1000; // Limpieza cada segundo
const unsigned long REQUEST_TIMEOUT = 30000;  // Timeout de 30 segundos para solicitudes indefinidas

// Estructura para recibir datos
typedef struct {
  char json_data[250];
} esp_now_message;

void setup() {
  Serial.begin(115200);
  
  // Configurar pines
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_STATUS, LOW);
  
  // Configurar WiFi en modo estación
  WiFi.mode(WIFI_STA);
  
  // Esperar a que WiFi se inicialice completamente
  delay(100);
  
  // Configurar canal WiFi fijo para mejor estabilidad
  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("Error configurando canal WiFi");
  }
  
  // Configurar potencia de transmisión al máximo
  if (esp_wifi_set_max_tx_power(84) != ESP_OK) { // 21dBm = 84 * 0.25dBm
    Serial.println("Error configurando potencia TX");
  }
  
  // Imprimir MAC del esclavo
  Serial.print("MAC del Esclavo: ");
  Serial.println(WiFi.macAddress());
  
  // Verificar que la MAC no sea 00:00:00:00:00:00
  if (WiFi.macAddress() == "00:00:00:00:00:00") {
    Serial.println("Error: MAC no inicializada, reintentando...");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.print("MAC del Esclavo (reintento): ");
    Serial.println(WiFi.macAddress());
  }
  
  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }
  
  // Registrar callback para recibir datos
  esp_now_register_recv_cb(onDataReceived);
  
  Serial.println("Esclavo ESP-NOW iniciado - Esperando comandos...");
}

void loop() {
  // Limpieza periódica de solicitudes expiradas
  if (millis() - lastCleanup > CLEANUP_INTERVAL) {
    cleanupExpiredRequests();
    updateRelayState();
    lastCleanup = millis();
  }
  
  delay(10);
}

// Callback cuando se reciben datos
void onDataReceived(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
  // Convertir MAC a string
  String senderMAC = macToString(recv_info->src_addr);
  
  // Copiar datos recibidos
  esp_now_message receivedData;
  memcpy(&receivedData, data, sizeof(receivedData));
  
  Serial.printf("Datos recibidos de %s: %s\n", senderMAC.c_str(), receivedData.json_data);
  
  // Parsear JSON
  processJSONCommand(senderMAC, receivedData.json_data);
}

void processJSONCommand(String senderMAC, const char* jsonData) {
  // Crear documento JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.printf("Error parseando JSON de %s: %s\n", senderMAC.c_str(), error.c_str());
    return;
  }
  
  // Verificar campos requeridos
  if (!doc.containsKey("command") || !doc.containsKey("relay")) {
    Serial.printf("JSON inválido de %s - faltan campos requeridos\n", senderMAC.c_str());
    return;
  }
  
  String command = doc["command"];
  String relayAction = doc["relay"];
  
  if (command != "relay_control") {
    Serial.printf("Comando desconocido de %s: %s\n", senderMAC.c_str(), command.c_str());
    return;
  }
  
  // Procesar comando del relé
  if (relayAction == "on") {
    unsigned long duration = 0;
    if (doc.containsKey("duration")) {
      duration = doc["duration"]; // Duración en ms
    }
    
    addRelayRequest(senderMAC, duration);
    Serial.printf("Solicitud ON de %s", senderMAC.c_str());
    if (duration > 0) {
      Serial.printf(" por %lu ms", duration);
    }
    Serial.println();
    
  } else if (relayAction == "off") {
    removeRelayRequest(senderMAC);
    Serial.printf("Solicitud OFF de %s\n", senderMAC.c_str());
    
  } else {
    Serial.printf("Acción de relé inválida de %s: %s\n", senderMAC.c_str(), relayAction.c_str());
  }
  
  updateRelayState();
}

void addRelayRequest(String senderMAC, unsigned long duration) {
  RelayRequest request;
  request.active = true;
  request.timestamp = millis();
  request.duration = duration;
  
  activeRequests[senderMAC] = request;
}

void removeRelayRequest(String senderMAC) {
  activeRequests.erase(senderMAC);
}

void cleanupExpiredRequests() {
  unsigned long now = millis();
  auto it = activeRequests.begin();
  
  while (it != activeRequests.end()) {
    bool shouldRemove = false;
    
    // Verificar si la solicitud con duración específica ha expirado
    if (it->second.duration > 0) {
      if (now - it->second.timestamp >= it->second.duration) {
        Serial.printf("Solicitud de %s expirada por duración\n", it->first.c_str());
        shouldRemove = true;
      }
    } else {
      // Verificar timeout para solicitudes indefinidas
      if (now - it->second.timestamp >= REQUEST_TIMEOUT) {
        Serial.printf("Solicitud de %s expirada por timeout\n", it->first.c_str());
        shouldRemove = true;
      }
    }
    
    if (shouldRemove) {
      it = activeRequests.erase(it);
    } else {
      ++it;
    }
  }
}

void updateRelayState() {
  bool shouldBeOn = !activeRequests.empty();
  
  if (shouldBeOn != relayState) {
    relayState = shouldBeOn;
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    digitalWrite(LED_STATUS, relayState ? HIGH : LOW);
    
    Serial.printf("Relé %s - Solicitudes activas: %d\n", 
                  relayState ? "ENCENDIDO" : "APAGADO", 
                  activeRequests.size());
    
    // Mostrar solicitudes activas
    if (!activeRequests.empty()) {
      Serial.println("Solicitudes activas:");
      for (const auto& pair : activeRequests) {
        unsigned long elapsed = millis() - pair.second.timestamp;
        Serial.printf("  - %s: %lu ms transcurridos", pair.first.c_str(), elapsed);
        if (pair.second.duration > 0) {
          Serial.printf(" (restantes: %lu ms)", pair.second.duration - elapsed);
        }
        Serial.println();
      }
    }
  }
}

String macToString(const uint8_t* mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}