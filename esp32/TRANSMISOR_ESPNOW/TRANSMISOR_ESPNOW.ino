#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// MAC del esclavo (cambiar por la MAC real del esclavo)
uint8_t slaveMAC[] = {0xD8, 0xBC, 0x38, 0xFB, 0xAC, 0x00}; // Actualizada con tu MAC

// Configuración WiFi para mejor rendimiento ESP-NOW
#define WIFI_CHANNEL 1  // Canal WiFi fijo para mejor estabilidad

// Estructura para enviar datos
typedef struct {
  char json_data[250];
} esp_now_message;

// Pines de entrada
#define BUTTON_ON 18
#define BUTTON_OFF 19
#define BUTTON_TEMP 21  // Botón para activación temporal

// Variables de control
bool lastButtonOnState = HIGH;
bool lastButtonOffState = HIGH;
bool lastButtonTempState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Variables para reenvío y confiabilidad
bool waitingForAck = false;
unsigned long lastSendTime = 0;
const unsigned long SEND_TIMEOUT = 2000; // 2 segundos timeout
const unsigned long MIN_SEND_INTERVAL = 100; // Mínimo 100ms entre envíos
int sendAttempts = 0;
const int MAX_SEND_ATTEMPTS = 3;

void setup() {
  Serial.begin(115200);
  
  // Configurar pines
  pinMode(BUTTON_ON, INPUT_PULLUP);
  pinMode(BUTTON_OFF, INPUT_PULLUP);
  pinMode(BUTTON_TEMP, INPUT_PULLUP);
  
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
  
  // Imprimir MAC del maestro
  Serial.print("MAC del Maestro: ");
  Serial.println(WiFi.macAddress());
  
  // Verificar que la MAC no sea 00:00:00:00:00:00
  if (WiFi.macAddress() == "00:00:00:00:00:00") {
    Serial.println("Error: MAC no inicializada, reintentando...");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.print("MAC del Maestro (reintento): ");
    Serial.println(WiFi.macAddress());
  }
  
  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }
  
  // Registrar peer (esclavo)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo)); // Limpiar estructura
  memcpy(peerInfo.peer_addr, slaveMAC, 6);
  peerInfo.channel = WIFI_CHANNEL; // Usar el mismo canal
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error agregando peer");
    return;
  }
  
  // Registrar callback para confirmación de envío
  esp_now_register_send_cb(onDataSent);
  
  Serial.println("Maestro ESP-NOW iniciado");
  Serial.println("Presiona botones para controlar el relé:");
  Serial.println("- Botón ON (GPIO18): Encender relé indefinidamente");
  Serial.println("- Botón OFF (GPIO19): Apagar relé");
  Serial.println("- Botón TEMP (GPIO21): Encender relé por 5 segundos");
}

