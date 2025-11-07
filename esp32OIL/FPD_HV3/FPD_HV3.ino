/*
 * Well Analyzer - Load Cell & Accelerometer Monitor
 * Optimized version with OLED feedback and LoRaWAN Config
 * Sensors: HX711 Load Cell, MPU6050 Accelerometer
 * Communication: WiFi/LoRaWAN
 */

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "HX711.h"
#include "heltec.h"
#include <esp_system.h>
#include <Average.h>
#include "HT_SSD1306Wire.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "SPIFFS.h"
#include <WebServer.h>
#include "LoRaWan_APP.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include <esp_now.h>
#include <math.h>

/* =============== HARDWARE PINS =============== */
#define Vext 36
#define ADC_CTRL_PIN 37 
const uint8_t SCL_ACC = 42;
const uint8_t SDA_ACC = 41;
const int LOADCELL_DOUT_PIN = 47;
const int LOADCELL_SCK_PIN = 48;
const int INT_PIN = 33;
const int PIN_CONFIG = 46;
const int PIN_TX_MODE = 26;
const int pinBattery = 1;
const int lR = 7;
const int lG = 6;
const int lB = 5;

/* =============== WIFI CONFIG =============== */
String runssid = "MIFI_8D3D";
String runpassword = "1234567890";
const char *topicPublish = "jhpOandG/data";
char *broker = "24.199.125.52";
int mqtt_port = 1883;

const char *apSSID = "WA_DataGraph";
const char *apPassword = "12345678";

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
String currentMode = "WiFi";
String SearchPath = "/devices/api/buscar-well-analyzer/?mac=";

String mqttTopic = "well/analyzer/data";

/* =============== LORA CONFIG =============== */
uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x03 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 };
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda, 0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef, 0x67 };
uint32_t devAddr = (uint32_t)0x007e6ae1;
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass = CLASS_A;
uint32_t appTxDutyCycle = 300000;
bool overTheAirActivation = true;
bool loraWanAdr = true;
bool isTxConfirmed = true;
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 8;

/* =============== VARIABLES GLOBALES PARA CONTROL DE INTENTOS =============== */
bool transmissionSuccessful = false;
bool isJoined = false; 

uint32_t sleepCycleMs = 300000;  // Default: 5 minutos
bool sleepCycleLoaded = false;   // Flag para saber si ya se cargó

// Nuevas variables para control de intentos
bool downlinkReceived = false;
int downlinkAttemptCount = 0;
int joinAttemptCount = 0;
const int MAX_DOWNLINK_ATTEMPTS = 3;
const int MAX_JOIN_ATTEMPTS = 3;

bool optimizerEnabled = false;
String sensorType = "1"; // HT SENSOR

/* =============== ESP NOW =============== */
bool espnowEnabled = false;
String espnowMac = "";
esp_now_peer_info_t peerInfo;
#include <base64.h>

/* =============== SENSORS =============== */
Adafruit_MPU6050 mpu;
HX711 scale;
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

/* =============== DATA VARIABLES =============== */
const int16_t nData = 600;
float a0[50];
float spm = 0;
float fill = 0;
float sLength = 0;
uint8_t status = 0;
uint8_t integrated = 1;
boolean runNN = true;
float nnDiagnosis[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

Average<int16_t> load20(20);
Average<int16_t> load50(50);

// Rangos de sensores corregidos
const float LOAD_MIN = 0;      // Carga mínima en unidades brutas
const float LOAD_MAX = 10000;  // Carga máxima en unidades brutas
const float ACC_MIN = -20.0;   // Aceleración mínima en m/s²
const float ACC_MAX = 20.0;    // Aceleración máxima en m/s²

/* =============== OLED FUNCTIONS =============== */
void VextON() {
  if(optimizerEnabled) return;
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void oledInit() {
  if(optimizerEnabled) return;
  VextON();
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.display();
}

void oledShow(String line1, String line2 = "", String line3 = "", String line4 = "", String line5 = "") {
  if(optimizerEnabled) return;
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (line1 != "") display.drawString(0, 0, line1);
  if (line2 != "") display.drawString(0, 12, line2);
  if (line3 != "") display.drawString(0, 24, line3);
  if (line4 != "") display.drawString(0, 36, line4);
  if (line5 != "") display.drawString(0, 48, line5);
  display.display();
}

void oledStatus(String status, float load, float acc, float battery) {
  if(optimizerEnabled) return;
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Status: " + status);
  display.drawString(0, 12, "Load: " + String(load, 1));
  display.drawString(0, 24, "Acc: " + String(acc, 2));
  display.drawString(0, 36, "Bat: " + String(battery, 2) + "V");
  display.drawString(0, 48, "Mode: " + currentMode);
  display.display();
}

/* =============== ESP-NOW FUNCTIONS =============== */
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.print("Estado envío: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "Error");
}

// Inicializar ESP-NOW
bool initESPNow(String macAddress) {
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al inicializar ESP-NOW");
    return false;
  }
  
  esp_now_register_send_cb(onDataSent);
  
  // Parsear la dirección MAC
  uint8_t mac[6];
  int parsed = sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                      &mac[0], &mac[1], &mac[2],
                      &mac[3], &mac[4], &mac[5]);
  if (parsed != 6) {
    Serial.println("Dirección MAC inválida");
    return false;
  }

  // Limpiar peerInfo antes de usarlo
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;      // Canal por defecto
  peerInfo.encrypt = false;  // Sin cifrado
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error al agregar el peer");
    return false;
  }

  espnowEnabled = true;
  espnowMac = macAddress;
  Serial.println("ESP-NOW inicializado correctamente");
  return true;
}

void sendESPNowData(Average<uint16_t>& rLoad, Average<float>& rAccel,int16_t i_start = -1, int16_t i_end = -1,float spm = 0.0) {
  if (!espnowEnabled) return;
  initESPNow(espnowMac);

  StaticJsonDocument<512> doc;
  
  doc["spm"] = spm;
  doc["fill"] = 0;
  doc["max_idx"] = i_end;
  doc["min_idx"] = i_start;
  
  // Diagnosis
  JsonArray diag = doc.createNestedArray("diagnosis");
  for (int i = 0; i < 10; i++) {
    diag.add(nnDiagnosis[i]);
  }
 
  // Últimos 50 valores de load (uint16_t = 2 bytes cada uno = 100 bytes)
  uint8_t loadBuffer[100];
  uint8_t accelBuffer[200];

  int loadCount = rLoad.getCount();
  int step = (float)loadCount / 49.0;
  for (int i = 0; i < 50 ; i++) {
    uint16_t idx = (i < 49) ? (i_start + i * step) : i_end;
    uint16_t val = rLoad.get(idx);
    loadBuffer[i*2] = val & 0xFF;
    loadBuffer[i*2 + 1] = (val >> 8) & 0xFF;

    float valA = rAccel.get(idx);
    memcpy(&accelBuffer[i*4], &valA, 4);
  }
  doc["load_b64"] = base64::encode(loadBuffer, 100);
  doc["accel_b64"] = base64::encode(accelBuffer, 200);
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  Serial.printf("Tamaño JSON: %d bytes\n", jsonStr.length());
  
  esp_err_t result = esp_now_send(peerInfo.peer_addr, 
                                   (uint8_t*)jsonStr.c_str(), 
                                   jsonStr.length());
  
  if (result != ESP_OK) {
    Serial.println("Error envío ESP-NOW");
  }
}

/* =============== LED FUNCTIONS =============== */
void ledsInit() {
  pinMode(lR, OUTPUT);
  pinMode(lG, OUTPUT);
  pinMode(lB, OUTPUT);
  digitalWrite(lR, LOW);
  digitalWrite(lG, LOW);
  digitalWrite(lB, LOW);
}

void ledBlink(char color, uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    if (color == 'R') digitalWrite(lR, HIGH);
    else if (color == 'G') digitalWrite(lG, HIGH);
    else if (color == 'B') digitalWrite(lB, HIGH);
    delay(300);
    digitalWrite(lR, LOW);
    digitalWrite(lG, LOW);
    digitalWrite(lB, LOW);
    delay(200);
  }
}

/* =============== UTILITY FUNCTIONS =============== */
float vBat() {
  digitalWrite(ADC_CTRL_PIN, LOW);
  delay(20);  // Pequeña pausa para estabilizar
  
  // Leer múltiples muestras para promediar
  uint32_t sum = 0;
  const int samples = 20;
  
  for(int i = 0; i < samples; i++) {
    sum += analogRead(pinBattery);
    delay(5);
  }
  
  float avgValue = sum / (float)samples;
  
  // Desactivar el circuito para ahorrar energía
  digitalWrite(ADC_CTRL_PIN, HIGH);
  
  // Calcular voltaje real
  // Divisor de voltaje: 390kΩ y 100kΩ
  // Ratio = (390 + 100) / 100 = 4.9
  const float ADC_RESOLUTION = 4095.0;
  const float VREF = 3.3;
  const float VOLTAGE_DIVIDER = 4.9;
  
  float voltage = (avgValue / ADC_RESOLUTION) * VREF * VOLTAGE_DIVIDER;  
  return voltage;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, "w");
  if (file) {
    file.print(message);
    file.close();
  }
}

String readFile(fs::FS &fs, const char *path) {
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) return String();
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  return fileContent;
}

void saveSetting(String value, String filename) {
  String currentValue = readFile(SPIFFS, filename.c_str());
  if (currentValue != value) {
    writeFile(SPIFFS, filename.c_str(), value.c_str());
  }
}

void saveLoRaConfig() {
  String appEuiStr = "";
  String appKeyStr = "";
  for (int i = 0; i < 8; i++) appEuiStr += String(appEui[i], HEX) + (i < 7 ? ":" : "");
  for (int i = 0; i < 16; i++) appKeyStr += String(appKey[i], HEX) + (i < 15 ? ":" : "");

  saveSetting(appEuiStr, "/appeui.txt");
  saveSetting(appKeyStr, "/appkey.txt");
}

/* =============== FUNCIÓN PARA LEER TODAS LAS CONFIGURACIONES =============== */

void loadAllConfigurations() {
  Serial.println("📂 Loading all configurations from SPIFFS...");
  
  // Sistema
  currentMode = readFile(SPIFFS, "/mode.txt");
  if (currentMode == "") currentMode = "WiFi";
  
  String sleepCycleStr = readFile(SPIFFS, "/sleep_cycle.txt");
  if (sleepCycleStr != "") {
    sleepCycleMs = sleepCycleStr.toInt() * 1000;
    sleepCycleLoaded = true;
  }

  //SENSOR
  sensorType = readFile(SPIFFS, "/sensor_type.txt");

  // WiFi
  runssid = readFile(SPIFFS, "/wifi_ssid.txt");
  runpassword = readFile(SPIFFS, "/wifi_pass.txt");
  
  // MQTT
  //mqttServer = readFile(SPIFFS, "/mqtt_server.txt");
  //mqttPort = readFile(SPIFFS, "/mqtt_port.txt");
  //if (mqttPort == "") mqttPort = "1883";
  //mqttUser = readFile(SPIFFS, "/mqtt_user.txt");
  //mqttPass = readFile(SPIFFS, "/mqtt_pass.txt");
  //mqttTopic = readFile(SPIFFS, "/mqtt_topic.txt");
  //if (mqttTopic == "") mqttTopic = "well/analyzer/data";
  
  // Optimizador
  String optimizerStr = readFile(SPIFFS, "/optimizer_enabled.txt");
  optimizerEnabled = stringToBool(optimizerStr);
  
  // ESP-NOW
  String espnowStr = readFile(SPIFFS, "/espnow_enabled.txt");
  espnowEnabled = stringToBool(espnowStr);
  espnowMac = readFile(SPIFFS, "/espnow_mac.txt");
  
  // LoRaWAN
  String appEuiStr = readFile(SPIFFS, "/appeui.txt");
  String appKeyStr = readFile(SPIFFS, "/appkey.txt");

  if (appEuiStr != "") {
    int idx = 0;
    for (int i = 0; i < 8; i++) {
      int end = appEuiStr.indexOf(':', idx);
      if (end == -1) end = appEuiStr.length();
      appEui[i] = strtol(appEuiStr.substring(idx, end).c_str(), NULL, 16);
      idx = end + 1;
    }
  }

  if (appKeyStr != "") {
    int idx = 0;
    for (int i = 0; i < 16; i++) {
      int end = appKeyStr.indexOf(':', idx);
      if (end == -1) end = appKeyStr.length();
      appKey[i] = strtol(appKeyStr.substring(idx, end).c_str(), NULL, 16);
      idx = end + 1;
    }
  }
  
  Serial.println("✅ All configurations loaded successfully!");
  printCurrentConfig();  // Opcional: imprimir configuraciones actuales
}

