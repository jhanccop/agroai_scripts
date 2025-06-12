#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

// Configuración de red
const char* apSSID = "RelayControllerAP";
const char* apPassword = "control123";
const int serverPort = 80;

// Hardware
const int relayPin = 2;
bool relayState = false;

// Networking
WiFiServer server(serverPort);
unsigned long lastClientCheck = 0;
const unsigned long CLIENT_TIMEOUT = 30000; // 30 segundos timeout
const unsigned long CLIENT_CHECK_INTERVAL = 5000; // Verificar cada 5 segundos

// Estructura para transmisores
struct Transmitter {
  String id;
  bool active;
  unsigned long lastSeen;
};

Transmitter transmitters[5];
int txCount = 0;
const unsigned long TX_TIMEOUT = 60000; // 60 segundos para considerar transmisor inactivo

// Control de temperatura y rendimiento
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 10000; // 10 segundos

void setup() {
  Serial.begin(115200);
  
  // Configurar hardware
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  
  // Configurar WiFi con parámetros optimizados
  WiFi.mode(WIFI_AP);
  
  // Configuración AP optimizada para estabilidad
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),    // IP del AP
    IPAddress(192, 168, 4, 1),    // Gateway
    IPAddress(255, 255, 255, 0)   // Subnet mask
  );
  
  // Iniciar AP con configuración optimizada
  bool apResult = WiFi.softAP(apSSID, apPassword, 1, 0, 4); // Canal 1, no oculto, max 4 clientes
  
  if (!apResult) {
    Serial.println("Error iniciando AP");
    ESP.restart();
  }
  
  // Reducir potencia de transmisión para menos calentamiento
  WiFi.setTxPower(WIFI_POWER_11dBm); // Reducir de 20dBm a 11dBm
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(myIP);
  
  server.begin();
  server.setNoDelay(true); // Deshabilitar algoritmo de Nagle
  
  Serial.println("Servidor iniciado - Modo optimizado");
  Serial.printf("Potencia TX: 11dBm, Canal: 1, Max clientes: 4\n");
}

void processCommand(String cmd) {
  int separatorIndex = cmd.indexOf(':');
  if (separatorIndex == -1) return;
  
  String txId = cmd.substring(0, separatorIndex);
  String command = cmd.substring(separatorIndex + 1);
  
  // Buscar transmisor existente
  int txIndex = -1;
  for (int i = 0; i < txCount; i++) {
    if (transmitters[i].id == txId) {
      txIndex = i;
      break;
    }
  }
  
  // Agregar nuevo transmisor si no existe
  if (txIndex == -1 && command == "ON" && txCount < 5) {
    transmitters[txCount].id = txId;
    transmitters[txCount].active = true;
    transmitters[txCount].lastSeen = millis();
    txCount++;
    Serial.println("Nuevo transmisor: " + txId);
  }
  
  // Actualizar estado existente
  if (txIndex != -1) {
    transmitters[txIndex].active = (command == "ON");
    transmitters[txIndex].lastSeen = millis();
  }
  
  updateRelay();
}

void updateRelay() {
  bool newState = false;
  
  // Verificar transmisores activos
  for (int i = 0; i < txCount; i++) {
    if (transmitters[i].active) {
      newState = true;
      break;
    }
  }
  
  if (newState != relayState) {
    relayState = newState;
    digitalWrite(relayPin, relayState ? HIGH : LOW);
    Serial.printf("Relé: %s\n", relayState ? "ON" : "OFF");
  }
}

void cleanupInactiveTransmitters() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < txCount; i++) {
    if (currentTime - transmitters[i].lastSeen > TX_TIMEOUT) {
      Serial.println("Timeout transmisor: " + transmitters[i].id);
      
      // Mover el último elemento a esta posición
      if (i < txCount - 1) {
        transmitters[i] = transmitters[txCount - 1];
        i--; // Revisar esta posición nuevamente
      }
      txCount--;
      updateRelay();
    }
  }
}

void handleClient(WiFiClient& client) {
  unsigned long clientStart = millis();
  String request = "";
  
  // Leer con timeout
  while (client.connected() && (millis() - clientStart < 5000)) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        break;
      } else if (c != '\r') {
        request += c;
      }
    }
    yield(); // Permitir que otras tareas se ejecuten
  }
  
  request.trim();
  
  if (request.length() > 0) {
    Serial.println("RX: " + request);
    processCommand(request);
    
    // Respuesta rápida
    client.printf("OK:%s\n", relayState ? "ON" : "OFF");
  }
  
  // Cerrar conexión inmediatamente
  client.stop();
}

void monitorSystem() {
  unsigned long currentTime = millis();
  
  // Heartbeat y estadísticas
  if (currentTime - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = currentTime;
    
    Serial.printf("Status - Clientes: %d, Transmisores: %d, Relé: %s, Heap: %d\n", 
                  WiFi.softAPgetStationNum(), txCount, 
                  relayState ? "ON" : "OFF", ESP.getFreeHeap());
    
    // Verificar si el AP sigue activo
    if (WiFi.getMode() != WIFI_AP) {
      Serial.println("AP desconectado - Reiniciando...");
      ESP.restart();
    }
  }
  
  // Limpiar transmisores inactivos
  if (currentTime - lastClientCheck > CLIENT_CHECK_INTERVAL) {
    lastClientCheck = currentTime;
    cleanupInactiveTransmitters();
  }
}

void loop() {
  // Verificar nuevos clientes
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
  
  // Monitoreo del sistema
  monitorSystem();
  
  // Delay mínimo para eficiencia energética
  delay(1);
  yield(); // Permitir tareas de WiFi
}