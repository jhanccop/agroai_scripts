#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define RELAY_PIN 27

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
int activeRequests = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Dispositivo conectado");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Dispositivo desconectado");
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      // Solución definitiva para el problema de conversión
      String valueStr = pCharacteristic->getValue().c_str();
      
      if (valueStr.length() > 0) {
        Serial.print("Comando recibido: ");
        Serial.println(valueStr);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, valueStr);
        
        if (error) {
          Serial.print("Error al parsear JSON: ");
          Serial.println(error.c_str());
          return;
        }
        
        const char* action = doc["action"];
        bool state = doc["state"];
        
        if (strcmp(action, "relay") == 0) {
          if (state) {
            activeRequests++;
            digitalWrite(RELAY_PIN, HIGH);
            Serial.printf("Relay ACTIVADO. Solicitudes activas: %d\n", activeRequests);
          } else {
            activeRequests = max(0, activeRequests - 1);
            if (activeRequests == 0) {
              digitalWrite(RELAY_PIN, LOW);
              Serial.println("Relay DESACTIVADO. No hay solicitudes pendientes.");
            } else {
              Serial.printf("Solicitud removida. Solicitudes restantes: %d\n", activeRequests);
            }
          }
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  while (!Serial); // Espera a que el puerto serial esté listo
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  BLEDevice::init("ESP32_Relay_Server");
  
  // Mostrar dirección MAC
  Serial.print("Dirección MAC: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Servidor BLE listo. Esperando conexiones...");
}

void loop() {
  delay(1000);
}