void printCurrentConfig() {
  Serial.println("\n📋 CURRENT CONFIGURATION:");
  Serial.println("=== SYSTEM ===");
  Serial.println("Mode: " + currentMode);
  Serial.println("Sleep Cycle: " + String(sleepCycleMs) + " ms");
  
  //Serial.println("=== WIFI ===");
  //Serial.println("SSID: " + wifiSsid);
  //Serial.println("Password: " + (wifiPass != "" ? "***" : "Not set"));
  
  //Serial.println("=== MQTT ===");
  //Serial.println("Server: " + mqttServer);
  //Serial.println("Port: " + mqttPort);
  //Serial.println("User: " + mqttUser);
  //Serial.println("Topic: " + mqttTopic);

  Serial.println("=== SENSOR ===");
  Serial.println("Sensor: " + String(sensorType == "1" ? "HT" : "PRT"));
  
  Serial.println("=== OPTIMIZER ===");
  Serial.println("Enabled: " + String(optimizerEnabled ? "Yes" : "No"));
  
  Serial.println("=== ESP-NOW ===");
  Serial.println("Enabled: " + String(espnowEnabled ? "Yes" : "No"));
  Serial.println("MAC: " + espnowMac);
  
  //Serial.println("=== LoRaWAN ===");
  //Serial.println("DevEUI: " + getDevEuiString());
  //Serial.println("AppEUI: " + getAppEuiString());
  //Serial.println("AppKey: " + getAppKeyString().substring(0, 16) + "...");
  //Serial.println("========================\n");
}

/* =============== NEURAL NETWORK FUNCTIONS =============== */
float relu(float n) {
  return (n >= 0) ? n : 0;
}

float sigmoid(float n) {
  return 1.0 / (1.0 + exp(-n));
}

void normArray(float array[], int length) {
  float minVal = array[0];
  float maxVal = array[0];

  for (int i = 1; i < length; i++) {
    if (array[i] < minVal) minVal = array[i];
    if (array[i] > maxVal) maxVal = array[i];
  }

  if (maxVal == 0 && minVal == 0) {
    for (int i = 0; i < length; i++) array[i] = 0.0;
    return;
  }

  float range = maxVal - minVal;
  for (int i = 0; i < length; i++) {
    array[i] = (array[i] - minVal) / range;
  }
}

void fillPump() {
  oledShow("Neural Network", "Calculating", "Fill Pump...");
  normArray(a0, 50);

  float W1[15][50] = { { 0.116, -0.138, 0.087, 0.401, 0.003, 0.141, 0.118, 0.451, 0.406, -0.111, 0.082, -0.231, -0.154, 0.188, 0.356, 0.108, -0.209, -0.35, -0.294, -0.064, -0.31, 0.163, 0.351, 0.049, 0.226, 0.089, 0.002, -0.236, 0.118, 0.105, 0.217, 0.22, 0.047, 0.194, 0.575, 0.568, 0.268, -0.105, 0.355, 0.208, 0.168, -0.009, 0.383, -0.242, -0.321, -0.38, 0.07, -0.357, 0.46, 0.187 }, { 0.354, -0.002, 0.153, -0.097, 0.072, -0.139, -0.295, -0.137, -0.128, 0.268, -0.052, 0.011, 0.211, 0.027, 0.092, -0.337, -0.378, -0.157, 0.043, -0.04, 0.224, -0.021, -0.076, -0.051, 0.016, -0.234, -0.299, 0.125, -0.167, -0.091, -0.084, 0.063, 0.207, -0.15, -0.307, -0.045, 0.033, -0.047, -0.1, -0.274, 0.148, -0.261, 0.102, -0.039, -0.168, 0.23, -0.09, 0.166, -0.228, -0.395 }, { -0.118, -0.106, 0.266, 0.263, -0.064, -0.246, -0.329, -0.103, 0.2, -0.157, -0.194, -0.079, -0.219, 0.072, 0.332, -0.131, -0.235, -0.303, -0.112, -0.182, -0.119, 0.031, 0.215, -0.198, 0.032, -0.007, 0.016, 0.065, 0.296, -0.08, -0.008, 0.282, -0.235, 0.083, 0.327, -0.183, -0.019, -0.192, -0.231, 0.153, -0.274, -0.135, -0.232, 0.095, -0.055, -0.346, -0.175, -0.167, -0.289, 0.11 }, { -0.138, 0.309, 0.05, 0.019, -0.123, -0.225, -0.4, -0.084, -0.078, -0.221, -0.393, 0.055, 0.511, 0.287, 0.221, 0.292, 0.231, 0.034, 0.263, 0.229, 0.277, 0.483, 0.14, 0.182, 0.44, 0.13, 0.018, -0.035, 0.061, -0.363, 0.1, -0.568, -0.176, -0.046, 0.063, -0.255, -0.237, -0.178, 0.211, 0.198, 0.298, 0.187, 0.063, 0.216, 0.622, 0.31, -0.156, 0.514, -0.158, -0.036 }, { 0.392, -0.004, -0.346, -0.111, -0.284, -0.1, 0.257, 0.187, 0.113, -0.358, -0.111, -0.074, 0.416, 0.016, 0.237, 0.433, 0.46, 0.42, 0.093, -0.037, 0.14, 0.291, 0.409, -0.082, -0.1, -0.226, 0.112, -0.187, -0.149, -0.136, -0.114, -0.516, -0.259, -0.134, -0.09, -0.248, -0.349, -0.425, -0.327, -0.292, -0.161, 0.197, 0.179, 0.278, 0.226, 0.378, 0.231, 0.255, -0.39, -0.282 }, { -0.094, 0.049, 0.083, 0.087, -0.08, 0.221, -0.263, -0.096, -0.009, 0.036, -0.347, 0.18, -0.334, -0.123, 0.24, -0.199, -0.386, 0.079, -0.333, -0.231, 0.402, -0.113, -0.097, 0.333, -0.107, 0.169, -0.285, -0.206, -0.43, 0.228, -0.024, -0.302, -0.222, 0.054, 0.132, 0.357, -0.404, 0.045, 0.084, -0.122, 0.32, -0.337, 0.226, -0.192, 0.015, -0.15, 0.091, 0.402, 0.305, -0.043 }, { -0.16, 0.219, 0.444, 0.359, 0.51, -0.203, -0.051, -0.176, 0.134, -0.037, 0.325, 0.314, -0.484, 0.103, 0.023, -0.08, 0.128, -0.18, 0.166, -0.442, -0.317, -0.404, -0.03, 0.38, 0.309, -0.096, 0.061, 0.195, 0.333, 0.251, 0.155, 0.278, 0.044, 0.252, 0.265, 0.558, 0.057, 0.009, 0.413, -0.06, -0.061, 0.133, -0.252, 0.077, -0.736, -0.367, 0.261, -0.314, 0.664, 0.186 }, { 0.308, 0.054, 0.091, -0.177, -0.179, -0.121, -0.698, -0.484, 0.063, -0.235, -0.344, 0.052, -0.288, 0.105, 0.422, 0.076, 0.351, -0.061, 0.325, 0.027, 0.101, 0.418, 0.141, -0.173, -0.241, -0.222, -0.206, -0.204, -0.421, 0.003, 0.244, 0.026, 0.064, 0.051, 0.605, 0.462, 0.243, 0.1, 0.185, 0.83, 0.663, 0.02, 0.38, 0.223, -0.162, -0.399, -0.716, -0.313, 1.197, 0.333 }, { -0.13, 0.167, -0.106, -0.533, -0.209, -0.008, 0.154, 0.024, 0.229, 0.247, -0.244, -0.109, -0.013, -0.028, 0.146, 0.344, 0.282, 0.187, 0.333, -0.21, 0.492, -0.255, 0.377, 0.338, 0.282, 0.343, -0.036, 0.297, -0.186, -0.216, -0.257, -0.2, -0.014, -0.31, -0.215, -0.59, -0.224, -0.42, -0.015, -0.196, -0.05, -0.154, -0.375, 0.113, 0.782, 0.264, -0.04, -0.079, -0.707, -0.12 }, { -0.073, 0.227, -0.274, 0.308, -0.199, 0.016, -0.155, 0.005, -0.149, -0.124, -0.227, -0.061, -0.419, 0.281, -0.18, 0.106, 0.106, -0.327, 0.197, 0.24, -0.189, -0.238, -0.197, 0.202, -0.054, -0.154, -0.008, -0.049, -0.278, 0.052, -0.332, 0.05, 0.025, -0.05, 0.286, -0.004, 0.074, -0.332, -0.112, -0.028, 0.039, 0.148, 0.019, -0.07, -0.028, 0.007, -0.317, 0.394, -0.425, 0.288 }, { 0.051, 0.265, 0.131, 0.063, 0.137, 0.256, 0.196, 0.015, -0.297, -0.096, -0.123, -0.308, -0.284, -0.341, -0.17, 0.099, -0.125, -0.059, 0.129, 0.117, -0.077, -0.016, 0.132, -0.039, 0.069, -0.025, -0.079, -0.267, 0.205, -0.202, -0.018, -0.025, -0.013, -0.149, -0.035, -0.244, -0.226, -0.002, -0.372, -0.131, 0.075, -0.138, -0.206, -0.219, -0.204, 0.185, -0.269, 0.302, -0.418, -0.067 }, { -0.215, 0.172, 0.082, 0.224, 0.026, -0.216, 0.047, -0.124, -0.126, -0.115, -0.08, 0.204, 0.219, -0.048, 0.125, -0.36, 0.292, -0.184, -0.451, -0.071, 0.157, 0.023, -0.381, 0.082, -0.192, 0.039, 0.174, -0.197, -0.178, 0.03, -0.086, 0.449, -0.234, -0.1, 0.201, 0.258, 0.15, -0.442, 0.004, 0.185, 0.025, -0.002, 0.066, -0.261, -0.202, 0.154, -0.274, -0.064, -0.437, -0.38 }, { -0.09, -0.06, -0.213, 0.097, -0.368, -0.23, -0.099, 0.24, -0.285, -0.076, -0.088, 0.355, 0.195, 0.056, -0.248, 0.061, -0.068, -0.207, -0.287, -0.107, -0.444, -0.043, 0.174, 0.371, -0.09, -0.432, 0.112, -0.038, 0.054, 0.121, 0.072, 0.104, -0.193, 0.096, -0.052, 0.035, -0.142, 0.253, -0.21, 0.355, 0.147, -0.357, -0.259, -0.346, -0.14, -0.321, -0.329, 0.183, -0.175, -0.33 }, { 0.291, 0.158, -0.186, 0.292, -0.264, -0.17, -0.051, -0.387, -0.11, -0.393, 0.042, -0.118, 0.216, 0.133, -0.189, -0.154, 0.046, -0.17, -0.208, -0.009, 0.011, -0.386, -0.147, 0.028, 0.184, 0.117, 0.225, 0.12, -0.009, 0.324, 0.633, 0.696, 0.381, 0.375, 0.369, 0.549, 0.027, 0.094, 0.001, 0.323, 0.073, -0.011, 0.463, -0.204, -0.431, -0.282, -0.001, -0.496, 0.872, 0.412 }, { 0.129, -0.175, -0.11, -0.32, -0.181, 0.422, 0.145, 0.177, 0.252, 0.277, -0.173, 0.203, 0.182, -0.03, 0.198, 0.192, 0.06, 0.009, 0.229, 0.455, 0.415, 0.127, -0.109, 0.278, 0.173, -0.032, -0.116, -0.273, -0.476, -0.006, -0.022, -0.205, 0.146, -0.256, -0.222, -0.27, 0.062, 0.13, -0.222, -0.279, 0.237, -0.089, 0.306, -0.009, 0.669, 0.443, 0.117, -0.189, -0.396, -0.123 } };
  float a1[15];
  float W2[1][15] = { { -0.325, -0.008, 0.504, 0.72, 0.614, 0.144, -0.156, -1.291, 0.525, 0.105, 0.066, -0.272, 0.217, -0.32, 0.357 } };
  float a2[1];
  float b1[15] = { 0.039, 0.0, -0.056, -0.003, -0.058, 0.0, 0.054, -0.05, -0.072, -0.03, 0.0, 0.0, -0.047, -0.038, -0.049 };
  float b2[1] = { -0.031 };

  for (int i = 0; i < 15; i++) {
    float aux = 0.0;
    for (int j = 0; j < 50; j++) aux += W1[i][j] * a0[j];
    a1[i] = relu(aux + b1[i]);
  }

  float aux = 0.0;
  for (int j = 0; j < 15; j++) aux += W2[0][j] * a1[j];
  a2[0] = sigmoid(aux + b2[0]);

  fill = constrain(a2[0], 0.0, 1.0);

  if (fill <= 0.3) ledBlink('R', 3);
  else if (fill < 0.7) ledBlink('B', 3);
  else ledBlink('G', 3);

  oledShow("Fill Pump", String(fill * 100, 1) + "%");
  Serial.println("fill: " + String(fill * 100, 1));
  delay(2000);
}

