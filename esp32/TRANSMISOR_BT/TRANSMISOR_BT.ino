#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <ArduinoJson.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const int buttonPin = 27;
bool lastButtonState = HIGH;
bool relayState = false;
BLEClient* pClient;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  
  BLEDevice::init("ESP32_Client");
  pClient = BLEDevice::createClient();
  Serial.println("Cliente BLE creado");
}

void connectToServer() {
  if (!pClient->isConnected()) {
    Serial.println("Conectando al servidor...");
    BLEAddress serverAddress("d4:8a:fc:a5:7a:5a"); // Reemplazar con la dirección MAC del esclavo
    if (pClient->connect(serverAddress)) {
      Serial.println("Conectado al servidor");
    } else {
      Serial.println("Error al conectar");
      return;
    }
  }

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Servicio no encontrado");
    pClient->disconnect();
    return;
  }

  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Característica no encontrada");
    pClient->disconnect();
    return;
  }

  // Crear y enviar el comando JSON
  DynamicJsonDocument doc(200);
  doc["action"] = "relay";
  doc["state"] = !relayState; // Alternar estado
  
  String output;
  serializeJson(doc, output);
  
  pRemoteCharacteristic->writeValue(output.c_str(), output.length());
  relayState = !relayState;
  Serial.println("Comando enviado: " + output);
}

void loop() {
  bool currentButtonState = digitalRead(buttonPin);
  
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    // Botón presionado
    connectToServer();
    delay(200); // Debounce
  }
  
  lastButtonState = currentButtonState;
  delay(10);
}