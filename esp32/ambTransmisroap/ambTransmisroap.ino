#include <WiFi.h>

// Configuración WiFi - conéctate al AP del receptor
char ssid[]= "RelayControllerAP";      // Nombre del AP del receptor
char password[]  = "control123";          // Contraseña del AP

// Configuración del servidor (receptor)
const char* host = "192.168.4.1";            // IP fija del AP del receptor
const int port = 80;                          // Puerto del servidor

// Configuración del hardware
#define BUTTON_PIN 2                         // Pin del botón (ajustar según conexión)
#define LED_PIN 3                            // Pin del LED (opcional)

// Identificador único para este transmisor
String txId = "AMB1";                        // Cambiar por un ID único para cada dispositivo

// Variables de estado
bool lastButtonState = HIGH;
bool currentRelayState = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);         // Botón con pull-up interno
  pinMode(LED_PIN, OUTPUT);                  // LED indicador (opcional)
  
  connectToWiFi();
}

void connectToWiFi() {
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // LED parpadeante durante conexión
  }
  
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH); // LED fijo cuando conectado
}

void sendCommand(String command) {
  Serial.print("Enviando comando: ");
  Serial.println(command);
  
  WiFiClient client;
  
  if (!client.connect(host, port)) {
    Serial.println("Fallo al conectar con el receptor");
    digitalWrite(LED_PIN, LOW); // Indicar error
    delay(200);
    digitalWrite(LED_PIN, HIGH);
    return;
  }
  
  // Enviar el comando
  client.print(command + "\n");
  
  // Esperar respuesta
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 2000) {
      Serial.println("Timeout esperando respuesta");
      client.stop();
      return;
    }
  }
  
  // Leer respuesta
  String response = client.readStringUntil('\n');
  response.trim();
  Serial.print("Respuesta: ");
  Serial.println(response);
  
  // Actualizar estado del relé (si la respuesta incluye el estado)
  if (response.startsWith("OK:")) {
    currentRelayState = (response.substring(3) == "ON");
    Serial.print("Estado actual del relé: ");
    Serial.println(currentRelayState ? "ON" : "OFF");
  }
  
  client.stop();
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW); // LED apagado si desconectado
    connectToWiFi();
    return;
  }
  
  // Leer estado del botón
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detectar cambio de estado del botón
  if (currentButtonState != lastButtonState) {
    delay(50); // Debounce
    
    if (digitalRead(BUTTON_PIN) == currentButtonState) {
      lastButtonState = currentButtonState;
      
      if (currentButtonState == LOW) {
        // Botón presionado - enviar ON
        sendCommand(txId + ":ON");
      } else {
        // Botón liberado - enviar OFF
        sendCommand(txId + ":OFF");
      }
    }
  }
  
  // Opcional: LED refleja estado del relé
  digitalWrite(LED_PIN, currentRelayState ? HIGH : LOW);
  
  delay(10);
}