void diagnosis() {
  oledShow("Neural Network", "Running", "Diagnosis...");
  normArray(a0, 50);

  float W1[25][50] = { { 0.0755, 0.0329, -0.1383, 0.0467, 0.2215, -0.0807, 0.2073, 0.0523, -0.2558, 0.3212, 0.1519, 0.2436, -0.2785, -0.0971, -0.1351, -0.3684, -0.0904, 0.2216, 0.2327, 0.3437, -0.3905, -0.127, -0.2218, 0.1722, -0.2197, 0.2917, -0.3835, -0.2932, -0.4477, -0.3297, -0.2362, -0.274, 0.0441, -0.0975, -0.0871, 0.2418, -0.1267, 0.1557, -0.0539, 0.0045, -0.1237, -0.0709, 0.0944, -0.2018, 0.2882, 0.0049, 0.3462, -0.1692, -0.3792, -0.3468 }, { 0.3023, 0.0471, -0.2249, 0.1239, -0.0611, 0.0955, -0.2201, 0.0417, 0.2335, -0.2862, -0.0289, -0.2307, 0.1427, 0.1455, 0.1616, -0.2997, -0.1082, 0.2647, 0.3304, -0.0866, -0.2222, -0.1856, 0.0172, -0.2344, -0.299, -0.155, -0.1114, 0.0715, 0.0313, 0.1916, -0.0029, 0.0943, -0.0548, -0.3695, 0.0079, 0.0196, 0.1295, 0.0933, 0.133, -0.3309, 0.1026, 0.1639, -0.0351, -0.0999, 0.0338, -0.0656, -0.4221, 0.3636, 0.154, -0.0168 }, { 1.0177, 0.727, -0.1868, -0.1838, 0.2109, 0.3648, 0.4496, 0.3975, 0.3684, 0.3219, 0.5521, 0.1086, 0.3524, -0.1866, -0.3407, -0.2194, -0.3056, -0.0003, 0.371, 0.147, 0.2744, 0.2572, 0.0138, 0.0922, -0.0749, 0.2112, 0.2911, 0.1899, -0.1323, -0.7407, -0.0805, 0.1761, -0.1075, -0.4332, -0.6121, -0.796, -1.2058, -1.5349, -1.0672, 0.0239, 0.35, 0.2807, 0.4927, 0.3602, 0.377, 0.7621, 1.223, 1.7834, 1.4467, 1.8351 }, { -0.1916, 0.0462, 0.0641, 0.0659, -0.2078, 0.1385, -0.2622, -0.115, -0.3187, -0.0805, -0.1458, -0.002, -0.3965, -0.1526, -0.0672, 0.2721, 0.1177, 0.3865, 0.1642, 0.2584, -0.0934, 0.2141, -0.0966, -0.2232, -0.2005, -0.2398, -0.0494, 0.0995, 0.0607, -0.0569, 0.3283, -0.1416, -0.0545, 0.0295, -0.2874, -0.0427, -0.1848, 0.0823, -0.1121, -0.1415, 0.0683, 0.305, 0.0226, 0.2569, -0.286, -0.2612, 0.248, 0.0957, 0.076, -0.0247 } };

  float a1[25];
  float W2[10][25] = { { -0.1031, 0.3084, 0.3811, -0.0225, -0.1858, 0.1278, 0.0795, 0.0282, -0.2332, -1.1754, -0.0369, -1.3671, 0.3673, -0.1881, 0.1472, 0.1455, 0.0785, -0.0433, 0.1416, -0.372, -0.3188, 0.2789, 0.214, -0.1542, 1.5634 }, { 0.0443, -0.3823, -0.4901, -0.1283, 0.2888, 0.0406, -0.1446, 0.0255, 0.0692, 0.2413, 0.0781, -0.4461, -0.1725, 0.2786, -0.2767, 0.0683, 0.2654, -0.2179, 0.2649, 0.0232, -0.36, -0.21, 0.0528, -0.3715, -0.5914 }, { 0.1982, -0.0184, 0.5164, -0.2346, -0.082, 0.0056, -0.2026, -0.2509, -0.3108, -0.728, 0.3812, 0.8671, 0.2138, 0.1633, -0.1394, -0.8898, -0.1345, -0.4291, 0.1149, -0.0816, 0.5426, -0.3568, -0.0547, -2.1485, -0.1517 }, { -0.3702, -0.2469, 0.1363, 0.3833, -0.3955, 0.1444, -0.1032, 0.3464, -0.0115, -0.4733, 0.3507, -0.224, 0.1542, 0.1358, 0.0464, -0.2685, 0.0279, -0.4915, 0.1423, 0.3241, -0.3724, 0.2916, 0.1211, 0.0354, -0.3495 }, { -0.4453, -0.333, -0.1574, 0.3398, 0.1887, -0.2173, 0.0825, -0.1112, -0.3296, 1.9248, -0.3813, -0.4052, 0.2862, 0.0551, -0.2428, -0.3354, 0.1683, 0.6345, -0.2297, 0.2433, 0.0421, 0.201, -0.4071, -1.594, -0.9756 }, { 0.3324, -0.3916, -1.1234, 0.2569, 0.0064, 0.1419, 0.1342, 0.0897, -0.3948, 1.3064, 0.325, 0.3112, -0.2327, 0.0064, 0.0513, -0.5443, 0.2272, 0.1591, 0.0902, -0.3392, -0.1557, -0.3778, -0.3804, 0.5214, -0.8373 }, { -0.3731, -0.1139, -0.8278, 0.2134, 0.2422, 0.3411, 0.0141, 0.0406, -0.2871, -1.8052, -0.3215, 0.6934, 0.4407, -0.0267, -0.2732, 0.532, -0.1755, -0.6483, 0.18, 0.1885, -0.3026, -0.2248, 0.1679, 0.3409, -0.6114 }, { -0.2094, 0.3624, 0.0363, 0.0143, 0.0158, -0.2953, -0.2231, 0.3273, 0.351, 1.4159, -0.2999, 0.6529, 0.194, -0.1065, -0.1346, -0.8931, 0.0372, -2.1091, 0.2221, 0.3474, 0.3417, 0.3812, 0.0586, 0.0476, 0.737 }, { 0.0244, -0.2174, 0.1832, -0.0784, 0.0015, 0.398, -0.4087, -0.0158, -0.35, -0.4786, -0.202, 1.4981, 0.1442, 0.1404, 0.3427, -0.6367, 0.158, 0.6273, -0.3482, -0.2237, -0.7691, 0.3733, 0.2811, -0.0207, -0.0722 }, { 0.3994, 0.0027, -0.4982, -0.1072, -0.191, 0.3065, 0.2842, 0.2721, -0.1214, 2.0278, 0.2881, -0.8576, -0.4596, -0.2342, -0.0694, -0.1087, -0.3479, -0.5115, 0.3012, -0.2754, -0.8987, 0.0134, -0.091, 0.6855, 1.2987 } };

  float a2[10];
  float b1[25] = { -0.0901, 0.0, 0.3984, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.0278, 0.0, -0.002, -0.0235, -0.0757, -0.0436, 0.304, -0.0285, 0.8204, 0.0, 0.0, 0.1211, 0.0, -0.0587, 0.2522, -0.0529 };
  float b2[10] = { -0.1843, -0.1281, -0.4104, -0.1167, -0.2755, -0.2255, -0.2435, -0.3616, 0.0797, -0.1318 };

  for (int i = 0; i < 25; i++) {
    float aux = 0.0;
    for (int j = 0; j < 50; j++) aux += W1[i][j] * a0[j];
    a1[i] = relu(aux + b1[i]);
  }

  for (int i = 0; i < 10; i++) {
    float aux = 0.0;
    for (int j = 0; j < 25; j++) aux += W2[i][j] * a1[j];
    nnDiagnosis[i] = sigmoid(aux + b2[i]);
  }

  oledShow("Diagnosis", "Complete!", "", "Check results");
  delay(2000);
}

/* =============== HTTP CONFIG =============== */
void getHttpConfig() {
  oledShow("WiFi", "Getting config", "from server...");

  String brokerString = readFile(SPIFFS, "/mqtt_server.txt");

  HTTPClient http;
  uint64_t chipId = ESP.getEfuseMac();
  String url = "http://" + brokerString + ":80" + SearchPath + String(chipId, HEX);

  if (WiFi.status() == WL_CONNECTED) {
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;

      if (!deserializeJson(doc, payload)) {
        runNN = doc["RunNNdevice"] | true;
        appTxDutyCycle = (doc["SamplingRate"] | 300) * 1000;
        oledShow("Config OK", "NN: " + String(runNN ? "ON" : "OFF"), "Rate: " + String(appTxDutyCycle / 1000) + "s");
        ledBlink('G', 2);
      }
    }
    http.end();
  }
  delay(2000);
}