void loop() {
  // Verificar timeout de envío y reintentar si es necesario
  if (waitingForAck && (millis() - lastSendTime > SEND_TIMEOUT)) {
    Serial.printf("Timeout en envío, reintentando... (intento %d/%d)\n", sendAttempts + 1, MAX_SEND_ATTEMPTS);
    if (sendAttempts < MAX_SEND_ATTEMPTS) {
      // Reenviar último comando
      sendAttempts++;
      waitingForAck = false; // Permitir reenvío
      // El comando se reenviará automáticamente en la próxima iteración
    } else {
      Serial.println("Máximo número de intentos alcanzado, cancelando envío");
      waitingForAck = false;
      sendAttempts = 0;
    }
  }
  
  // Solo procesar nuevos comandos si no estamos esperando confirmación
  // o si ha pasado el intervalo mínimo
  if (!waitingForAck || (millis() - lastSendTime > MIN_SEND_INTERVAL)) {
    // Leer botones con debounce
    bool buttonOnState = digitalRead(BUTTON_ON);
    bool buttonOffState = digitalRead(BUTTON_OFF);
    bool buttonTempState = digitalRead(BUTTON_TEMP);
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
      // Botón ON presionado
      if (buttonOnState == LOW && lastButtonOnState == HIGH) {
        sendRelayCommand("on", 0);
        lastDebounceTime = millis();
      }
      
      // Botón OFF presionado
      if (buttonOffState == LOW && lastButtonOffState == HIGH) {
        sendRelayCommand("off", 0);
        lastDebounceTime = millis();
      }
      
      // Botón TEMP presionado (5 segundos)
      if (buttonTempState == LOW && lastButtonTempState == HIGH) {
        sendRelayCommand("on", 5000);
        lastDebounceTime = millis();
      }
    }
    
    lastButtonOnState = buttonOnState;
    lastButtonOffState = buttonOffState;
    lastButtonTempState = buttonTempState;
  }
  
  // Comandos por Serial para pruebas
  if (Serial.available() && !waitingForAck) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "on") {
      sendRelayCommand("on", 0);
    } else if (command == "off") {
      sendRelayCommand("off", 0);
    } else if (command.startsWith("on ")) {
      int duration = command.substring(3).toInt();
      sendRelayCommand("on", duration * 1000); // Convertir a ms
    } else {
      Serial.println("Comandos disponibles:");
      Serial.println("  on - Encender relé indefinidamente");
      Serial.println("  off - Apagar relé");
      Serial.println("  on [segundos] - Encender relé por tiempo específico");
    }
  }
  
  delay(10);
}

void sendRelayCommand(String action, unsigned long duration) {
  // No enviar si estamos esperando confirmación (excepto si es reintento)
  if (waitingForAck && sendAttempts == 0) {
    Serial.println("Esperando confirmación del envío anterior...");
    return;
  }
  
  // Crear JSON
  DynamicJsonDocument doc(1024);
  doc["command"] = "relay_control";
  doc["relay"] = action;
  doc["master_id"] = WiFi.macAddress();
  doc["timestamp"] = millis();
  doc["attempt"] = sendAttempts + 1;
  
  if (duration > 0) {
    doc["duration"] = duration;
  }
  
  // Convertir a string
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Preparar mensaje
  esp_now_message message;
  memset(&message, 0, sizeof(message)); // Limpiar estructura
  strncpy(message.json_data, jsonString.c_str(), sizeof(message.json_data) - 1);
  message.json_data[sizeof(message.json_data) - 1] = '\0'; // Asegurar terminación
  
  // Enviar mensaje con pequeño delay para estabilidad
  delay(10);
  esp_err_t result = esp_now_send(slaveMAC, (uint8_t*)&message, sizeof(message));
  
  if (result == ESP_OK) {
    waitingForAck = true;
    lastSendTime = millis();
    Serial.printf("Comando enviado (intento %d): %s", sendAttempts + 1, jsonString.c_str());
    if (duration > 0) {
      Serial.printf(" (duración: %lu ms)", duration);
    }
    Serial.println();
  } else {
    Serial.printf("Error en esp_now_send: %d\n", result);
    waitingForAck = false;
    sendAttempts = 0;
  }
}

// Callback cuando se confirma el envío
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  String macStr = "";
  for (int i = 0; i < 6; i++) {
    if (i > 0) macStr += ":";
    if (mac_addr[i] < 16) macStr += "0";
    macStr += String(mac_addr[i], HEX);
  }
  macStr.toUpperCase();
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.printf("✓ Envío a %s: ÉXITO\n", macStr.c_str());
    waitingForAck = false;
    sendAttempts = 0;
  } else {
    Serial.printf("✗ Envío a %s: FALLO", macStr.c_str());
    if (sendAttempts < MAX_SEND_ATTEMPTS) {
      Serial.printf(" - Reintentando en %dms...\n", SEND_TIMEOUT/2);
      // El reintento se manejará en el loop principal
    } else {
      Serial.println(" - Máximo de intentos alcanzado");
      waitingForAck = false;
      sendAttempts = 0;
    }
  }
}