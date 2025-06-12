#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

const char* apSSID = "RelayControllerAP";
const char* apPassword = "control123";

WiFiServer server(80);
WiFiClient client;

long tnow = 0;

const int relayPin = 16; // Pin del relé
bool relayState = false;

// Estructura para los transmisores
struct Transmitter {
  String id;
  bool active;
};

Transmitter transmitters[5]; // Máximo 5 transmisores
int txCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Iniciar AP
  WiFi.softAP(apSSID, apPassword);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  server.begin();
  Serial.println("Servidor iniciado");
}

void processCommand(String cmd) {
  // Formato: "ID:COMANDO" (ej. "TX1:ON" o "TX2:OFF")
  int separatorIndex = cmd.indexOf(':');
  if (separatorIndex == -1) return;
  
  String txId = cmd.substring(0, separatorIndex);
  String command = cmd.substring(separatorIndex + 1);
  
  // Buscar transmisor
  int txIndex = -1;
  for (int i = 0; i < txCount; i++) {
    if (transmitters[i].id == txId) {
      txIndex = i;
      break;
    }
  }
  
  // Si no existe y es comando ON, agregarlo
  if (txIndex == -1 && command == "ON") {
    digitalWrite(relayPin, HIGH);
    tnow = millis();
    if (txCount < 5) {
      transmitters[txCount].id = txId;
      transmitters[txCount].active = true;
      txCount++;
      txIndex = txCount - 1;
    }
  }
  if (txIndex == -1 && command == "OFF") {
    digitalWrite(relayPin, LOW);
    
  }
  
  // Actualizar estado
  if (txIndex != -1) {
    transmitters[txIndex].active = (command == "ON");
  }
  
  // Actualizar relé
  //updateRelay();
}

void updateRelay() {
  bool newState = false;
  for (int i = 0; i < txCount; i++) {
    if (transmitters[i].active) {
      newState = true;
      break;
    }
  }
  
  if (newState != relayState) {
    relayState = newState;
    digitalWrite(relayPin, relayState ? HIGH : LOW);
    Serial.print("Relé cambiado a: ");
    Serial.println(relayState ? "ON" : "OFF");
  }
}

void loop() {
  if (!client.connected()) {
    client = server.available();
    return;
  }
  
  if (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      Serial.print("Recibido: ");
      Serial.println(line);
      processCommand(line);
      
      // Respuesta simple
      client.print("OK:");
      client.println(relayState ? "ON" : "OFF");
    }
  }
  
  delay(10);

  if(millis() - tnow > 60000){
    digitalWrite(relayPin, LOW);
  }


}