/* =============== FUNCIÓN PARA CARGAR SLEEP CYCLE UNA SOLA VEZ =============== */
void loadSleepCycleOnce() {
  // Si ya se cargó, no hacer nada
  if (sleepCycleLoaded) {
    Serial.println("Sleep cycle already loaded: " + String(sleepCycleMs / 1000) + "s");
    return;
  }

  String sleepStr = readFile(SPIFFS, "/sleep_cycle.txt");

  if (sleepStr != "" && sleepStr.length() > 0) {
    // Eliminar espacios en blanco
    sleepStr.trim();

    uint32_t sleepSeconds = strtoul(sleepStr.c_str(), NULL, 10);

    // Validar rango (10 segundos a 24 horas)
    if (sleepSeconds >= 10 && sleepSeconds <= 86400) {
      sleepCycleMs = sleepSeconds * 1000;  // Convertir a milisegundos
      sleepCycleLoaded = true;

      Serial.println("✓ Sleep cycle loaded from SPIFFS: " + String(sleepSeconds) + "s (" + String(sleepCycleMs) + "ms)");
      oledShow("Sleep Config", "Loaded!", String(sleepSeconds) + " seconds");
      delay(1000);
      return;
    } else {
      Serial.println("⚠ Invalid sleep value in file: " + String(sleepSeconds) + "s");
    }
  } else {
    Serial.println("⚠ Sleep cycle file empty or not found");
  }

  // Si llegamos aquí, usar valor por defecto y guardarlo
  sleepCycleMs = 300000;  // 5 minutos
  sleepCycleLoaded = true;

  saveSetting("300", "/sleep_cycle.txt");
  Serial.println("✓ Using default sleep cycle: 300s (saved to SPIFFS)");
  oledShow("Sleep Config", "Default", "300 seconds");
  delay(1000);
}

/* =============== FUNCIÓN PARA OBTENER SLEEP CYCLE (CON CARGA AUTOMÁTICA) =============== */
uint32_t getSleepCycleMs() {
  // Cargar si no se ha hecho todavía
  if (!sleepCycleLoaded) {
    loadSleepCycleOnce();
  }

  return sleepCycleMs;
}

/* =============== WIFI TX =============== */
void wifiTx() {
  runssid = readFile(SPIFFS, "/wifi_ssid.txt");
  runpassword = readFile(SPIFFS, "/wifi_pass.txt");

  oledShow("WiFi Mode", "Connecting...", runssid);
  ledBlink('B', 2);

  int attempts = 0;
  WiFi.begin(runssid.c_str(), runpassword.c_str());

  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
    oledShow("WiFi", "Connecting...", "Attempt: " + String(attempts));
  }

  if (WiFi.status() != WL_CONNECTED) {
    oledShow("WiFi", "Failed!", "Restarting...");
    ledBlink('R', 3);
    delay(2000);
    ESP.restart();
  }

  String brokerString = readFile(SPIFFS, "/mqtt_server.txt");
  String mqtt_potString = readFile(SPIFFS, "/mqtt_port.txt");

  oledShow("WiFi", "Connected!", WiFi.localIP().toString());
  client.setServer(brokerString.c_str(), mqtt_potString.toInt());
  getHttpConfig();

  JsonDocument doc;
  uint64_t chipId = ESP.getEfuseMac();

  doc["type"] = "wellAnalizer";
  doc["devEUI"] = String(chipId, HEX);
  doc["vBat"] = String(vBat(), 2);
  doc["sLength"] = String(sLength, 2);
  doc["status"] = status;

  if (status == 1 && integrated == 1 && runNN) {
    fillPump();
    diagnosis();

    doc["fillPump"] = String(fill * 100, 2);

    String sDiag = "";
    for (uint8_t i = 0; i < 9; i++) sDiag += String(nnDiagnosis[i], 3) + ",";
    sDiag += String(nnDiagnosis[9], 3);
    doc["diagnosis"] = sDiag;

    String sLoad = "";
    for (uint8_t i = 0; i < 49; i++) sLoad += String(load50.get(i)) + ",";
    sLoad += String(load50.get(49));
    doc["load"] = sLoad;
  }

  String payload;
  serializeJson(doc, payload);

  oledShow("MQTT", "Connecting...", broker);
  attempts = 0;

  while (!client.connected() && attempts < 5) {
    String clientId = "esp32-" + String(chipId, HEX);
    if (client.connect(clientId.c_str())) {
      oledShow("MQTT", "Connected!", "Publishing...");
      client.publish(topicPublish, payload.c_str());

      uint32_t sleepMs = getSleepCycleMs();

      oledShow("MQTT", "Data sent!", "Sleep: " + String(sleepMs / 1000) + "s");
      Serial.println("✓ MQTT data published. Entering sleep for " + String(sleepMs / 1000) + "s");
      ledBlink('G', 3);
    } else {
      attempts++;
      oledShow("MQTT", "Retry: " + String(attempts), "State: " + String(client.state()));
      delay(2000);
    }
  }

  if (!client.connected()) {
    oledShow("MQTT", "Failed!", "Restarting...");
    ledBlink('R', 3);
    delay(2000);
    ESP.restart();
  }
}

/* =============== LORA TX =============== */
static void prepareTxFrame(uint8_t port) {
  oledShow("LoRa", "Preparing", "payload...");
  appDataSize = 0;

  int16_t vBatInt = (int16_t)(vBat() * 100);

  appData[appDataSize++] = (vBatInt >> 8) & 0xFF;
  appData[appDataSize++] = vBatInt & 0xFF;
  appData[appDataSize++] = status;

  if (status == 1 && integrated == 1) {
    fillPump();
    diagnosis();

    int16_t spmInt = (int16_t)(spm * 10);
    appData[appDataSize++] = (spmInt >> 8) & 0xFF;
    appData[appDataSize++] = spmInt & 0xFF;

    int16_t sLengthInt = (int16_t)(sLength * 100);
    appData[appDataSize++] = (sLengthInt >> 8) & 0xFF;
    appData[appDataSize++] = sLengthInt & 0xFF;

    int16_t fillInt = (int16_t)(fill * 100);
    appData[appDataSize++] = (fillInt >> 8) & 0xFF;
    appData[appDataSize++] = fillInt & 0xFF;

    for (uint8_t i = 0; i < 10; i++) {
      int16_t diag = (int16_t)(nnDiagnosis[i] * 100);
      appData[appDataSize++] = (diag >> 8) & 0xFF;
      appData[appDataSize++] = diag & 0xFF;
    }

    for (uint8_t i = 0; i < 20; i++) {
      int16_t tData = (int16_t)(load20.get(i));
      appData[appDataSize++] = (tData >> 8) & 0xFF;
      appData[appDataSize++] = tData & 0xFF;
    }
  }

  oledShow("LoRa", "Payload ready", "Size: " + String(appDataSize) + " bytes");
  delay(1000);
}

/* =============== MAIN PLOT =============== */

void displaySimpleGraphs(Average<uint16_t>& rLoad, Average<float>& rAccel, uint16_t ndata) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  
  const uint8_t GRAPH_WIDTH = 95;  // Espacio para gráfica
  const uint8_t STATS_X = 98;       // Inicio de zona de estadísticas
  const uint8_t GRAPH_HEIGHT = 28;
  
  // ========== MITAD SUPERIOR: ACELERACIÓN ==========
  const uint8_t ACCEL_Y = 2;
  
  // Título
  display.drawString(2, 0, "ACCEL");
  
  // Calcular min/max de aceleración (los datos YA están en mili-g)

  int maxIdx, minIdx;
  float maxAccel = rAccel.maximum(&maxIdx);
  float minAccel = rAccel.minimum(&minIdx);
  
  float accelRange = maxAccel - minAccel;
  if (accelRange < 0.01) accelRange = 0.01;
  
  // DEBUG: Imprimir valores para verificar
  Serial.println("=== ACCEL DEBUG ===");
  Serial.println("Min: " + String(minAccel) + " Max: " + String(maxAccel) + " Range: " + String(accelRange));
  
  // Dibujar gráfica de aceleración
  for (uint16_t x = 0; x < GRAPH_WIDTH - 1; x++) {
    uint16_t idx1 = (long)x * (ndata - 1) / (GRAPH_WIDTH - 1);
    uint16_t idx2 = (long)(x + 1) * (ndata - 1) / (GRAPH_WIDTH - 1);
    
    float val1 = rAccel.get(idx1);  // Ya en mili-g
    float val2 = rAccel.get(idx2);
    
    // Ajustar al rango real min-max
    val1 = constrain(val1, minAccel, maxAccel);
    val2 = constrain(val2, minAccel, maxAccel);
    
    uint8_t y1 = ACCEL_Y + GRAPH_HEIGHT - 
                 (uint8_t)((val1 - minAccel) * GRAPH_HEIGHT / accelRange);
    uint8_t y2 = ACCEL_Y + GRAPH_HEIGHT - 
                 (uint8_t)((val2 - minAccel) * GRAPH_HEIGHT / accelRange);
    
    display.drawLine(x, y1, x + 1, y2);
  }
  
  // Línea separadora vertical
  display.drawVerticalLine(GRAPH_WIDTH + 1, 0, 64);
  
  // Valores a la derecha (aceleración) - mostrar en m/s²
  display.drawString(STATS_X, 8, String(maxAccel / 1000.0, 1));
  display.drawString(STATS_X, 20, String(minAccel / 1000.0, 1));
  
  // ========== LÍNEA DIVISORIA HORIZONTAL ==========
  display.drawHorizontalLine(0, 32, GRAPH_WIDTH);
  
  // ========== MITAD INFERIOR: CARGA ==========
  const uint8_t LOAD_Y = 34;
  
  // Título
  display.drawString(2, 32, "LOAD");
  
  // Calcular min/max de carga
  int maxLoadIdx, minLoadIdx;
  float minLoad = rLoad.minimum(&minLoadIdx);
  float maxLoad = rLoad.maximum(&maxLoadIdx);
  float loadRange = maxLoad - minLoad;
  if (loadRange < 1) loadRange = 1;
  
  // Dibujar gráfica de carga
  for (uint16_t x = 0; x < GRAPH_WIDTH - 1; x++) {
    uint16_t idx1 = (long)x * (ndata - 1) / (GRAPH_WIDTH - 1);
    uint16_t idx2 = (long)(x + 1) * (ndata - 1) / (GRAPH_WIDTH - 1);
    
    float val1 = constrain((float)rLoad.get(idx1), minLoad, maxLoad);
    float val2 = constrain((float)rLoad.get(idx2), minLoad, maxLoad);
    
    uint8_t y1 = LOAD_Y + GRAPH_HEIGHT - 
                 (uint8_t)((val1 - minLoad) * GRAPH_HEIGHT / loadRange);
    uint8_t y2 = LOAD_Y + GRAPH_HEIGHT - 
                 (uint8_t)((val2 - minLoad) * GRAPH_HEIGHT / loadRange);
    
    display.drawLine(x, y1, x + 1, y2);
  }
  
  // Valores a la derecha (carga)
  display.drawString(STATS_X, 40, String((int)maxLoad));
  display.drawString(STATS_X, 52, String((int)minLoad));
  
  display.display();
  delay(5000);
}

