#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Credenciales para el punto de acceso
const char* ssid = "Viscosimeter Dashboard";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando...");
  
  // Configurar ESP32 como punto de acceso
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Configuración de ArduinoOTA
  ArduinoOTA.setHostname("esp32-ota");
  
  // Contraseña opcional para la actualización OTA
  ArduinoOTA.setPassword("admin");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else
      type = "filesystem";
    Serial.println("Iniciando actualización " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nActualización finalizada");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();
  // Tu código principal aquí
}