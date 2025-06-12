#include <WiFi.h>

const char* ssid = "RelayControllerAP";
const char* password = "control123";

const char* host = "192.168.4.1"; // IP del AP
const int port = 80;

const int buttonPin = 27; // Pin del botón
bool lastButtonState = HIGH;
String txId = "TX1"; // ID único para cada transmisor

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado al AP");
}

void sendCommand(String cmd) {
  WiFiClient client;
  
  if (!client.connect(host, port)) {
    Serial.println("Fallo en conexión");
    return;
  }
  
  // Enviar comando
  client.println(cmd);
  
  // Leer respuesta
  while (client.connected() && !client.available()) delay(1);
  String response = client.readStringUntil('\n');
  Serial.print("Respuesta: ");
  Serial.println(response);
  
  client.stop();
}

void loop() {
  bool currentButtonState = digitalRead(buttonPin);
  
  if (currentButtonState != lastButtonState) {
    delay(50); // Debounce
    if (digitalRead(buttonPin) == currentButtonState) {
      lastButtonState = currentButtonState;
      
      if (currentButtonState == LOW) {
        // Botón presionado
        sendCommand(txId + ":ON");
      } else {
        // Botón liberado
        sendCommand(txId + ":OFF");
      }
    }
  }
  
  delay(10);
}