void displayCycleGraphs(Average<int16_t>& load50, Average<int16_t>& acc50, int numPoints) {
  display.clear();
  
  const int screenWidth = 128;
  const int screenHeight = 64;
  const int halfWidth = screenWidth / 2;  // 64 pixels para cada lado
  const int halfHeight = screenHeight / 2; // 32 pixels para cada mitad vertical
  
  // ===== LADO DERECHO: Scatter X vs Load =====
  // Generar onda senoidal para posición X (de 0 a numPoints-1)
  int maxLoadIdx, minLoadIdx;
  float minLoad = load50.minimum(&minLoadIdx);
  float maxLoad = load50.maximum(&maxLoadIdx);
  float meanLoad = load50.mean();
  
  // Evitar división por cero
  float loadRange = maxLoad - minLoad;
  if (loadRange < 0.1) loadRange = 1.0;
  
  for (int i = 0; i < numPoints; i++) {
    // Posición X como senoidal: comenzando desde mínimo (0)
    // sin(0) = 0, sin(π/2) = 1, sin(π) = 0, sin(3π/2) = -1, sin(2π) = 0
    // Para comenzar desde mínimo usamos: sin(x - π/2) que empieza en -1
    float angle = (float)i / (numPoints - 1) * 2.0 * PI - PI/2.0;
    float sinVal = sin(angle); // Rango [-1, 1]
    
    // Mapear sinVal de [-1, 1] a [0, halfWidth-1]
    int xPos = halfWidth + (int)((sinVal + 1.0) / 2.0 * (halfWidth - 2)) + 1;
    
    // Mapear carga al eje Y (invertido porque Y crece hacia abajo)
    float loadVal = load50.get(i);
    int yPos = (int)((maxLoad - loadVal) / loadRange * (screenHeight - 2)) + 1;
    
    // Dibujar punto
    display.setPixel(xPos, yPos);
  }
  
  // Línea divisoria vertical en x=64
  for (int y = 0; y < screenHeight; y += 2) {
    display.setPixel(halfWidth, y);
  }
  
  // ===== LADO IZQUIERDO SUPERIOR: Aceleración =====
  int maxAccIdx, minAccIdx;
  float minAcc = acc50.minimum(&minAccIdx);
  float maxAcc = acc50.maximum(&maxAccIdx);
  
  float accRange = maxAcc - minAcc;
  if (accRange < 0.1) accRange = 1.0;
  
  for (int i = 0; i < numPoints - 1; i++) {
    // Mapear índice a coordenada X
    int x1 = (int)((float)i / (numPoints - 1) * (halfWidth - 2)) + 1;
    int x2 = (int)((float)(i + 1) / (numPoints - 1) * (halfWidth - 2)) + 1;
    
    // Mapear aceleración a coordenada Y (superior)
    float accVal1 = acc50.get(i);
    float accVal2 = acc50.get(i + 1);
    int y1 = (int)((maxAcc - accVal1) / accRange * (halfHeight - 2)) + 1;
    int y2 = (int)((maxAcc - accVal2) / accRange * (halfHeight - 2)) + 1;
    
    // Dibujar línea conectando puntos
    display.drawLine(x1, y1, x2, y2);
  }
  
  // ===== LADO IZQUIERDO INFERIOR: Carga =====
  for (int i = 0; i < numPoints - 1; i++) {
    // Mapear índice a coordenada X
    int x1 = (int)((float)i / (numPoints - 1) * (halfWidth - 2)) + 1;
    int x2 = (int)((float)(i + 1) / (numPoints - 1) * (halfWidth - 2)) + 1;
    
    // Mapear carga a coordenada Y (inferior, offset por halfHeight)
    float loadVal1 = load50.get(i);
    float loadVal2 = load50.get(i + 1);
    int y1 = halfHeight + (int)((maxLoad - loadVal1) / loadRange * (halfHeight - 2)) + 1;
    int y2 = halfHeight + (int)((maxLoad - loadVal2) / loadRange * (halfHeight - 2)) + 1;
    
    // Dibujar línea conectando puntos
    display.drawLine(x1, y1, x2, y2);
  }
  
  // Línea divisoria horizontal en y=32
  for (int x = 0; x < halfWidth; x += 2) {
    display.setPixel(x, halfHeight);
  }
  
  display.display();
}

/* =============== CALCULO DE ALTURA =============== */
float calLenght(Average<float>& rAccel,int i_start,int i_end,float sample_time_ms) {

  int num_samples = i_end - i_start;
  float accel_promedio;
  for(int i = i_start; i < i_end + 1; i++){
    accel_promedio += rAccel.get(i);
  }
  accel_promedio /= num_samples;
  
  const float GRAVEDAD = 9810.0; // mm/s² (9.81 m/s²)
  float dt = sample_time_ms / 1000.0; // Convertir a segundos
  
  // Variables para integración
  float velocidad = 0.0; // mm/s
  float posicion = 0.0; // mm
  float altura_maxima = 0.0; // mm
   
  // Procesar cada muestra
  for (int i = i_start; i < i_end + 1; i++) {
    
    // Restar gravedad para obtener aceleración neta
    // Usamos el promedio como referencia de gravedad
    float accel_neta = rAccel.get(i) - accel_promedio;
    
    // Integración de aceleración para obtener velocidad
    velocidad += accel_neta * dt;
    
    // Integración de velocidad para obtener posición
    posicion += velocidad * dt;
    
    // Guardar altura máxima
    if (posicion > altura_maxima) {
      altura_maxima = posicion;
    }
  }
  
  // Convertir de mm a cm
  return altura_maxima / 10.0;
}

/* ************************* =============== MAIN TASK =============== ************************* */
void mainTask() {
  oledShow("Main Task", "Starting", "acquisition...");
  ledBlink('B', 1);

  Average<uint16_t> rLoad(nData);
  Average<float> rAccel(nData);  // Array para aceleración

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float fAcc = a.acceleration.x;
  uint16_t fLoad = scale.get_value(5) * 0.01;
  
  uint16_t blLoad = 0;
  float blAcc = 0.0;

  // Parámetros de detección
  const float ALPHA_L = 0.05;  // Factor de suavizado load 0.03
  const float ALPHA_A = 0.08;  // Factor de suavizado aceleracion 0.08

  // ========== FASE 1: ADQUISICIÓN ==========
  oledShow("Acquiring", "Calculating", "baseline...");

  // Calcular baseline (primeros 50 samples)
  for (uint16_t i = 0; i < 50; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float accel_x = a.acceleration.x;
    fAcc = ALPHA_A * accel_x + (1.0 - ALPHA_A) * fAcc;
    blAcc += fAcc;

    fLoad = (ALPHA_L * scale.get_value(5) * 0.01) + (1.0 - ALPHA_L) * fLoad;
    blLoad += fLoad;
    delay(15);
  }

  blAcc /= 50.0;
  blLoad /= 50;

  oledShow("Baseline", "Calculated", String(blAcc, 2), String(blLoad));
  if(!optimizerEnabled) delay(500);

  // Adquisición principal
  long startTime = millis();
  long acctime = 0;

  for (uint16_t i = 0; i < nData; i++) {
    // Leer carga
    fLoad = (ALPHA_L * scale.get_value(5) * 0.01) + (1.0 - ALPHA_L) * fLoad;
    rLoad.push(fLoad);

    // Leer aceleración en X
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float accel_x = a.acceleration.x;
    fAcc = ALPHA_A * accel_x + (1.0 - ALPHA_A) * fAcc;

    rAccel.push(fAcc * 1000.0);

    if (i % 20 == 0) {
      oledShow("Acquiring", String(i) + "/" + String(nData),"L: " + String(fLoad) + " A: " + String(fAcc, 2));
    }
    delay(15);

  }

  long elapsedTime = millis() - startTime;
  oledShow("Acquisition", "Complete!", "Time: " + String(elapsedTime) + "ms");
  ledBlink('B', 2);
  if(!optimizerEnabled) delay(500);

  // GRAFICO principal
  displaySimpleGraphs(rLoad,rAccel,nData);
  //sendESPNowData(rLoad,rAccel);

  // ========== FASE 2: PROCESAMIENTO - VERIFICAR SI ESTÁ DETENIDO ==========
  oledShow("Processing", "Checking", "motion...");

  // Calcular desviación estándar de aceleración
  float accelStdDev = 0.0;
  accelStdDev = rAccel.stddev();

  // Verificar rango de carga
  int maxIdx, minIdx;
  float maxAccel = rAccel.maximum(&maxIdx);
  float minAccel = rAccel.minimum(&minIdx);
  float range = maxAccel - minAccel;

  Serial.println(String(range) + " ** "+ String(accelStdDev));

  // DETENIDO si: bajo rango de carga O baja variación de aceleración
  if (range <= 100 || accelStdDev < 60) {  // 100 mili-g de umbral
    status = 0;
    oledShow("Status STOPPED",
      "Acc max: " + String(maxAccel, 1),
      "Acc-min: " + String(minAccel, 1),
      "max-min: " + String(range, 1),
      "aStdDev: " + String(accelStdDev, 2));
    ledBlink('R', 3);
    Serial.println("Status: STOPPED - Low activity");
    return;
  }

  status = 1;
  oledShow("Status", "RUNNING", "Detecting", "cycle...");

  // ========== FASE 3: DETECCIÓN DE CICLO (PICO-VALLE-PICO) ==========

  float value;
  int16_t i_start = 0;
  int16_t i_flag = 0;
  int16_t i_end = 0;
  float tp = 0;
  float diff;
  float tol = 0.8;

  for (int16_t i = 0; i < nData - 1; i++)
  {
    value = rAccel.get(i);
    diff = value - minAccel;
    if (diff > tol * range && i_flag == 0 && i_end == 0)
    {
      if (diff > tp)
      {
        tp = diff;
        i_start = i;
      }
    }
    else if (i_start != 0 && diff < (1-tol) * range && i_end == 0)
    {
      if (diff < tp)
      {
        tp = diff;
        i_flag = i;
      }
    }
    else if (i_flag != 0 && diff > tol * range)
    {
      if (diff > tp)
      {
        tp = diff;
        i_end = i;
      }
    }
    else if (diff < (1-tol) * range && i_end != 0)
    {
      break;
    }
  }

  Serial.println("TODO:");
  for (int16_t i = 0; i < nData - 1; i++){
    Serial.print(String(rAccel.get(i)) + ",");
  }

  Serial.println("acc: " + String(maxAccel) + "," + String(minAccel) + "; mmin index " + String(i_start) + " min index: " + String(i_end));

  // Verificar si se encontró un ciclo válido
  if (i_start <= 0 || i_flag <= 0 || i_end <= 0) {
    integrated = 0;
    oledShow("Cycle", "INCOMPLETE", "Start:" + String(i_start), "End:" + String(i_end));
    ledBlink('R', 2);
    Serial.println("Incomplete cycle - No peak-valley-peak found");
    return;
  }

  // Verificar duración del ciclo (8-20 segundos a 15ms/muestra = 533-1333 muestras)
  int cycleSamples = i_end - i_start;
  float meanRead = elapsedTime / nData;
  if (cycleSamples < 50 || cycleSamples > 300) {
    oledShow("Cycle", "INVALID", "Duration:", String(cycleSamples * meanRead,2) + "ms");
    ledBlink('R', 2);
    Serial.println("Invalid cycle duration " + String(cycleSamples * meanRead, 2) + "ms");
    return;
  }

  // ========== FASE 4: CÁLCULO DE PARÁMETROS ==========
  spm = 60000.0 / (cycleSamples * meanRead);  // Strokes por minuto

  sLength = calLenght(rAccel,i_start,i_end,meanRead);
  Serial.println("altura: " + String(sLength));

  oledShow("Cycle OK",
    "SPM: " + String(spm, 1),
    "range: " + String(sLength, 1),
    "Samples: " + String(cycleSamples));
  if(!optimizerEnabled) delay(1000);

  // ========== FASE 5: REDIMENSIONAR A 50 Y 20 PUNTOS ==========

  Average<int16_t> acc50(50);

  // ***** 50 POINTS *****
  if(sensorType == "1"){
    /* HT */
    Serial.println("HT50 RAW LOAD:");
    float step = float(cycleSamples -1) / 49.0;
    for (uint16_t i = 0; i < 50; i++) {
      uint16_t idx = (i < 49) ? (i_start + i * step) : i_end;
      load50.push(rLoad.get(idx));
      Serial.print(String(rLoad.get(idx)) + ",");
      a0[i] = rLoad.get(idx);
      acc50.push(rAccel.get(idx));
    }
  }else{
    /* PRT */
    Serial.println("PRT50 RAW LOAD:");
    Average<int16_t> temLoad(50);

    float step = float(cycleSamples -1) / 49.0;
    for (uint16_t i = 0; i < 50; i++) {
      uint16_t idx = (i < 49) ? (i_start + i * step) : i_end;
      temLoad.push(rLoad.get(idx));
      Serial.print(String(rLoad.get(idx)) + ",");
      acc50.push(rAccel.get(idx));
    }
    Serial.println();

    int maxIdxL, minIdxL;
    int16_t maxLoad = temLoad.maximum(&maxIdxL);
    int16_t minLoad = temLoad.minimum(&minIdxL);

    uint16_t rangoLoad = maxLoad - minLoad;
    float factor = 1000/ (float)rangoLoad;

    // procesamiento para sesnor por deformacion axial
    Serial.println("PRT50 PROCESS LOAD:");
    for (int i = 0; i < 50; i++) {
      int16_t tempp = ((maxLoad - temLoad.get(i)) * 1000) / rangoLoad;
      //load50.push(tempp);
      load50.push(tempp);
      a0[i] = tempp;
      Serial.print(String(tempp) + ",");
    }
  }

  /* 20 */
  Serial.println("PRT20 RAW LOAD:");
  float step = 50 / 19.0;
  for (uint16_t i = 0; i < 20; i++) {
    uint16_t idx = (i < 19) ? (i * step) : 49;
    Serial.print(String(load50.get(idx)) + ",");
    load20.push(load50.get(idx));
  }

  oledShow("Processing", "COMPLETE!", "SPM: " + String(spm, 1), "Ready to TX");
  ledBlink('G', 2);
  Serial.println();
  Serial.println("=== CYCLE SUMMARY ===");
  Serial.println("Start: " + String(i_start) + " Valley: " + String(i_flag) + " End: " + String(i_end));
  Serial.println("Duration: " + String(cycleSamples * meanRead) + "ms");
  Serial.println("SPM: " + String(spm, 1));
  Serial.println("Length: " + String(sLength, 1));

  if(!optimizerEnabled) delay(2000);

  displayCycleGraphs(load50, acc50, 50);
  if(!optimizerEnabled) delay(2000);

}

/* =============== WEB SERVER HANDLERS =============== */
String getDevEuiString() {
  uint64_t chipid = ESP.getEfuseMac();
  // Extraer los 6 bytes del ChipID
  uint8_t mac[6];
  mac[0] = (chipid >> 40) & 0xFF;  // bc
  mac[1] = (chipid >> 32) & 0xFF;  // 2f
  mac[2] = (chipid >> 24) & 0xFF;  // 67
  mac[3] = (chipid >> 16) & 0xFF;  // fa
  mac[4] = (chipid >> 8) & 0xFF;   // 12
  mac[5] = chipid & 0xFF;          // f4

  // Algoritmo Heltec: [mac[2], mac[3], mac[4], mac[5], 0x00, 0x00, mac[0], mac[1]]
  devEui[0] = mac[2];  // 67
  devEui[1] = mac[3];  // FA
  devEui[2] = mac[4];  // 12
  devEui[3] = mac[5];  // F4
  devEui[4] = 0x00;    // 00
  devEui[5] = 0x00;    // 00
  devEui[6] = mac[0];  // BC
  devEui[7] = mac[1];  // 2F

  String result = "";
  for (int i = 0; i < 8; i++) {
    if (devEui[i] < 16) result += "0";
    result += String(devEui[i], HEX);
    if (i < 7) result += ":";
  }
  result.toUpperCase();
  return result;
}

String getAppEuiString() {
  String result = "";
  for (int i = 0; i < 8; i++) {
    if (appEui[i] < 16) result += "0";
    result += String(appEui[i], HEX);
    if (i < 7) result += ":";
  }
  result.toUpperCase();
  return result;
}

String getAppKeyString() {
  String result = "";
  for (int i = 0; i < 16; i++) {
    if (appKey[i] < 16) result += "0";
    result += String(appKey[i], HEX);
    if (i < 15) result += ":";
  }
  result.toUpperCase();
  return result;
}

bool stringToBool(String value) {
  value.toLowerCase();
  value.trim();
  return (value == "true" || value == "1" || value == "on" || value == "yes" || value == "enabled");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>Well Analyzer Config</title><style>*{margin:0;padding:0;box-sizing:border-box}";
  html += "body{font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px}";
  html += ".container{max-width:1000px;margin:0 auto;background:white;border-radius:16px;box-shadow:0 20px 60px rgba(0,0,0,0.3);overflow:hidden}";
  html += ".header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:30px;text-align:center}";
  html += ".header h1{font-size:28px;margin-bottom:8px}.header p{opacity:0.9;font-size:14px}";
  html += ".content{padding:30px}.section{margin-bottom:30px}";
  html += ".section h2{color:#333;font-size:18px;margin-bottom:15px;padding-bottom:10px;border-bottom:2px solid #667eea}";
  html += ".info-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin-bottom:20px}";
  html += ".info-card{background:#f8f9fa;padding:15px;border-radius:10px;border-left:4px solid #667eea}";
  html += ".info-card h3{font-size:12px;color:#666;margin-bottom:5px;text-transform:uppercase;letter-spacing:0.5px}";
  html += ".info-card p{font-size:20px;font-weight:bold;color:#667eea;word-break:break-all}";
  html += ".chart-container{position:relative;height:300px;margin:20px 0;background:#f8f9fa;border-radius:10px;padding:15px}";
  html += "canvas{width:100%!important;height:100%!important}";
  html += ".form-group{margin-bottom:20px}.form-group label{display:block;font-weight:600;color:#333;margin-bottom:8px;font-size:14px}";
  html += ".form-group input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:8px;font-size:14px;transition:all 0.3s}";
  html += ".form-group input:focus{outline:none;border-color:#667eea;box-shadow:0 0 0 3px rgba(102,126,234,0.1)}";
  html += ".form-group input:disabled{background:#f5f5f5;cursor:not-allowed}";
  html += ".form-row{display:grid;grid-template-columns:1fr 1fr;gap:15px}";
  html += ".checkbox-group{display:flex;align-items:center;gap:10px;margin-bottom:15px}";
  html += ".checkbox-group input[type='checkbox']{width:18px;height:18px}";
  html += ".checkbox-group label{font-weight:600;color:#333;margin-bottom:0}";
  html += ".btn{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;padding:12px 24px;font-size:14px;font-weight:600;border-radius:8px;cursor:pointer;transition:all 0.3s;margin:5px}";
  html += ".btn:hover{transform:translateY(-2px);box-shadow:0 5px 15px rgba(102,126,234,0.4)}";
  html += ".btn:active{transform:translateY(0)}";
  html += ".btn-success{background:linear-gradient(135deg,#11998e 0%,#38ef7d 100%)}";
  html += ".btn-danger{background:linear-gradient(135deg,#eb3349 0%,#f45c43 100%)}";
  html += ".btn-warning{background:linear-gradient(135deg,#f093fb 0%,#f5576c 100%)}";
  html += ".btn-block{width:100%;margin:10px 0}";
  html += ".alert{padding:15px;border-radius:8px;margin-bottom:20px;font-size:14px}";
  html += ".alert-info{background:#d1ecf1;color:#0c5460;border-left:4px solid #17a2b8}";
  html += ".alert-warning{background:#fff3cd;color:#856404;border-left:4px solid #ffc107}";
  html += "@media(max-width:768px){.info-grid{grid-template-columns:1fr}.content{padding:20px}.form-row{grid-template-columns:1fr}}";
  html += "</style></head><body><div class='container'>";
  html += "<div class='header'><h1>⚙️ Well Analyzer</h1><p>Configuration & Monitoring Dashboard</p></div>";
  html += "<div class='content'>";

  html += "<div class='section'><h2>📊 System Information</h2>";
  html += "<div class='info-grid'>";
  html += "<div class='info-card'><h3>MAC Address</h3><p id='mac'>...</p></div>";
  html += "<div class='info-card'><h3>Battery</h3><p id='bat'>...</p></div>";
  html += "<div class='info-card'><h3>Mode</h3><p id='mode'>...</p></div>";
  html += "<div class='info-card'><h3>Load -> 8000</h3><p id='load'>...</p></div>";
  html += "</div></div>";

  html += "<div class='section'><h2>📈 Real-Time Charts</h2>";
  html += "<div class='chart-container'><canvas id='chart1'></canvas></div>";
  html += "<div class='chart-container'><canvas id='chart2'></canvas></div>";
  html += "</div>";

  // Sensor Type Configuration Section
  html += "<div class='section'><h2>🔧 Sensor Type Configuration</h2>";
  html += "<div class='alert alert-info'>Select the type of sensor connected to the system</div>";
  html += "<form id='sensorForm'>";
  html += "<div class='checkbox-group'>";
  
  // Obtener el tipo de sensor actual
  String currentSensorType = readFile(SPIFFS, "/sensor_type.txt");
  if (currentSensorType == "") currentSensorType = "1"; // Default to HT
  
  String htChecked = (currentSensorType == "1") ? "checked" : "";
  String prtChecked = (currentSensorType == "2") ? "checked" : "";
  
  html += "<input type='radio' id='sensorHT' name='sensorType' value='1' " + htChecked + ">";
  html += "<label for='sensorHT'>HT Sensor (Horseshoe load cell)</label>";
  html += "</div>";
  html += "<div class='checkbox-group'>";
  html += "<input type='radio' id='sensorPRT' name='sensorType' value='2' " + prtChecked + ">";
  html += "<label for='sensorPRT'>PRT Sensor (Polisher Rod Transducer)</label>";
  html += "</div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveSensorType()'>💾 Save Sensor Type</button>";
  html += "</form></div>";

  // WiFi Configuration Section
  html += "<div class='section'><h2>📶 WiFi Configuration</h2>";
  html += "<form id='wifiForm'>";
  html += "<div class='form-group'><label>WiFi SSID</label>";
  html += "<input type='text' id='wifiSsid' value='" + readFile(SPIFFS, "/wifi_ssid.txt") + "' placeholder='Enter WiFi network name'></div>";
  html += "<div class='form-group'><label>WiFi Password</label>";
  html += "<input type='password' id='wifiPass' value='" + readFile(SPIFFS, "/wifi_pass.txt") + "' placeholder='Enter WiFi password'></div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveWiFi()'>💾 Save WiFi Config</button>";
  html += "</form></div>";

  // MQTT Configuration Section
  html += "<div class='section'><h2>📮 MQTT Configuration</h2>";
  html += "<form id='mqttForm'>";
  html += "<div class='form-group'><label>MQTT Server</label>";
  html += "<input type='text' id='mqttServer' value='" + readFile(SPIFFS, "/mqtt_server.txt") + "' placeholder='mqtt.example.com or IP address'></div>";
  html += "<div class='form-row'>";
  html += "<div class='form-group'><label>MQTT Port</label>";
  html += "<input type='number' id='mqttPort' value='" + readFile(SPIFFS, "/mqtt_port.txt") + "' placeholder='1883'></div>";
  html += "<div class='form-group'><label>MQTT User (optional)</label>";
  html += "<input type='text' id='mqttUser' value='" + readFile(SPIFFS, "/mqtt_user.txt") + "' placeholder='username'></div>";
  html += "</div>";
  html += "<div class='form-group'><label>MQTT Password (optional)</label>";
  html += "<input type='password' id='mqttPass' value='" + readFile(SPIFFS, "/mqtt_pass.txt") + "' placeholder='password'></div>";
  html += "<div class='form-group'><label>MQTT Topic</label>";
  html += "<input type='text' id='mqttTopic' value='" + readFile(SPIFFS, "/mqtt_topic.txt") + "' placeholder='well/analyzer/data'></div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveMQTT()'>💾 Save MQTT Config</button>";
  html += "</form></div>";

  // Optimizer Configuration Section - CORREGIDO
  html += "<div class='section'><h2>⚡ Optimizer Configuration</h2>";
  html += "<div class='alert alert-warning'>Enable optimizer for improved performance and power efficiency</div>";
  html += "<form id='optimizerForm'>";
  html += "<div class='checkbox-group'>";
  
  // Línea corregida - usar String() para la concatenación
  String optimizerChecked = stringToBool(readFile(SPIFFS, "/optimizer_enabled.txt")) ? "checked" : "";
  html += "<input type='checkbox' id='optimizerEnabled' " + optimizerChecked + ">";
  
  html += "<label for='optimizerEnabled'>Enable Optimizer</label>";
  html += "</div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveOptimizer()'>💾 Save Optimizer Config</button>";
  html += "</form></div>";

  // ESP-NOW Configuration Section - CORREGIDO
  html += "<div class='section'><h2>📡 ESP-NOW Configuration</h2>";
  html += "<div class='alert alert-info'>Configure peer-to-peer communication with other ESP32 devices</div>";
  html += "<form id='espnowForm'>";
  html += "<div class='checkbox-group'>";
  
  // Línea corregida - usar String() para la concatenación
  String espnowChecked = stringToBool(readFile(SPIFFS, "/espnow_enabled.txt")) ? "checked" : "";
  html += "<input type='checkbox' id='espnowEnabled' " + espnowChecked + ">";
  
  html += "<label for='espnowEnabled'>Enable ESP-NOW Communication</label>";
  html += "</div>";
  html += "<div class='form-group'><label>MAC Receiver Address</label>";
  html += "<input type='text' id='espnowMac' value='" + readFile(SPIFFS, "/espnow_mac.txt") + "' placeholder='FF:FF:FF:FF:FF:FF'></div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveEspNow()'>💾 Save ESP-NOW Config</button>";
  html += "</form></div>";

  // Deep Sleep Configuration Section
  html += "<div class='section'><h2>⏱️ Deep Sleep Configuration</h2>";
  html += "<div class='alert alert-info'>Set the time interval between measurements (in seconds)</div>";
  html += "<form id='sleepForm'>";
  html += "<div class='form-group'><label>Deep Sleep Cycle (appTxDutyCycle) - Seconds</label>";
  html += "<input type='number' id='sleepCycle' value='" + readFile(SPIFFS, "/sleep_cycle.txt") + "' placeholder='300' min='10' max='86400'></div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveSleep()'>💾 Save Sleep Config</button>";
  html += "</form></div>";

  // LoRaWAN Configuration Section
  html += "<div class='section'><h2>📡 LoRaWAN Configuration</h2>";
  html += "<div class='alert alert-info'>DevEUI is read-only and generated from device hardware</div>";
  html += "<form id='loraForm'>";
  html += "<div class='form-group'><label>DevEUI (Read-Only)</label>";
  html += "<input type='text' id='devEui' value='" + getDevEuiString() + "' disabled></div>";
  html += "<div class='form-group'><label>AppEUI</label>";
  html += "<input type='text' id='appEui' value='" + getAppEuiString() + "' placeholder='00:00:00:00:00:00:00:00'></div>";
  html += "<div class='form-group'><label>AppKey</label>";
  html += "<input type='text' id='appKey' value='" + getAppKeyString() + "' placeholder='00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'></div>";
  html += "<button type='button' class='btn btn-success btn-block' onclick='saveLoRa()'>💾 Save LoRaWAN Config</button>";
  html += "</form></div>";

  html += "<div class='section'><h2>🔧 Control Panel</h2>";
  html += "<button class='btn btn-success' onclick='setMode(\"WiFi\")'>📶 WiFi Mode</button>";
  html += "<button class='btn btn-success' onclick='setMode(\"LoRa\")'>📡 LoRa Mode</button>";
  html += "<button class='btn btn-warning' onclick='location.reload()'>🔄 Refresh</button>";
  html += "<button class='btn btn-danger' onclick='resetDevice()'>🔁 Reset Device</button>";
  html += "</div>";

  html += "</div></div>";

  html += "<script>";
  html += "let d1=[],d2=[],maxPoints=150;";
  html += "function draw(ctx,data,w,h){ctx.clearRect(0,0,w,h);if(data.length<2)return;";
  html += "let mx=Math.max(...data),mn=Math.min(...data),range=mx-mn||1;";
  html += "ctx.strokeStyle='#667eea';ctx.lineWidth=3;ctx.lineCap='round';ctx.lineJoin='round';";
  html += "ctx.shadowColor='rgba(102,126,234,0.3)';ctx.shadowBlur=8;ctx.beginPath();";
  html += "data.forEach((v,i)=>{let x=(i/(data.length-1))*w;let y=h-((v-mn)/range)*(h-20)-10;";
  html += "i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.stroke();";
  html += "ctx.shadowBlur=0;ctx.fillStyle='#999';ctx.font='12px Arial';";
  html += "ctx.fillText('Max: '+mx.toFixed(1),10,20);ctx.fillText('Min: '+mn.toFixed(1),10,h-10);}";
  html += "function updateCharts(){";
  html += "let c1=document.getElementById('chart1'),c2=document.getElementById('chart2');";
  html += "if(!c1||!c2)return;let ctx1=c1.getContext('2d'),ctx2=c2.getContext('2d');";
  html += "c1.width=c1.offsetWidth;c1.height=c1.offsetHeight;";
  html += "c2.width=c2.offsetWidth;c2.height=c2.offsetHeight;";
  html += "draw(ctx1,d1,c1.width,c1.height);draw(ctx2,d2,c2.width,c2.height);}";
  html += "setInterval(()=>{fetch('/data').then(r=>r.json()).then(data=>{";
  html += "document.getElementById('mac').textContent=data.mac;";
  html += "document.getElementById('bat').textContent=data.bat+' V';";
  html += "document.getElementById('mode').textContent=data.mode;";
  html += "document.getElementById('load').textContent=data.load.toFixed(1);";
  html += "d1.push(parseFloat(data.load));d2.push(parseFloat(data.acc));";
  html += "if(d1.length>maxPoints){d1.shift();d2.shift()}updateCharts();";
  html += "}).catch(e=>console.error('Error:',e))},500);";

  // Sensor Type save function
  html += "function saveSensorType(){";
  html += "let sensorType=document.querySelector('input[name=\"sensorType\"]:checked');";
  html += "if(!sensorType){alert('Please select a sensor type');return;}";
  html += "fetch('/setSensorType?type='+sensorType.value)";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  // WiFi save function
  html += "function saveWiFi(){let ssid=document.getElementById('wifiSsid').value;";
  html += "let pass=document.getElementById('wifiPass').value;";
  html += "if(!ssid){alert('SSID cannot be empty');return;}";
  html += "fetch('/setWiFi?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass))";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  // MQTT save function
  html += "function saveMQTT(){let server=document.getElementById('mqttServer').value;";
  html += "let port=document.getElementById('mqttPort').value;";
  html += "let user=document.getElementById('mqttUser').value;";
  html += "let pass=document.getElementById('mqttPass').value;";
  html += "let topic=document.getElementById('mqttTopic').value;";
  html += "if(!server||!port){alert('Server and Port are required');return;}";
  html += "fetch('/setMQTT?server='+encodeURIComponent(server)+'&port='+port+'&user='+encodeURIComponent(user)+'&pass='+encodeURIComponent(pass)+'&topic='+encodeURIComponent(topic))";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  // Optimizer save function
  html += "function saveOptimizer(){let enabled=document.getElementById('optimizerEnabled').checked;";
  html += "fetch('/setOptimizer?enabled='+enabled)";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  // ESP-NOW save function
  html += "function saveEspNow(){let enabled=document.getElementById('espnowEnabled').checked;";
  html += "let mac=document.getElementById('espnowMac').value;";
  html += "if(enabled&&!mac){alert('MAC Address is required when ESP-NOW is enabled');return;}";
  html += "fetch('/setEspNow?enabled='+enabled+'&mac='+encodeURIComponent(mac))";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  // Sleep cycle save function
  html += "function saveSleep(){let cycle=document.getElementById('sleepCycle').value;";
  html += "if(!cycle||cycle<10||cycle>86400){alert('Sleep cycle must be between 10 and 86400 seconds');return;}";
  html += "fetch('/setSleep?cycle='+cycle)";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";

  html += "function setMode(m){fetch('/setMode?mode='+m).then(()=>alert('Mode changed to '+m)).catch(e=>alert('Error: '+e))}";
  html += "function saveLoRa(){let appeui=document.getElementById('appEui').value;";
  html += "let appkey=document.getElementById('appKey').value;";
  html += "fetch('/setLoRa?appeui='+encodeURIComponent(appeui)+'&appkey='+encodeURIComponent(appkey))";
  html += ".then(r=>r.text()).then(msg=>alert(msg)).catch(e=>alert('Error: '+e))}";
  html += "function resetDevice(){if(confirm('Reset device? This will restart the ESP32.')){";
  html += "fetch('/reset').then(()=>alert('Device resetting...')).catch(e=>alert('Error: '+e))}}";
  html += "window.addEventListener('resize',updateCharts);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleData() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float rawLoad = scale.get_value(5) * 0.01;
  //float load = constrain(map(rawLoad, LOAD_MIN, LOAD_MAX, 0, 10000), 0, 10000);
  //float acc = constrain(a.acceleration.x, ACC_MIN, ACC_MAX);
  float acc = a.acceleration.x;
  //currentMode = readFile(SPIFFS, "/mode.txt");

  uint64_t chipId = ESP.getEfuseMac();
  String json = "{\"mac\":\"" + String(chipId, HEX) + "\",\"bat\":" + String(vBat(), 2);
  json += ",\"mode\":\"" + currentMode + "\",\"load\":" + String(rawLoad, 2);
  json += ",\"acc\":" + String(acc, 2) + "}";

  server.send(200, "application/json", json);
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    currentMode = server.arg("mode");
    saveSetting(currentMode, "/mode.txt");
    oledShow("Mode Changed", currentMode, "", "Saved!");
    ledBlink('G', 2);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing mode parameter");
  }
}

void handleSetWiFi() {
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");

    saveSetting(ssid, "/wifi_ssid.txt");
    saveSetting(pass, "/wifi_pass.txt");

    oledShow("WiFi Config", "Updated!", ssid, "Saved to SPIFFS");
    ledBlink('G', 3);

    server.send(200, "text/plain", "WiFi configuration saved successfully!");
  } else {
    server.send(400, "text/plain", "Missing SSID parameter");
  }
}

void handleSetMQTT() {
  if (server.hasArg("server") && server.hasArg("port")) {
    String mqttServer = server.arg("server");
    String mqttPort = server.arg("port");
    String mqttUser = server.arg("user");
    String mqttPass = server.arg("pass");
    String mqttTopic = server.arg("topic");

    saveSetting(mqttServer, "/mqtt_server.txt");
    saveSetting(mqttPort, "/mqtt_port.txt");
    saveSetting(mqttUser, "/mqtt_user.txt");
    saveSetting(mqttPass, "/mqtt_pass.txt");
    saveSetting(mqttTopic, "/mqtt_topic.txt");

    oledShow("MQTT Config", "Updated!", mqttServer + ":" + mqttPort, "Saved!");
    ledBlink('G', 3);

    server.send(200, "text/plain", "MQTT configuration saved successfully!");
  } else {
    server.send(400, "text/plain", "Missing server or port parameter");
  }
}

void handleSetOptimizer() {
  if (server.hasArg("enabled")) {
    String optimizerEnabledS = server.arg("enabled");
    saveSetting(optimizerEnabledS, "/optimizer_enabled.txt");

    oledShow("Optimizer", "Updated!", optimizerEnabledS == "true" ? "Enabled" : "Disabled", "Saved!");
    ledBlink('G', 2);

    optimizerEnabled = stringToBool(optimizerEnabledS);

    server.send(200, "text/plain", "Optimizer configuration saved: " + optimizerEnabledS);
  } else {
    server.send(400, "text/plain", "Missing enabled parameter");
  }
}

void handleSetEspNow() {
  if (server.hasArg("enabled")) {
    String espnowEnabled = server.arg("enabled");
    String espnowMac = server.arg("mac");

    saveSetting(espnowEnabled, "/espnow_enabled.txt");
    saveSetting(espnowMac, "/espnow_mac.txt");

    oledShow("ESP-NOW Config", "Updated!", espnowEnabled == "true" ? "Enabled" : "Disabled", "Saved!");
    ledBlink('G', 2);

    server.send(200, "text/plain", "ESP-NOW configuration saved successfully!");
  } else {
    server.send(400, "text/plain", "Missing enabled parameter");
  }
}

void handleSetSleep() {
  if (server.hasArg("cycle")) {
    String sleepCycle = server.arg("cycle");
    sleepCycle.trim();

    int cycleSeconds = sleepCycle.toInt();

    if (cycleSeconds >= 10 && cycleSeconds <= 86400) {
      // Guardar en SPIFFS
      saveSetting(sleepCycle, "/sleep_cycle.txt");

      // 🔥 ACTUALIZAR VARIABLES GLOBALES
      sleepCycleMs = cycleSeconds * 1000;
      sleepCycleLoaded = true;  // Marcar como cargado

      oledShow("Sleep Config", "Updated!", sleepCycle + " seconds", "Saved!");
      ledBlink('G', 3);

      Serial.println("✓ Sleep cycle updated: " + String(cycleSeconds) + "s (" + String(sleepCycleMs) + "ms)");

      server.send(200, "text/plain",
                  "✓ Sleep cycle saved: " + String(cycleSeconds) + "s (" + String(sleepCycleMs) + "ms)");
    } else {
      Serial.println("✗ Invalid sleep cycle: " + String(cycleSeconds) + "s");
      server.send(400, "text/plain",
                  "✗ Sleep cycle must be between 10 and 86400 seconds");
    }
  } else {
    server.send(400, "text/plain", "✗ Missing cycle parameter");
  }
}

void handleSetLoRa() {
  if (server.hasArg("appeui") && server.hasArg("appkey")) {
    String appEuiStr = server.arg("appeui");
    String appKeyStr = server.arg("appkey");

    appEuiStr.replace(":", "");
    appKeyStr.replace(":", "");
    appEuiStr.toUpperCase();
    appKeyStr.toUpperCase();

    if (appEuiStr.length() == 16) {
      for (int i = 0; i < 8; i++) {
        String byteStr = appEuiStr.substring(i * 2, i * 2 + 2);
        appEui[i] = strtol(byteStr.c_str(), NULL, 16);
      }
    }

    if (appKeyStr.length() == 32) {
      for (int i = 0; i < 16; i++) {
        String byteStr = appKeyStr.substring(i * 2, i * 2 + 2);
        appKey[i] = strtol(byteStr.c_str(), NULL, 16);
      }
    }

    saveLoRaConfig();
    oledShow("LoRa Config", "Updated!", "", "Saved to SPIFFS");
    ledBlink('G', 3);

    server.send(200, "text/plain", "LoRaWAN configuration saved successfully!");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleReset() {
  server.send(200, "text/plain", "Resetting...");
  oledShow("System", "RESET", "Restarting...");
  ledBlink('R', 3);
  delay(1000);
  ESP.restart();
}

void handleSetSensorType() {
  if (server.hasArg("type")) {
    sensorType = server.arg("type");
    saveSetting(sensorType, "/sensor_type.txt");
    
    oledShow("Sensor Type", "Updated!", sensorType == "1" ? "HT Sensor" : "PRT Sensor", "Saved!");
    ledBlink('G', 2);
    
    server.send(200, "text/plain", "Sensor type saved: " + sensorType);
  } else {
    server.send(400, "text/plain", "Missing type parameter");
  }
}

void testTask() {
  oledShow("Test Mode", "Starting AP", apSSID);
  WiFi.softAP(apSSID, apPassword);

  IPAddress IP = WiFi.softAPIP();
  oledShow("AP Ready", "IP: " + IP.toString(), "", "Connect now!");
  ledBlink('G', 3);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setMode", handleSetMode);
  server.on("/setSensorType", handleSetSensorType);
  server.on("/setWiFi", handleSetWiFi);
  server.on("/setMQTT", handleSetMQTT);
  server.on("/setOptimizer", handleSetOptimizer);  // Nuevo
  server.on("/setEspNow", handleSetEspNow);        // Nuevo
  server.on("/setSleep", handleSetSleep);
  server.on("/setLoRa", handleSetLoRa);
  server.on("/reset", handleReset);
  server.begin();

  oledShow("Web Server", "Running", IP.toString(), "Port: 80");

  while (1) {
    server.handleClient();
    delay(10);
  }
}

/* =============== SETUP =============== */
void setup() {
  Serial.begin(115200);

  // Configurar Vext
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // Encender periféricos
  delay(100);

  ledsInit();
  oledInit();
  oledShow("Well Analyzer", "Initializing...", "Sensor: " + String(sensorType == "1" ? "HT" : "PRT"),"", "v2.1 Enhanced");
  ledBlink('B', 1);

  Wire1.begin(SDA_ACC, SCL_ACC);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  pinMode(PIN_CONFIG, INPUT);
  pinMode(PIN_TX_MODE, INPUT);

  // Lectura configuraciones
  if (!SPIFFS.begin(true)) {
    ledBlink('R', 3);
  } else {
    loadSleepCycleOnce();
    loadAllConfigurations();
    ledBlink('G', 1);
  }
  delay(1000);

  if(optimizerEnabled) display.displayOff();

  // PINES DE BATERIA
  pinMode(pinBattery, INPUT);
  pinMode(ADC_CTRL_PIN, OUTPUT);
  analogReadResolution(12);  // 12 bits
  analogSetAttenuation(ADC_11db);  // Rango 0-3.3V

  oledShow("Sensors", "Init MPU6050...");
  if (!mpu.begin(0x68, &Wire1)) {
    oledShow("Error!", "MPU6050", "not found!", "Check wiring");
    ledBlink('R', 5);
    delay(3000);
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    oledShow("Sensors", "MPU6050 OK!");
    ledBlink('G', 1);
  }
  delay(1000);

  if (!digitalRead(PIN_CONFIG)) {
    oledShow("Mode", "CONFIGURATION", "", "Starting...");
    ledBlink('B', 3);
    testTask();
  }

  // ************** main task **************
  oledShow("Mode", "OPERATION", "", "Starting...");
  mainTask();

  digitalWrite(Vext, HIGH); 

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  deviceState = DEVICE_STATE_INIT;

  oledShow("TX Mode", currentMode, "", "Preparing...");

  if (currentMode == "WiFi") {
    wifiTx();

    uint32_t sleepMs = getSleepCycleMs();

    oledShow("Going to", "DEEP SLEEP", String(sleepMs / 1000) + "s");
    Serial.println("Entering WiFi deep sleep for " + String(sleepMs / 1000) + "s");

    if(!optimizerEnabled) delay(3000);
    
    prepararDeepSleep();

    esp_sleep_enable_timer_wakeup(sleepMs * 1000);
    esp_deep_sleep_start();
  }

  // Inicializar variables para modo LoRa
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if(wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
      Serial.println("🔔 Woke up from deep sleep");
  } else {
      Serial.println("🚀 Cold start");
  }
  
  // Resetear flags
  downlinkReceived = false;

  ledBlink('G', 2);
}

/* =============== FUNCIÓN VERIFICACIÓN DEEP SLEEP =============== */
void prepararDeepSleep() {

  // 1. Apagar Vext (periféricos externos)
  digitalWrite(Vext, HIGH);
  delay(10);

  // 3. Opcional: Configurar qué pines mantener en qué estado
  //rtc_gpio_hold_en(GPIO_NUM_36);

  Serial.println("Entrando en deep sleep...");
  Serial.flush();  
}

void enterDeepSleep(unsigned long sleepTimeMs) {
    String reason = "";
    
    if(downlinkReceived) {
        reason = "Downlink OK";
    } else if(!isJoined && joinAttemptCount >= MAX_JOIN_ATTEMPTS) {
        reason = "Join Fail";
    } else if(isJoined && downlinkAttemptCount >= MAX_DOWNLINK_ATTEMPTS) {
        reason = "No Downlink";
    } else {
        reason = transmissionSuccessful ? "TX OK" : "TX Fail";
    }
    
    Serial.println("💤 Entering deep sleep for " + String(sleepTimeMs / 1000) + "s - Reason: " + reason);
    
    // Mostrar en OLED si tienes
    oledShow("Deep Sleep", String(sleepTimeMs / 1000) + "s", "Reason:", reason);
    
    // LED blink si tienes
    ledBlink(downlinkReceived ? 'G' : 'B', 3);
    
    delay(2000);
    
    // Preparar deep sleep
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);
    
    // Configurar wakeup para Clase A
    if(loraWanClass == CLASS_A) {
        #ifdef Wireless_Mini_Shell
        esp_deep_sleep_enable_gpio_wakeup(1 << INT_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        #else
        esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_PIN, 1);
        #endif
    }
    
    Serial.flush();
    
    // Limpiar flags para el próximo ciclo
    downlinkReceived = false;

    prepararDeepSleep();
    
    delay(100);
    esp_deep_sleep_start();
}

/* ========================= MAIN LOOP ========================= */
void loop() {
  switch (deviceState) {
    case DEVICE_STATE_INIT:
      {
      #if (LORAWAN_DEVEUI_AUTO)
        LoRaWAN.generateDeveuiByChipID();
      #endif
        LoRaWAN.init(loraWanClass, loraWanRegion);
        break;
      }
    case DEVICE_STATE_JOIN:
      {
        LoRaWAN.join();
        break;
      }
    case DEVICE_STATE_SEND:
      {
        prepareTxFrame(appPort);
        LoRaWAN.send();
        deviceState = DEVICE_STATE_CYCLE;
        break;
      }
    case DEVICE_STATE_CYCLE:
      {
        // Schedule next packet transmission
        txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
        break;
      }
    case DEVICE_STATE_SLEEP:
      {
        if (loraWanClass == CLASS_A) {
        #ifdef WIRELESS_MINI_SHELL
          esp_deep_sleep_enable_gpio_wakeup(1 << INT_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        #else
          esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_PIN, 1);
        #endif
        }
        LoRaWAN.sleep(loraWanClass);
        break;
      }
    default:
      {
        deviceState = DEVICE_STATE_INIT;
        break;
      }
  }
}

// Función para manejar ACK
void downLinkAckHandle() {
  Serial.println("ACK recibido del servidor");
  transmissionSuccessful = true;
  enterDeepSleep(sleepCycleMs);
}
