/*
Runing
- A: wakeup motion detected and timmer.
- B: motion?
- x C: select wifi or lora mode
- D: read configuration (lora)
- x E: adquisiton raw data
- x F: proccess data
- x G: running NN
- x H: make payload
- x I: send data
- J: deep sleep start (revisar... )

*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "HX711.h"
#include "heltec.h"
#include <esp_system.h>

#include <Average.h>

#include "HT_SSD1306Wire.h"
#include "TFLI2C.h"
TFLI2C sensor;

#include <HardwareSerial.h>
HardwareSerial lidarSerial(1);

/* =============== WIFI MODE TX =============== */
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "SPIFFS.h"

String runssid = "MIFI_8D3D";
String runpassword = "1234567890";
const char *topicPublish = "jhpOandG/data";
int statusW = WL_IDLE_STATUS;

const char *broker = "24.199.125.52";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

#include <WebServer.h>

// Configuración del punto de acceso WiFi
const char* ssid = "WA_DataGraph";
const char* password = "12345678";

WebServer server(80);
float tload = 0;
float tdist = 0;
float tacc = 0;
String currentMode = "WiFi";
String SearchPath = "/devices/api/buscar-well-analyzer/?mac=";
/* =============== CONFIG SENSORS =============== */
Adafruit_MPU6050 mpu;

const uint8_t SCL_ACC = 42;
const uint8_t SDA_ACC = 41;

const int LOADCELL_DOUT_PIN = 47;
const int LOADCELL_SCK_PIN = 48;
float alpha = 0.5;
boolean runNN = true;

float a0[50] = {-0.161094, 0.133767, 0.509254, 0.586077, 0.588004, 0.572422, 0.577633, 0.567187, 0.58284, 0.574648, 0.567952, 0.564416, 0.566411, 0.591799, 0.592944, 0.584578, 0.607816, 0.609944, 0.606361, 0.607037, 0.625326, 0.663207, 0.661868, 0.597974, 0.369072, 0.179431, -0.0484264, -0.113182, -0.097836, -0.0759193, -0.0531394, -0.0452247, -0.0245321, -0.0140658, -0.010772, -0.0177663, -0.0145408, -0.00956059, -0.0108517, 0.00724792, 0.0163409, 0.0166782, 0.0187881, 0.0292116, 0.0331711, 0.0459051, 0.0554916, -0.0497948, -0.202742, -0.291714};
/*
float a0[567] = {4.19,4.22,4.26,4.29,4.32,4.36,4.39,4.43,4.46,4.51,4.56,4.61,4.66,4.7,4.73,4.78,4.82,4.86,4.9,4.94,4.99,5.03,5.08,5.13,5.18,5.24,5.29,5.33,5.38,5.42,5.47,5.51,5.54,5.57,5.58,5.6,5.6,5.59,5.58,5.56,5.55,5.54,5.53,5.51,5.5,5.48,5.47,5.44,5.43,5.41,5.4,5.38,5.36,5.34,5.32,5.3,5.28,5.26,5.24,5.22,5.21,5.19,5.19,5.2,5.21,5.23,5.25,5.27,5.29,5.31,5.34,5.36,5.38,5.4,5.42,5.43,5.44,5.45,5.45,5.46,5.47,5.49,5.5,5.52,5.53,5.54,5.54,5.54,5.54,5.54,5.53,5.53,5.51,5.49,5.46,5.44,5.41,5.39,5.36,5.32,5.3,5.28,5.27,5.26,5.25,5.24,5.23,5.21,5.2,5.2,5.19,5.19,5.18,5.19,5.18,5.19,5.19,5.18,5.19,5.2,5.21,5.24,5.27,5.29,5.32,5.35,5.38,5.39,5.41,5.42,5.43,5.43,5.44,5.45,5.45,5.45,5.45,5.44,5.43,5.43,5.42,5.42,5.42,5.42,5.42,5.42,5.41,5.39,5.37,5.35,5.33,5.31,5.29,5.27,5.25,5.24,5.21,5.2,5.19,5.18,5.18,5.18,5.2,5.21,5.22,5.24,5.25,5.26,5.27,5.28,5.29,5.29,5.31,5.33,5.35,5.36,5.37,5.39,5.4,5.41,5.42,5.43,5.44,5.45,5.46,5.47,5.48,5.48,5.48,5.47,5.46,5.44,5.43,5.43,5.42,5.42,5.4,5.39,5.38,5.37,5.35,5.34,5.33,5.33,5.33,5.32,5.32,5.32,5.31,5.3,5.29,5.29,5.29,5.3,5.31,5.32,5.33,5.35,5.36,5.38,5.39,5.41,5.43,5.44,5.46,5.47,5.49,5.5,5.51,5.52,5.52,5.53,5.53,5.53,5.54,5.55,5.55,5.56,5.56,5.57,5.57,5.57,5.56,5.55,5.54,5.53,5.52,5.51,5.49,5.48,5.46,5.45,5.44,5.42,5.42,5.41,5.41,5.41,5.41,5.41,5.4,5.39,5.38,5.36,5.33,5.3,5.27,5.23,5.19,5.16,5.12,5.09,5.06,5.02,4.99,4.97,4.94,4.91,4.88,4.86,4.83,4.8,4.77,4.74,4.71,4.68,4.65,4.61,4.58,4.55,4.52,4.49,4.46,4.43,4.4,4.37,4.34,4.31,4.29,4.26,4.23,4.21,4.19,4.16,4.13,4.11,4.08,4.05,4.03,4.0,3.97,3.95,3.93,3.9,3.86,3.83,3.8,3.76,3.72,3.69,3.66,3.62,3.58,3.55,3.51,3.47,3.43,3.39,3.35,3.32,3.28,3.24,3.2,3.16,3.12,3.08,3.05,3.02,2.99,2.95,2.93,2.91,2.89,2.88,2.87,2.87,2.87,2.87,2.87,2.88,2.89,2.9,2.92,2.94,2.95,2.96,2.97,2.99,3.0,3.02,3.03,3.05,3.06,3.07,3.09,3.1,3.11,3.12,3.13,3.13,3.13,3.12,3.11,3.1,3.08,3.07,3.04,3.03,3.01,2.99,2.96,2.95,2.93,2.92,2.9,2.89,2.89,2.88,2.87,2.87,2.86,2.86,2.86,2.87,2.87,2.87,2.87,2.88,2.89,2.91,2.93,2.95,2.97,3.0,3.02,3.05,3.08,3.1,3.12,3.14,3.15,3.17,3.18,3.18,3.19,3.19,3.18,3.18,3.18,3.18,3.17,3.17,3.17,3.17,3.17,3.16,3.15,3.13,3.13,3.12,3.11,3.09,3.07,3.05,3.03,3.01,2.99,2.98,2.98,2.98,2.98,2.99,3.0,3.01,3.02,3.04,3.05,3.06,3.07,3.08,3.08,3.09,3.1,3.11,3.12,3.12,3.12,3.12,3.13,3.15,3.17,3.2,3.22,3.24,3.26,3.27,3.27,3.28,3.27,3.27,3.26,3.25,3.25,3.24,3.23,3.22,3.21,3.2,3.2,3.19,3.19,3.19,3.19,3.2,3.21,3.21,3.21,3.2,3.19,3.18,3.16,3.16,3.16,3.16,3.15,3.16,3.17,3.18,3.19,3.2,3.22,3.23,3.25,3.27,3.29,3.3,3.31,3.32,3.32,3.33,3.33,3.33,3.33,3.33,3.34,3.34,3.34,3.35,3.35,3.36,3.36,3.36,3.36,3.37,3.37,3.37,3.37,3.36,3.35,3.33,3.32,3.31,3.31,3.31,3.31,3.31,3.33,3.34,3.36,3.38,3.4,3.42,3.44,3.46,3.48,3.5,3.52,3.54,3.56,3.57,3.6,3.61,3.63,3.65,3.67,3.7,3.72,3.74,3.76,3.78,3.79,3.81,3.83,3.86,3.88,3.91,3.94};
*/
const int16_t nData = 500;

HX711 scale;

#define INT_PIN 33 // Pin movement detector
#define PIN_CONFIG 46 // Pin config
#define PIN_TX_MODE 26 // Pin TX WiFi or LORA config
#define pinBattery 4
#define lR 7
#define lG 6
#define lB 5

String typeLidar = "1"; //====================================================!!!! 0: DFROBOT  1: LUNA //====================================================!!!! 0: DFROBOT  1: LUNA

/* =============== CONFIG OLED =============== */

//static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);

/* =============== CONFIG LORA =============== */
#include "LoRaWan_APP.h"

#define LORA_BANDWIDTH  0
#define LORA_SPREADING_FACTOR 12
// #define LORAWAN_DEVEUI_AUTO true

/* OTAA para*/
uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x03 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 };

/* ABP para*/
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda, 0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef, 0x67 };
uint32_t devAddr = (uint32_t)0x007e6ae1;

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 300000;

/*OTAA or ABP*/
bool overTheAirActivation = true;  //OTAA security is better

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 2;

uint8_t confirmedNbTrials = 8;  //

/* ================================== MAIN PAYLOAD DATA ================================== */
float spm = 0;
float fill = 0;
float sLength = 0;
float volBat = 4.55;
uint8_t status = -1; /*  0: stopped.  1: running, */
uint8_t integrated = 1;
float thresholdDiagnosis = 0.4;
float nnDiagnosis[10] = {0,0,0,0,0,0,0,0,0};

const uint64_t uS_TO_S_FACTOR = 1000000ULL;

Average<int16_t> load20(20);
Average<float> pos20(20);

Average<int16_t> load50(50);
Average<float> pos50(50);

/* ================================== TEST FUNCTIONS ================================== */
void handleRoot() {
  String html = "<!DOCTYPE html>\n";
  html += "<html lang='es'>\n";
  html += "<head>\n";
  html += "  <meta charset='UTF-8'>\n";
  html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "  <title>Well analyzer real time plots</title>\n";
  html += "  <script>\n";
  html += "   function submitMessage() {\n";
  html += "     alert('Saved value to SENSOR IN SPIFFS');\n";
  html += "     setTimeout(function(){ document.location.reload(false); }, 500);\n";
  html += "   }\n";
  html += "  </script>\n";
  html += "  <style>\n";
  html += "    body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }\n";
  html += "    h1 { color: #333; }\n";
  html += "    h2 { color: #555; margin-top: 30px; }\n";
  html += "    .chart-container { width: 100%; height: 200px; border: 1px solid #ccc; margin: 20px 0; }\n";
  html += "    .current-value { font-size: 18px; font-weight: bold; margin: 5px 0; }\n";
  html += "    .device-info { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #f5f5f5; }\n";
  html += "    .battery-indicator { display: inline-block; width: 150px; height: 20px; border: 1px solid #333; margin: 5px; }\n";
  html += "    .battery-level { height: 100%; background-color: #4CAF50; transition: width 0.5s; }\n";
  html += "    .control-panel { margin: 40px 0; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #f9f9f9; }\n";
  html += "    .reset-btn { background-color: #ff3333; color: white; border: none; padding: 12px 25px; font-size: 16px; cursor: pointer; border-radius: 5px; margin: 0 10px; }\n";
  html += "    .reset-btn:hover { background-color: #cc0000; }\n";
  html += "    .mode-btn { background-color: #4285f4; color: white; border: none; padding: 12px 25px; font-size: 16px; cursor: pointer; border-radius: 5px; margin: 0 10px; }\n";
  html += "    .mode-btn:hover { background-color: #2a75f3; }\n";
  html += "    .active-mode { background-color: #34a853; }\n";
  html += "    .active-mode:hover { background-color: #2d9249; }\n";
  html += "    .reset-confirm { display: none; margin-top: 15px; }\n";
  html += "    .confirm-btns { display: flex; justify-content: center; gap: 10px; margin-top: 10px; }\n";
  html += "    .confirm-btn { padding: 8px 20px; border: none; border-radius: 4px; cursor: pointer; }\n";
  html += "    .yes-btn { background-color: #cc0000; color: white; }\n";
  html += "    .yes-btn:hover { background-color: #aa0000; }\n";
  html += "    .no-btn { background-color: #666; color: white; }\n";
  html += "    .no-btn:hover { background-color: #444; }\n";
  html += "  </style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "  <h5>Well Analyzer test mode</h5>\n";
  // Información del dispositivo (MAC y batería)
  html += "  <div class='device-info'>\n";
  html += "    <h3>Información del Dispositivo</h3>\n";
  html += "    <p><strong>Dirección MAC:</strong> <span id='macAddress'>Cargando...</span></p>\n";
  html += "    <p><strong>Voltaje de Batería:</strong> <span id='batteryVoltage'>0.00</span>V</p>\n";
  html += "    <div class='battery-indicator'>\n";
  html += "      <div id='batteryLevel' class='battery-level' style='width: 0%'></div>\n";
  html += "    </div>\n";
  html += "  </div>\n";

    // Panel de control con botón de reinicio y selector de modo
  html += "  <div class='control-panel'>\n";
  html += "    <h3>Panel de Control</h3>\n";
  html += "    <div align='center'>\n";
  html += "     <table>\n";
  html += "     <tr>\n";
  html += "     <td>\n";
  html += "      <button id='wifiModeBtn' class='mode-btn'>WiFi</button>\n";
  html += "     </td>\n";
  html += "     <td>\n";
  html += "      <button id='loraModeBtn' class='mode-btn'>LoRa</button>\n";
  html += "     </td>\n";
  html += "     </tr>\n";
  html += "     </table>\n";
  html += "      <button id='resetButton' class='reset-btn'>REINICIAR ESP32</button>\n";
  html += "    </div>\n";
  html += "      <button type = 'button' class='mode-btn' onclick='location.reload()'>REFRESH</button>\n";
  html += "    <p><strong>Modo Actual:</strong> <span id='currentMode'>Cargando...</span></p>\n";
  html += "    <div id='resetConfirm' class='reset-confirm'>\n";
  html += "      <p>¿Estás seguro que deseas reiniciar el ESP32?</p>\n";
  html += "      <div class='confirm-btns'>\n";
  html += "        <button id='confirmYes' class='confirm-btn yes-btn'>Sí, reiniciar</button>\n";
  html += "        <button id='confirmNo' class='confirm-btn no-btn'>No, cancelar</button>\n";
  html += "      </div>\n";
  html += "    </div>\n";
  html += "  </div>\n";

  // Gráfico 1
  html += "  <div class='graph-section'>\n";
  html += "    <h6>Load: <span id='val1'>0</span></h6>\n";
  html += "    <div class='chart-container'>\n";
  html += "      <canvas id='graphCanvas1' width='800' height='150'></canvas>\n";
  html += "    </div>\n";
  html += "  </div>\n";
  
  // Gráfico 2
  html += "  <div class='graph-section'>\n";
  html += "    <h6>Position: <span id='val2'>0</span></h6>\n";
  html += "    <div class='chart-container'>\n";
  html += "      <canvas id='graphCanvas2' width='800' height='150'></canvas>\n";
  html += "    </div>\n";
  html += "  </div>\n";
  
  // Gráfico 3
  html += "  <div class='graph-section'>\n";
  html += "    <h6>Acceleration: <span id='val3'>0</span></h6>\n";
  html += "    <div class='chart-container'>\n";
  html += "      <canvas id='graphCanvas3' width='800' height='150'></canvas>\n";
  html += "    </div>\n";
  html += "  </div>\n";
  
  html += "  <script>\n";
  html += "    // Actualizar información del dispositivo\n";
  html += "    function updateDeviceInfo() {\n";
  html += "      fetch('/deviceInfo')\n";
  html += "        .then(response => response.json())\n";
  html += "        .then(info => {\n";
  html += "          document.getElementById('macAddress').textContent = info.mac;\n";
  html += "          document.getElementById('batteryVoltage').textContent = info.battery.toFixed(2);\n";
  html += "          document.getElementById('currentMode').textContent = info.mode;\n";
  html += "          \n";
  html += "          // Actualizar el nivel de batería (asumiendo que 4.2V es 100% y 3.0V es 0%)\n";
  html += "          const batteryPercentage = Math.min(100, Math.max(0, (info.battery - 3.0) / 1.2 * 100));\n";
  html += "          document.getElementById('batteryLevel').style.width = batteryPercentage + '%';\n";
  html += "          \n";
  html += "          // Actualizar el color según el nivel de batería\n";
  html += "          if (batteryPercentage > 50) {\n";
  html += "            document.getElementById('batteryLevel').style.backgroundColor = '#4CAF50';\n";
  html += "          } else if (batteryPercentage > 20) {\n";
  html += "            document.getElementById('batteryLevel').style.backgroundColor = '#FFC107';\n";
  html += "          } else {\n";
  html += "            document.getElementById('batteryLevel').style.backgroundColor = '#F44336';\n";
  html += "          }\n";
  html += "          \n";
  html += "          // Actualizar los botones de modo\n";
  html += "          if (info.mode === 'WiFi') {\n";
  html += "            document.getElementById('wifiModeBtn').classList.add('active-mode');\n";
  html += "            document.getElementById('loraModeBtn').classList.remove('active-mode');\n";
  html += "          } else {\n";
  html += "            document.getElementById('wifiModeBtn').classList.remove('active-mode');\n";
  html += "            document.getElementById('loraModeBtn').classList.add('active-mode');\n";
  html += "          }\n";
  html += "        })\n";
  html += "        .catch(error => console.error('Error:', error));\n";
  html += "    }\n";
  html += "    \n";
  html += "    // Cambiar modo\n";
  html += "    document.getElementById('wifiModeBtn').addEventListener('click', function() {\n";
  html += "      fetch('/setMode?mode=WiFi')\n";
  html += "        .then(() => updateDeviceInfo())\n";
  html += "        .catch(error => console.error('Error:', error));\n";
  html += "    });\n";
  html += "    \n";
  html += "    document.getElementById('loraModeBtn').addEventListener('click', function() {\n";
  html += "      fetch('/setMode?mode=LoRa')\n";
  html += "        .then(() => updateDeviceInfo())\n";
  html += "        .catch(error => console.error('Error:', error));\n";
  html += "    });\n";
  html += "    \n";

  html += "    // Botón de reinicio\n";
  html += "    document.getElementById('resetButton').addEventListener('click', function() {\n";
  html += "      document.getElementById('resetConfirm').style.display = 'block';\n";
  html += "    });\n";
  html += "    \n";
  html += "    document.getElementById('confirmNo').addEventListener('click', function() {\n";
  html += "      document.getElementById('resetConfirm').style.display = 'none';\n";
  html += "    });\n";
  html += "    \n";
  html += "    document.getElementById('confirmYes').addEventListener('click', function() {\n";
  html += "      fetch('/reset')\n";
  html += "        .then(response => {\n";
  html += "          alert('Reiniciando ESP32...');\n";
  html += "          setTimeout(function() {\n";
  html += "            alert('El ESP32 se está reiniciando. Por favor, espera unos segundos y recarga la página.');\n";
  html += "          }, 1000);\n";
  html += "        })\n";
  html += "        .catch(error => {\n";
  html += "          // Si hay un error, es probable que sea porque el ESP32 ya se reinició\n";
  html += "          alert('El ESP32 se está reiniciando. Por favor, espera unos segundos y recarga la página.');\n";
  html += "        });\n";
  html += "    });\n";
  html += "    const canvas1 = document.getElementById('graphCanvas1');\n";
  html += "    const canvas2 = document.getElementById('graphCanvas2');\n";
  html += "    const canvas3 = document.getElementById('graphCanvas3');\n";
  html += "    const ctx1 = canvas1.getContext('2d');\n";
  html += "    const ctx2 = canvas2.getContext('2d');\n";
  html += "    const ctx3 = canvas3.getContext('2d');\n";
  html += "    const maxPoints = 150;\n";
  html += "    let data1 = [];\n";
  html += "    let data2 = [];\n";
  html += "    let data3 = [];\n";
  html += "    \n";
  html += "    function resizeCanvas() {\n";
  html += "      const containers = document.querySelectorAll('.chart-container');\n";
  html += "      canvas1.width = containers[0].clientWidth;\n";
  html += "      canvas1.height = containers[0].clientHeight;\n";
  html += "      canvas2.width = containers[1].clientWidth;\n";
  html += "      canvas2.height = containers[1].clientHeight;\n";
  html += "      canvas3.width = containers[2].clientWidth;\n";
  html += "      canvas3.height = containers[2].clientHeight;\n";
  html += "      drawGraphs();\n";
  html += "    }\n";
  html += "    \n";
  html += "    window.addEventListener('resize', resizeCanvas);\n";
  html += "    resizeCanvas();\n";
  html += "    \n";
  html += "    function drawGraph(canvas, ctx, data, color) {\n";
  html += "      const width = canvas.width;\n";
  html += "      const height = canvas.height;\n";
  html += "      const padding = 30;\n";
  html += "      \n";
  html += "      ctx.clearRect(0, 0, width, height);\n";
  html += "      \n";
  html += "      // Dibujar ejes\n";
  html += "      ctx.beginPath();\n";
  html += "      ctx.strokeStyle = '#333';\n";
  html += "      ctx.moveTo(padding, padding);\n";
  html += "      ctx.lineTo(padding, height - padding);\n";
  html += "      ctx.lineTo(width - padding, height - padding);\n";
  html += "      ctx.stroke();\n";
  html += "      \n";
  html += "      // Dibujar líneas de cuadrícula\n";
  html += "      ctx.strokeStyle = '#eee';\n";
  html += "      ctx.beginPath();\n";
  html += "      for (let i = 1; i < 5; i++) {\n";
  html += "        const y = padding + (height - 2 * padding) * i / 5;\n";
  html += "        ctx.moveTo(padding, y);\n";
  html += "        ctx.lineTo(width - padding, y);\n";
  html += "      }\n";
  html += "      ctx.stroke();\n";
  html += "      \n";
  html += "      // Dibujar línea de datos\n";
  html += "      if (data.length <= 1) return;\n";
  html += "      \n";
  html += "      // Encontrar valores máximos y mínimos\n";
  html += "      let max = Math.max(...data);\n";
  html += "      let min = Math.min(...data);\n";
  html += "      min = Math.floor(min);\n";
  html += "      max = Math.ceil(max);\n";
  html += "      if (max === min) {\n";
  html += "        max += 1;\n";
  html += "        min -= 1;\n";
  html += "      }\n";
  html += "      \n";
  html += "      // Dibujar etiquetas del eje Y\n";
  html += "      ctx.fillStyle = '#333';\n";
  html += "      ctx.font = '10px Arial';\n";
  html += "      ctx.textAlign = 'right';\n";
  html += "      ctx.textBaseline = 'middle';\n";
  html += "      for (let i = 0; i <= 5; i++) {\n";
  html += "        const y = height - padding - (i / 5) * (height - 2 * padding);\n";
  html += "        const value = min + (i / 5) * (max - min);\n";
  html += "        ctx.fillText(value.toFixed(1), padding - 5, y);\n";
  html += "      }\n";
  html += "      \n";
  html += "      ctx.beginPath();\n";
  html += "      ctx.strokeStyle = color;\n";
  html += "      ctx.lineWidth = 2;\n";
  html += "      \n";
  html += "      for (let i = 0; i < data.length; i++) {\n";
  html += "        const x = padding + (i * (width - 2 * padding) / (maxPoints - 1));\n";
  html += "        const valueRange = max - min;\n";
  html += "        const normalized = (data[i] - min) / valueRange;\n";
  html += "        const y = height - padding - (normalized * (height - 2 * padding));\n";
  html += "        \n";
  html += "        if (i === 0) {\n";
  html += "          ctx.moveTo(x, y);\n";
  html += "        } else {\n";
  html += "          ctx.lineTo(x, y);\n";
  html += "        }\n";
  html += "      }\n";
  html += "      ctx.stroke();\n";
  html += "    }\n";
  html += "    \n";
  html += "    function drawGraphs() {\n";
  html += "      drawGraph(canvas1, ctx1, data1, 'red');\n";
  html += "      drawGraph(canvas2, ctx2, data2, 'green');\n";
  html += "      drawGraph(canvas3, ctx3, data3, 'blue');\n";
  html += "    }\n";
  html += "    \n";
  html += "    function updateData() {\n";
  html += "      fetch('/data')\n";
  html += "        .then(response => response.json())\n";
  html += "        .then(newData => {\n";
  html += "          data1.push(newData.var1);\n";
  html += "          data2.push(newData.var2);\n";
  html += "          data3.push(newData.var3);\n";
  html += "          \n";
  html += "          if (data1.length > maxPoints) {\n";
  html += "            data1.shift();\n";
  html += "            data2.shift();\n";
  html += "            data3.shift();\n";
  html += "           location.reload();\n";
  html += "          }\n";
  html += "          \n";
  html += "          document.getElementById('val1').textContent = newData.var1.toFixed(2);\n";
  html += "          document.getElementById('val2').textContent = newData.var2.toFixed(2);\n";
  html += "          document.getElementById('val3').textContent = newData.var3.toFixed(2);\n";
  html += "          \n";
  html += "          drawGraphs();\n";
  html += "        })\n";
  html += "        .catch(error => console.error('Error:', error));\n";
  html += "    }\n";
  html += "    \n";
  html += "    // Iniciar la actualización de datos\n";
  html += "    setInterval(updateData, 100);\n";
  html += "    updateData();\n";
  html += "  </script>\n";
  html += "</body>\n";
  html += "</html>";
  
  server.send(200, "text/html", html);
}

// Función para proporcionar información del dispositivo
void handleDeviceInfo() {
  // Crear respuesta JSON
  uint64_t chipId = ESP.getEfuseMac();

  String json = "{\"mac\":\"" + String(chipId, HEX) + 
                "\",\"battery\":" + String(vBat(), 2) + 
                ",\"mode\":\"" + currentMode + "\"}";
  
  server.send(200, "application/json", json);
}

// Función para cambiar el modo
void handleSetMode() {
  if (server.hasArg("mode")) {
    String newMode = server.arg("mode");
    if (newMode == "WiFi" || newMode == "LoRa") {
      currentMode = newMode;

      saveSetting(newMode, "/mode.txt");
      Serial.println(newMode);
      
      // Aquí puedes implementar la lógica para cambiar realmente el modo
      // Por ejemplo, inicializar el módulo LoRa o cambiar configuraciones
      
      server.send(200, "application/json", "{\"success\":true, \"mode\":\"" + currentMode + "\"}");
    } else {
      server.send(400, "application/json", "{\"success\":false, \"error\":\"Modo no válido\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false, \"error\":\"Parámetro 'mode' no especificado\"}");
  }
}

void handleReset() {
  server.send(200, "text/plain", "Reiniciando ESP32...");
  delay(500);  // Pequeña pausa para permitir que se envíe la respuesta
  ESP.restart(); // Reiniciar el ESP32
}

void handleData() {
  
  String SdevEui = "";
  for (int i = 0; i < 8; i++) {
    SdevEui += String(devEui[i], HEX);
    SdevEui += " ";
  }

  String SappSKey1 = "";
  String SappSKey2 = "";
  for (int i = 0; i < 8; i++) {
    SappSKey1 += String(appSKey[i], HEX);
    SappSKey1 += " ";
    SappSKey2 += String(appSKey[8 + i], HEX);
    SappSKey2 += " ";
  }

  uint16_t DIST = 0;
  if(typeLidar == "0"){
    DIST = getDistance0() * 0.393701;
  }else{
    DIST = getDistance1() * 0.393701;
  }


  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float xacc = a.acceleration.x;
  float load = scale.get_value(5) * 0.01;
  load = map(load, 274, 440, 0, 188);

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(10, 0, "D: "); display.drawString(25, 0, String(DIST));
  display.drawString(10, 9, "L: "); display.drawString(25, 9, String(load,0));
  display.drawString(10, 18, "A: "); display.drawString(25, 18, String(xacc,2));
  display.drawString(0, 30, String(SdevEui));
  display.drawString(0, 39, String(SappSKey1));
  display.drawString(0, 48, String(SappSKey2));
  display.display();
  
  // Crear respuesta JSON
  String json = "{\"var1\":" + String(load,2) + 
                ",\"var2\":" + String(DIST) + 
                ",\"var3\":" + String(xacc,2) + "}";
  
  server.send(200, "application/json", json);
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  //Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    //Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    //Serial.println("- file written");
  } else {
    //Serial.println("- write failed");
  }
  file.close();
}

String readFile(fs::FS &fs, const char * path) {
  //Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    //Serial.println("- empty file or failed to open file");
    return String();
  }
  //Serial.println("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  //Serial.println(fileContent);
  return fileContent;
}

void saveSetting(String value, String fil){
  String orValue = readFile(SPIFFS, fil.c_str());
  if(orValue != value ){
    writeFile(SPIFFS, fil.c_str(), value.c_str());
  }
}

void leds(String color, bool n){
  for(uint8_t i = 0; i < n; i++){
    if(color = "R"){
      digitalWrite(lR,HIGH);
    }else if(color = "G"){
      digitalWrite(lG,HIGH);
    }
    else if(color = "B"){
      digitalWrite(lB,HIGH);
    }
    delay(500);
    digitalWrite(lR,LOW);
    digitalWrite(lG,LOW);
    digitalWrite(lB,LOW);
  }
}

/* =========================== GET SETTINGS ============================= */
void getHttp() {
  int err = 0;

  HTTPClient http;
  uint64_t chipId = ESP.getEfuseMac();
  String mac = String(chipId,HEX);
  SearchPath = SearchPath + mac;

  //String SERVER = "192.168.3.31";
  //String Port = "8000";
  String SERVER = String(broker);
  String Port = "80";

  Serial.println(SERVER);
  Serial.println(SearchPath);
  Serial.println(Port);

  String msg_in = "";
  String urlComplet = "http://" + String(SERVER) + ":" + String(Port) + SearchPath;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(urlComplet);
    http.begin(urlComplet);
    err = http.GET();
    if (err > 0) {
      msg_in = http.getString();
      Serial.println("Código HTTP: " + String(err));
    } else {
      Serial.println("Error en petición GET");
    }
    
    http.end();

    Serial.println(msg_in);
  }

  JsonDocument docIn;
  DeserializationError error = deserializeJson(docIn, msg_in);
  if (error) {
    return;
  }
  const char* deviceName = docIn["DeviceName"];
  const char* deviceMacAddress = docIn["DeviceMacAddress"];
  boolean RunNNdevice = docIn["RunNNdevice"];
  int sleepTime = docIn["SamplingRate"];
  runNN = RunNNdevice;

  appTxDutyCycle = sleepTime * 1000;

  Serial.println(deviceName);
  Serial.println(deviceMacAddress);
  Serial.println(runNN);
  Serial.println(appTxDutyCycle);

}

/* == OLED DISPLAY == */
void oledDisplay(uint8_t line, String text) {
  VextON();
  display.init();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, line*10, text.c_str());
  display.display();
}

/* == DISTANCE2 == */
uint16_t getDistance1(){
  unsigned char SLAVE_ADDRESS = 0x10;
  uint8_t DATA_LENGTH = 9;
  unsigned char buf1[] = {0x5A,0x05,0x00,0x01,0x60};
  Wire1.beginTransmission(SLAVE_ADDRESS);
  Wire1.write(buf1,5);
  Wire1.endTransmission();
  Wire1.requestFrom(SLAVE_ADDRESS, DATA_LENGTH);
  uint8_t data[DATA_LENGTH] = {0}; 
  uint16_t distance = 0, strength = 0;
  int index = 0;
  while (Wire1.available() > 0 && index < DATA_LENGTH) {
    data[index++] = Wire1.read(); // Read data into an array
  }
  if (index == DATA_LENGTH) {
    distance = data[2] + data[3] * 256;
  }
  return distance;
}

uint16_t getDistance00() {
  //lidarSerial.write(buf1,5);
  //lidarSerial.flush();
  Serial.println("---------");
  uint16_t distance;

  uint8_t TFbuff[9] = { 0 };
  long checksum = 0;
  while (lidarSerial.available()) {
    TFbuff[0] = lidarSerial.read();
    checksum += TFbuff[0];
    if (TFbuff[0] == 'Y') {
      TFbuff[1] = lidarSerial.read();
      checksum += TFbuff[1];
      if (TFbuff[1] == 'Y') {
        for (int i = 2; i < 8; i++) {
          TFbuff[i] = lidarSerial.read();
          checksum += TFbuff[i];
        }
        TFbuff[8] = lidarSerial.read();
        checksum &= 0xff;
        if (checksum == TFbuff[8]) {
          distance = TFbuff[2] + TFbuff[3] * 256;
          //strength = TFbuff[4] + TFbuff[5] * 256;
          return distance;
        } else {
          checksum = 0;
        }
      } else {
        checksum = 0;
      }
    } else {
      checksum = 0;
    }
  }

  return -1;
}

uint16_t getDistance0() {
  int valorActual;

  while (1) {
    if (lidarSerial.available() > 0) {
      byte d = lidarSerial.read();
      uint8_t buf[9] = { 0 };
      lidarSerial.readBytes(buf, 9);  // Read 9 bytes of data
      if (buf[0] == 0x59 && buf[1] == 0x59) {
        valorActual = buf[2] + buf[3] * 256;
        if(valorActual < 1500){
          break;
        }
      }
    }
  }

  return valorActual;
}

/* == vbat == */
float vBat(){
  long raw = map(analogRead(pinBattery),698,2416,199,408);
  return raw * 0.01;
}

/* ========================= TESTING RUN ========================= */
void testTask() {

  // Configurar el ESP32 como punto de acceso WiFi
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Dirección IP del punto de acceso: ");
  Serial.println(IP);
  
  // Configurar rutas del servidor
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/deviceInfo", handleDeviceInfo);
  server.on("/setMode", handleSetMode);
  server.on("/reset", handleReset);
  
  // Iniciar el servidor
  server.begin();
  Serial.println("Servidor HTTP iniciado");

  /*---------------*/

  VextON();
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();

  while (1) {
    server.handleClient();
    //delay(20);
  }
  ESP.restart();
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

/* ========================= TX FOR LORA MODE ========================= */
static void prepareTxFrame(uint8_t port) {

  oledDisplay(1, "LORAWAN mode");
  appDataSize = 0;

  int16_t vBatInt = (int16_t)(vBat() * 100);
  appData[appDataSize++] = (vBatInt >> 8) & 0xFF;
  appData[appDataSize++] = vBatInt & 0xFF;

  appData[appDataSize++] = status;

  if(status == 1 && integrated == 1){
    fillPump();
    diagnosis();

    int16_t spmInt = (int16_t)(spm * 10);
    appData[appDataSize++] = (spmInt >> 8) & 0xFF;
    appData[appDataSize++] = spmInt & 0xFF;

    int16_t sLengthInt = (int16_t)(sLength * 100);
    appData[appDataSize++] = (sLengthInt >> 8) & 0xFF;
    appData[appDataSize++] = sLengthInt & 0xFF;

    int16_t fillPumpInt = (int16_t)(fill * 100);
    appData[appDataSize++] = (fillPumpInt >> 8) & 0xFF;
    appData[appDataSize++] = fillPumpInt & 0xFF;

    for(uint8_t i = 0; i < 10; i++){
      int16_t diag = (int16_t)(nnDiagnosis[i] * 100);
      appData[appDataSize++] = (diag >> 8) & 0xFF;
      appData[appDataSize++] = diag & 0xFF;
    }

    // load data
    for(uint8_t i = 0; i < 20; i++){
      int16_t tData = (int16_t)(load20.get(i));
      appData[appDataSize++] = (tData >> 8) & 0xFF;
      appData[appDataSize++] = tData & 0xFF;
    }
    // pos data
    for(uint8_t i = 0; i < 20; i++){
      int16_t tData = (int16_t)(pos20.get(i) * 100);
      appData[appDataSize++] = (tData >> 8) & 0xFF;
      appData[appDataSize++] = tData & 0xFF;
    }
  }
  Serial.println(appDataSize);
}

/* =========================  TX FOR WIFI MODE ========================= */
void wifiTx(){
  oledDisplay(1, "WIFI mode");
  leds("B",3);

  oledDisplay(1, "get config");

  /* ========== GET CONFIG FROM SERVER ========== */
  int count = 0;
  WiFi.begin(runssid.c_str(), runpassword.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    if (count == 5){
      oledDisplay(1, "no connect ");
      ESP.restart();
    }
    delay(5000);
    count++;
  }
  client.setServer(broker, mqtt_port);
  getHttp();

  /* ========== PREPARE PAYLOAD ========== */

  JsonDocument docOut;

  uint64_t chipId = ESP.getEfuseMac();

  docOut["type"] = "wellAnalizer";
  docOut["devEUI"] = String(chipId, HEX);
  docOut["vBat"] = String(vBat(),2);
  docOut["sLength"] = String(sLength,2);
  docOut["status"] = String(status);

  if(status == 1 && integrated == 1){
    /* ===== IF RUNNING MODE IN DEVICE ACTIVATED ===== */
    if(runNN == true){
      fillPump();
      diagnosis();

      Serial.println(String(fill));
      docOut["fillPump"] = String(fill * 100,2);

      String sDiagnosis = "";
      for(uint8_t i = 0; i < 9; i++){
        sDiagnosis += String(nnDiagnosis[i],3) + ",";
      }
      sDiagnosis += String(nnDiagnosis[9],3);
      docOut["diagnosis"] = sDiagnosis;
    }

    String sLoad = "";
    String sPos = "";
    for(uint8_t i = 0; i < 49; i++){
      sLoad += String(load50.get(i)) + ",";
      sPos += String(pos50.get(i),2) + ",";
    }
    sLoad += String(load50.get(49));
    sPos += String(pos50.get(49),2);

    docOut["load"] = sLoad;
    docOut["pos"] = sPos;
  }

  String payload = "";
  serializeJson(docOut, payload);

  count = 0;

  while (!client.connected()) {
    String client_id = "esp32-client-";
    
    client_id += String(chipId,HEX);
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      Serial.println("MQTT broker connected");
      oledDisplay(2, "Broker no connected ");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      if (count == 5)
      {
        //Serial.println("reset mqtt");
        ESP.restart();
      }
      delay(5000);
    }
    count++;
  }
  
  Serial.println(payload);
  client.publish(topicPublish, payload.c_str());
}

/* ========================= NEURAL NETWORKS FUNCTIONS AND VARIABLES ========================= */
float relu(float n)
{
  if (n >= 0)
    return n;
  else if (n < 0)
    return 0;
}

float sigmoid(float n)
{
  return 1.0 / (1.0 + exp(-n));
}

void normArray(float array[], int longitud) {
  float minVal = array[0];
  float maxVal = array[0];
  
  for(int i = 1; i < longitud; i++) {
    if(array[i] < minVal) {
      minVal = array[i];
    }
    if(array[i] > maxVal) {
      maxVal = array[i];
    }
  }

  if(maxVal == 0 && minVal == 0) {
    for(int i = 0; i < longitud; i++) {
      array[i] = 0.0; // Mantener los ceros
    }
    return;
  }
  
  float rango = maxVal - minVal;

  for(int i = 0; i < longitud; i++) {
    array[i] = (array[i] - minVal) / rango;
  }
}

void fillPump() {
  //float a0[50];
  //float a0[50] = {2.629,3.635,5.881,7.868,7.808,7.905,7.761,7.854,7.791,7.875,7.887,8.065,8.084,8.052,8.097,8.076,8.129,8.191,8.23,8.27,8.461,8.598,8.71,8.935,9.0,7.494,5.19,5.154,5.179,5.357,4.997,2.353,0.256,0.0,0.083,0.094,0.133,0.204,0.161,0.27,0.325,0.36,0.545,0.554,0.619,0.587,0.526,0.485,0.412,0.713};
  normArray(a0,50);

  float W1[15][50] = { { 0.116, -0.138, 0.087, 0.401, 0.003, 0.141, 0.118, 0.451, 0.406, -0.111, 0.082, -0.231, -0.154, 0.188, 0.356, 0.108, -0.209, -0.35, -0.294, -0.064, -0.31, 0.163, 0.351, 0.049, 0.226, 0.089, 0.002, -0.236, 0.118, 0.105, 0.217, 0.22, 0.047, 0.194, 0.575, 0.568, 0.268, -0.105, 0.355, 0.208, 0.168, -0.009, 0.383, -0.242, -0.321, -0.38, 0.07, -0.357, 0.46, 0.187 }, { 0.354, -0.002, 0.153, -0.097, 0.072, -0.139, -0.295, -0.137, -0.128, 0.268, -0.052, 0.011, 0.211, 0.027, 0.092, -0.337, -0.378, -0.157, 0.043, -0.04, 0.224, -0.021, -0.076, -0.051, 0.016, -0.234, -0.299, 0.125, -0.167, -0.091, -0.084, 0.063, 0.207, -0.15, -0.307, -0.045, 0.033, -0.047, -0.1, -0.274, 0.148, -0.261, 0.102, -0.039, -0.168, 0.23, -0.09, 0.166, -0.228, -0.395 }, { -0.118, -0.106, 0.266, 0.263, -0.064, -0.246, -0.329, -0.103, 0.2, -0.157, -0.194, -0.079, -0.219, 0.072, 0.332, -0.131, -0.235, -0.303, -0.112, -0.182, -0.119, 0.031, 0.215, -0.198, 0.032, -0.007, 0.016, 0.065, 0.296, -0.08, -0.008, 0.282, -0.235, 0.083, 0.327, -0.183, -0.019, -0.192, -0.231, 0.153, -0.274, -0.135, -0.232, 0.095, -0.055, -0.346, -0.175, -0.167, -0.289, 0.11 }, { -0.138, 0.309, 0.05, 0.019, -0.123, -0.225, -0.4, -0.084, -0.078, -0.221, -0.393, 0.055, 0.511, 0.287, 0.221, 0.292, 0.231, 0.034, 0.263, 0.229, 0.277, 0.483, 0.14, 0.182, 0.44, 0.13, 0.018, -0.035, 0.061, -0.363, 0.1, -0.568, -0.176, -0.046, 0.063, -0.255, -0.237, -0.178, 0.211, 0.198, 0.298, 0.187, 0.063, 0.216, 0.622, 0.31, -0.156, 0.514, -0.158, -0.036 }, { 0.392, -0.004, -0.346, -0.111, -0.284, -0.1, 0.257, 0.187, 0.113, -0.358, -0.111, -0.074, 0.416, 0.016, 0.237, 0.433, 0.46, 0.42, 0.093, -0.037, 0.14, 0.291, 0.409, -0.082, -0.1, -0.226, 0.112, -0.187, -0.149, -0.136, -0.114, -0.516, -0.259, -0.134, -0.09, -0.248, -0.349, -0.425, -0.327, -0.292, -0.161, 0.197, 0.179, 0.278, 0.226, 0.378, 0.231, 0.255, -0.39, -0.282 }, { -0.094, 0.049, 0.083, 0.087, -0.08, 0.221, -0.263, -0.096, -0.009, 0.036, -0.347, 0.18, -0.334, -0.123, 0.24, -0.199, -0.386, 0.079, -0.333, -0.231, 0.402, -0.113, -0.097, 0.333, -0.107, 0.169, -0.285, -0.206, -0.43, 0.228, -0.024, -0.302, -0.222, 0.054, 0.132, 0.357, -0.404, 0.045, 0.084, -0.122, 0.32, -0.337, 0.226, -0.192, 0.015, -0.15, 0.091, 0.402, 0.305, -0.043 }, { -0.16, 0.219, 0.444, 0.359, 0.51, -0.203, -0.051, -0.176, 0.134, -0.037, 0.325, 0.314, -0.484, 0.103, 0.023, -0.08, 0.128, -0.18, 0.166, -0.442, -0.317, -0.404, -0.03, 0.38, 0.309, -0.096, 0.061, 0.195, 0.333, 0.251, 0.155, 0.278, 0.044, 0.252, 0.265, 0.558, 0.057, 0.009, 0.413, -0.06, -0.061, 0.133, -0.252, 0.077, -0.736, -0.367, 0.261, -0.314, 0.664, 0.186 }, { 0.308, 0.054, 0.091, -0.177, -0.179, -0.121, -0.698, -0.484, 0.063, -0.235, -0.344, 0.052, -0.288, 0.105, 0.422, 0.076, 0.351, -0.061, 0.325, 0.027, 0.101, 0.418, 0.141, -0.173, -0.241, -0.222, -0.206, -0.204, -0.421, 0.003, 0.244, 0.026, 0.064, 0.051, 0.605, 0.462, 0.243, 0.1, 0.185, 0.83, 0.663, 0.02, 0.38, 0.223, -0.162, -0.399, -0.716, -0.313, 1.197, 0.333 }, { -0.13, 0.167, -0.106, -0.533, -0.209, -0.008, 0.154, 0.024, 0.229, 0.247, -0.244, -0.109, -0.013, -0.028, 0.146, 0.344, 0.282, 0.187, 0.333, -0.21, 0.492, -0.255, 0.377, 0.338, 0.282, 0.343, -0.036, 0.297, -0.186, -0.216, -0.257, -0.2, -0.014, -0.31, -0.215, -0.59, -0.224, -0.42, -0.015, -0.196, -0.05, -0.154, -0.375, 0.113, 0.782, 0.264, -0.04, -0.079, -0.707, -0.12 }, { -0.073, 0.227, -0.274, 0.308, -0.199, 0.016, -0.155, 0.005, -0.149, -0.124, -0.227, -0.061, -0.419, 0.281, -0.18, 0.106, 0.106, -0.327, 0.197, 0.24, -0.189, -0.238, -0.197, 0.202, -0.054, -0.154, -0.008, -0.049, -0.278, 0.052, -0.332, 0.05, 0.025, -0.05, 0.286, -0.004, 0.074, -0.332, -0.112, -0.028, 0.039, 0.148, 0.019, -0.07, -0.028, 0.007, -0.317, 0.394, -0.425, 0.288 }, { 0.051, 0.265, 0.131, 0.063, 0.137, 0.256, 0.196, 0.015, -0.297, -0.096, -0.123, -0.308, -0.284, -0.341, -0.17, 0.099, -0.125, -0.059, 0.129, 0.117, -0.077, -0.016, 0.132, -0.039, 0.069, -0.025, -0.079, -0.267, 0.205, -0.202, -0.018, -0.025, -0.013, -0.149, -0.035, -0.244, -0.226, -0.002, -0.372, -0.131, 0.075, -0.138, -0.206, -0.219, -0.204, 0.185, -0.269, 0.302, -0.418, -0.067 }, { -0.215, 0.172, 0.082, 0.224, 0.026, -0.216, 0.047, -0.124, -0.126, -0.115, -0.08, 0.204, 0.219, -0.048, 0.125, -0.36, 0.292, -0.184, -0.451, -0.071, 0.157, 0.023, -0.381, 0.082, -0.192, 0.039, 0.174, -0.197, -0.178, 0.03, -0.086, 0.449, -0.234, -0.1, 0.201, 0.258, 0.15, -0.442, 0.004, 0.185, 0.025, -0.002, 0.066, -0.261, -0.202, 0.154, -0.274, -0.064, -0.437, -0.38 }, { -0.09, -0.06, -0.213, 0.097, -0.368, -0.23, -0.099, 0.24, -0.285, -0.076, -0.088, 0.355, 0.195, 0.056, -0.248, 0.061, -0.068, -0.207, -0.287, -0.107, -0.444, -0.043, 0.174, 0.371, -0.09, -0.432, 0.112, -0.038, 0.054, 0.121, 0.072, 0.104, -0.193, 0.096, -0.052, 0.035, -0.142, 0.253, -0.21, 0.355, 0.147, -0.357, -0.259, -0.346, -0.14, -0.321, -0.329, 0.183, -0.175, -0.33 }, { 0.291, 0.158, -0.186, 0.292, -0.264, -0.17, -0.051, -0.387, -0.11, -0.393, 0.042, -0.118, 0.216, 0.133, -0.189, -0.154, 0.046, -0.17, -0.208, -0.009, 0.011, -0.386, -0.147, 0.028, 0.184, 0.117, 0.225, 0.12, -0.009, 0.324, 0.633, 0.696, 0.381, 0.375, 0.369, 0.549, 0.027, 0.094, 0.001, 0.323, 0.073, -0.011, 0.463, -0.204, -0.431, -0.282, -0.001, -0.496, 0.872, 0.412 }, { 0.129, -0.175, -0.11, -0.32, -0.181, 0.422, 0.145, 0.177, 0.252, 0.277, -0.173, 0.203, 0.182, -0.03, 0.198, 0.192, 0.06, 0.009, 0.229, 0.455, 0.415, 0.127, -0.109, 0.278, 0.173, -0.032, -0.116, -0.273, -0.476, -0.006, -0.022, -0.205, 0.146, -0.256, -0.222, -0.27, 0.062, 0.13, -0.222, -0.279, 0.237, -0.089, 0.306, -0.009, 0.669, 0.443, 0.117, -0.189, -0.396, -0.123 } };
  float a1[15];
  float W2[1][15] = { { -0.325, -0.008, 0.504, 0.72, 0.614, 0.144, -0.156, -1.291, 0.525, 0.105, 0.066, -0.272, 0.217, -0.32, 0.357 } };
  float a2[1];
  float b1[15] = { 0.039, 0.0, -0.056, -0.003, -0.058, 0.0, 0.054, -0.05, -0.072, -0.03, 0.0, 0.0, -0.047, -0.038, -0.049 };
  float b2[1] = { -0.031 };
  float aux = 0.0;

  for (int i = 0; i < 15; i++) {
    aux = 0.0;
    for (int j = 0; j < 50; j++) { aux = aux + W1[i][j] * a0[j]; }
    a1[i] = relu(aux + b1[i]);
  }
  for (int i = 0; i < 1; i++) {
    aux = 0.0;
    for (int j = 0; j < 15; j++) { aux = aux + W2[i][j] * a1[j]; }
    a2[i] = sigmoid(aux + b2[i]);
  }


  /* manejo de errores */
  if(a2[0] < 0){
    fill = 0;
  }else if(a2[0] > 1){
    fill = 1;
  }else{
    fill = a2[0];
    if(fill <= 30){
      leds("R",5);
    }else if(fill > 30 && fill < 70){
      leds("B",5);
    }else{
      leds("G",5);
    }
  }
}

void diagnosis() {
  //float a0[50] = {2.629,3.635,5.881,7.868,7.808,7.905,7.761,7.854,7.791,7.875,7.887,8.065,8.084,8.052,8.097,8.076,8.129,8.191,8.23,8.27,8.461,8.598,8.71,8.935,9.0,7.494,5.19,5.154,5.179,5.357,4.997,2.353,0.256,0.0,0.083,0.094,0.133,0.204,0.161,0.27,0.325,0.36,0.545,0.554,0.619,0.587,0.526,0.485,0.412,0.713};
  normArray(a0,50);

  float W1[25][50] = { { 0.0755, 0.0329, -0.1383, 0.0467, 0.2215, -0.0807, 0.2073, 0.0523, -0.2558, 0.3212, 0.1519, 0.2436, -0.2785, -0.0971, -0.1351, -0.3684, -0.0904, 0.2216, 0.2327, 0.3437, -0.3905, -0.127, -0.2218, 0.1722, -0.2197, 0.2917, -0.3835, -0.2932, -0.4477, -0.3297, -0.2362, -0.274, 0.0441, -0.0975, -0.0871, 0.2418, -0.1267, 0.1557, -0.0539, 0.0045, -0.1237, -0.0709, 0.0944, -0.2018, 0.2882, 0.0049, 0.3462, -0.1692, -0.3792, -0.3468 }, { 0.3023, 0.0471, -0.2249, 0.1239, -0.0611, 0.0955, -0.2201, 0.0417, 0.2335, -0.2862, -0.0289, -0.2307, 0.1427, 0.1455, 0.1616, -0.2997, -0.1082, 0.2647, 0.3304, -0.0866, -0.2222, -0.1856, 0.0172, -0.2344, -0.299, -0.155, -0.1114, 0.0715, 0.0313, 0.1916, -0.0029, 0.0943, -0.0548, -0.3695, 0.0079, 0.0196, 0.1295, 0.0933, 0.133, -0.3309, 0.1026, 0.1639, -0.0351, -0.0999, 0.0338, -0.0656, -0.4221, 0.3636, 0.154, -0.0168 }, { 1.0177, 0.727, -0.1868, -0.1838, 0.2109, 0.3648, 0.4496, 0.3975, 0.3684, 0.3219, 0.5521, 0.1086, 0.3524, -0.1866, -0.3407, -0.2194, -0.3056, -0.0003, 0.371, 0.147, 0.2744, 0.2572, 0.0138, 0.0922, -0.0749, 0.2112, 0.2911, 0.1899, -0.1323, -0.7407, -0.0805, 0.1761, -0.1075, -0.4332, -0.6121, -0.796, -1.2058, -1.5349, -1.0672, 0.0239, 0.35, 0.2807, 0.4927, 0.3602, 0.377, 0.7621, 1.223, 1.7834, 1.4467, 1.8351 }, { -0.1916, 0.0462, 0.0641, 0.0659, -0.2078, 0.1385, -0.2622, -0.115, -0.3187, -0.0805, -0.1458, -0.002, -0.3965, -0.1526, -0.0672, 0.2721, 0.1177, 0.3865, 0.1642, 0.2584, -0.0934, 0.2141, -0.0966, -0.2232, -0.2005, -0.2398, -0.0494, 0.0995, 0.0607, -0.0569, 0.3283, -0.1416, -0.0545, 0.0295, -0.2874, -0.0427, -0.1848, 0.0823, -0.1121, -0.1415, 0.0683, 0.305, 0.0226, 0.2569, -0.286, -0.2612, 0.248, 0.0957, 0.076, -0.0247 }, { 0.2296, -0.0301, 0.0778, -0.29, 0.1562, -0.2694, -0.262, -0.0549, 0.2359, 0.4015, -0.0312, 0.0547, 0.1629, -0.1199, 0.3976, 0.4104, 0.1116, 0.0358, -0.1706, -0.4026, -0.0193, -0.1886, -0.3498, 0.2048, -0.048, -0.2961, -0.1249, -0.2154, -0.2208, 0.3478, 0.0521, -0.3306, -0.445, 0.3174, 0.0141, -0.0278, -0.1809, -0.0431, 0.0465, -0.2812, 0.1195, -0.2168, -0.0239, 0.0182, 0.1427, -0.0797, -0.029, -0.2008, -0.2415, 0.1069 }, { -0.3971, 0.1617, -0.1984, 0.4078, -0.104, -0.3911, 0.3805, 0.2777, 0.1753, -0.3103, -0.0853, 0.1698, 0.0014, 0.0438, -0.3042, 0.1443, -0.0266, -0.2474, 0.017, 0.2485, -0.2107, -0.2044, -0.064, -0.1149, 0.4495, -0.196, 0.0885, -0.4547, -0.1683, 0.0, 0.0998, 0.2511, -0.415, -0.0284, -0.0365, -0.279, -0.1149, 0.2553, -0.1752, -0.0743, -0.4411, 0.2894, 0.1761, -0.1691, -0.119, 0.326, -0.1599, 0.1067, -0.1373, 0.2236 }, { -0.1132, -0.0847, -0.1831, -0.0118, -0.0853, 0.3354, 0.1686, 0.113, 0.1468, -0.23, -0.3785, 0.3161, -0.2681, 0.4344, 0.0158, -0.259, 0.0458, -0.1479, 0.0655, -0.273, -0.1933, 0.0008, -0.0596, -0.1907, -0.0367, 0.0031, -0.0659, -0.2801, 0.194, -0.1745, -0.24, -0.0908, -0.1145, 0.1332, -0.2433, -0.0483, -0.2395, 0.1152, -0.3596, -0.0545, 0.1872, -0.2361, -0.2755, -0.385, 0.1226, -0.0635, -0.4275, 0.3144, 0.0189, -0.2214 }, { -0.1622, 0.0083, -0.2689, 0.395, -0.1121, 0.0637, -0.0667, -0.1009, -0.2866, 0.2953, -0.0331, -0.015, 0.3109, -0.0288, 0.267, -0.1786, 0.0527, -0.3459, -0.2972, 0.1189, 0.0584, -0.2127, -0.0245, -0.0904, 0.3924, 0.0749, -0.0507, -0.2528, -0.0704, -0.2818, 0.098, -0.2533, -0.2021, -0.1023, 0.235, -0.2943, -0.0624, 0.044, 0.0641, 0.2, 0.2063, -0.0454, -0.2993, -0.1603, -0.0031, 0.0025, 0.0225, -0.3957, -0.1685, -0.1773 }, { -0.181, -0.1005, -0.0699, 0.4249, -0.2653, -0.0729, -0.1355, -0.2621, 0.1688, 0.0187, 0.2443, 0.2606, -0.0241, -0.4164, 0.0548, -0.3717, 0.2583, -0.2352, -0.3261, 0.1332, 0.117, 0.2999, 0.1615, -0.1992, -0.162, 0.0831, -0.1176, -0.1854, -0.0176, 0.1429, -0.2724, -0.1333, 0.0139, -0.0386, -0.1352, 0.056, -0.0204, 0.0083, 0.0037, 0.3555, 0.1685, -0.2228, 0.1335, 0.3144, -0.0521, -0.2617, -0.0436, 0.0956, -0.0252, -0.0574 }, { 0.5946, 0.4945, 1.1065, 0.9286, 0.2746, 0.1969, -0.7592, -0.843, -0.5451, -0.1416, -0.3387, -0.0444, 0.1763, 0.1205, -0.1515, -0.1248, -0.4341, 0.2056, 0.13, 0.4569, -0.1974, 0.0022, 0.1336, 0.2631, 0.241, -0.0486, -0.362, 0.2354, 0.9148, 1.1888, 0.6309, 0.0078, -0.0545, 0.2059, 0.1143, -0.2601, 0.3536, 0.3435, -0.2303, -1.4567, -2.1392, -1.7227, -0.2705, 2.33, 3.397, 2.2131, 1.8, 1.0968, -3.6723, -1.8709 }, { -0.0225, 0.3226, 0.0279, -0.1226, 0.2316, 0.0874, -0.0027, -0.1349, -0.1831, 0.0026, -0.3406, -0.0456, 0.186, -0.3143, -0.0666, -0.2329, 0.1212, -0.1275, -0.0741, 0.0316, 0.0916, 0.0233, -0.2036, -0.0275, 0.32, -0.1973, 0.0768, -0.3202, -0.1147, -0.0315, 0.141, -0.1129, 0.1398, -0.0168, 0.0996, 0.0185, -0.1887, 0.1521, -0.0405, -0.2254, 0.1967, -0.2491, 0.2739, -0.3174, -0.1685, 0.1646, -0.0793, 0.0348, 0.0694, 0.1017 }, { 0.6386, 0.3615, 0.8619, 0.4612, 0.0345, 0.5251, -0.0506, 0.4759, 0.6224, 0.8325, 0.1492, -0.1969, 0.0723, -0.2379, -0.3731, -0.3749, -0.6618, -0.4586, -0.5958, -0.7918, -0.3692, -0.5563, 0.002, -0.2115, -0.1239, -0.0805, -0.1874, 0.4681, 0.9348, 0.9729, 1.1002, 0.6443, 0.552, 0.6496, 1.0139, 0.4103, 0.0489, 0.0547, 0.0643, -0.4349, -0.1149, 0.0132, -0.4354, -1.3668, -1.859, -1.4382, -0.8504, 0.6726, 0.8801, 0.8114 }, { -0.0364, -0.3508, -0.1669, -0.1857, 0.0157, 0.2879, 0.0358, -0.0337, -0.2048, 0.1482, -0.1852, -0.001, -0.1883, 0.0334, 0.03, -0.0011, 0.0233, -0.1544, -0.0171, 0.098, 0.1182, -0.1209, 0.2998, 0.3712, 0.3397, -0.4219, 0.1126, -0.1015, -0.1952, -0.0186, -0.4932, -0.1007, 0.0571, -0.0466, -0.027, 0.1798, -0.1702, -0.2441, 0.0783, -0.2655, 0.048, 0.124, 0.1283, -0.0847, 0.2818, 0.1126, 0.1172, -0.2348, -0.0106, -0.0667 }, { -0.1996, -0.4328, -0.0484, -0.0181, -0.0936, -0.2086, 0.0273, 0.0622, 0.0069, 0.1638, -0.1851, -0.1613, -0.3333, -0.1415, -0.2476, -0.0954, 0.224, -0.3213, 0.0717, 0.3024, -0.1427, 0.2482, 0.1481, 0.1709, -0.0547, -0.0673, -0.2212, 0.2524, -0.012, 0.1412, -0.052, 0.1998, -0.1376, -0.1832, -0.4991, -0.2319, -0.2593, -0.4284, -0.4819, 0.0424, 0.1252, -0.3338, -0.203, 0.3483, -0.0983, -0.4118, -0.0809, -0.0621, 0.0246, -0.1377 }, { -0.1533, -0.4229, 0.1984, -0.0911, 0.3399, -0.031, -0.0213, 0.0154, -0.0115, -0.1095, -0.109, -0.4675, 0.0531, -0.0448, 0.1493, 0.0682, 0.1558, -0.1816, 0.2763, -0.0174, 0.2883, -0.2319, -0.0134, -0.1443, -0.1111, -0.0912, -0.3201, -0.0016, 0.1269, -0.2846, 0.0771, -0.3639, -0.0417, -0.1254, -0.0486, -0.4124, -0.25, -0.1539, -0.2509, -0.0656, -0.127, -0.1729, 0.2591, -0.2072, 0.3313, -0.1171, 0.3857, -0.1559, -0.0353, -0.0284 }, { 0.4751, 0.1282, -0.4017, -0.1676, 0.1483, 0.5183, -0.0983, 0.3611, -0.0443, 0.1239, 0.125, 0.514, 0.7743, 0.3371, 0.6709, 0.3632, 0.6326, 0.3445, 0.0781, 0.5864, 0.698, 0.2766, 0.2146, 0.0547, -0.4333, 0.0929, -0.0892, -0.0158, -0.4122, -0.4522, -0.5722, -0.1409, -0.464, -0.3538, 0.0783, 0.2371, 0.593, 1.2805, 1.526, 2.1798, 1.8143, 1.9322, 1.391, 0.2397, -0.5541, -1.2844, -1.0405, -1.044, 1.0901, 0.0563 }, { -0.1205, -0.0411, -0.0092, 0.0497, -0.1329, 0.0364, 0.0359, 0.3649, -0.0369, 0.3301, -0.3611, 0.1934, 0.0889, -0.1101, -0.1218, -0.1194, 0.0069, 0.2463, -0.3084, 0.119, 0.2831, -0.4786, -0.3354, -0.3098, -0.2491, 0.0537, -0.4153, 0.1748, -0.1656, -0.2932, -0.04, -0.0937, -0.111, 0.0061, -0.2798, 0.218, -0.232, -0.3822, 0.3937, 0.1752, -0.0813, 0.1761, 0.1713, -0.3766, -0.0719, 0.1048, -0.3742, -0.0992, 0.3739, -0.2263 }, { -0.7419, -0.6345, 0.1405, 1.5483, 1.0796, 0.437, 0.376, 0.2722, -0.2057, -0.3358, 0.1277, -0.0193, -0.0911, -0.0055, -0.5242, 0.0735, 0.0686, 0.1496, 0.286, 0.4159, 0.1531, 0.4717, 0.4389, 0.5047, 0.4045, 0.1106, -0.395, -0.2325, -0.2838, -0.5421, -0.596, -0.3154, -1.2085, -0.8744, -0.8279, -0.3504, 0.8475, 0.8918, 1.1671, -0.2539, 0.0143, -0.5594, -0.1093, -0.6256, -0.618, 0.1631, -0.7164, 0.19, 2.3864, 1.3954 }, { -0.1115, -0.0444, 0.229, -0.4005, -0.1547, -0.0144, -0.2159, 0.1397, 0.1798, 0.4486, -0.2525, -0.2274, 0.0737, 0.3932, 0.1681, 0.3246, 0.1446, -0.3633, -0.3537, 0.4431, -0.0437, -0.3728, -0.181, -0.0139, -0.4324, 0.1383, -0.0427, -0.064, -0.2077, -0.0817, 0.2947, -0.4126, -0.1514, 0.0842, -0.0661, -0.0392, 0.0352, 0.0094, 0.3111, 0.297, -0.0603, 0.1018, -0.1609, -0.1377, -0.1855, 0.0078, -0.0096, 0.1855, 0.1162, -0.0584 }, { 0.0009, 0.3072, -0.1672, 0.3023, 0.1081, 0.0536, -0.0966, 0.0158, -0.0186, -0.3452, -0.3083, -0.1166, -0.0691, 0.0114, -0.2318, 0.173, 0.1257, -0.0681, -0.0963, -0.0192, 0.1597, 0.0669, -0.037, 0.0464, -0.3015, 0.0453, -0.3455, 0.0573, 0.028, 0.0379, -0.1255, 0.0649, -0.1927, -0.114, -0.1446, 0.1725, -0.0863, 0.0408, -0.1272, -0.2969, 0.2425, 0.1744, 0.1275, -0.0662, -0.1427, -0.1921, 0.2882, -0.1545, 0.2697, -0.2763 }, { 1.239, 0.8499, 0.6598, 0.0463, 0.0708, -0.2646, -0.0053, 0.0992, -0.0849, -0.2644, -0.0811, 0.4857, 0.3414, 0.1485, 0.3419, 0.2403, -0.0044, 0.5993, 0.3097, -0.0944, 0.545, 0.2688, 0.3658, 0.0573, 0.3648, 0.5349, 0.2721, 0.1628, 0.5294, 0.7993, 0.8024, 0.7487, 0.3719, 0.3706, 0.7422, 0.4132, -0.4208, -1.1765, -1.089, -0.5263, -0.0207, -0.0142, -0.2076, -1.4849, -1.398, -0.5173, 0.0313, 0.7088, 2.0699, 2.0142 }, { -0.1049, -0.0581, -0.0967, 0.0056, 0.1275, -0.0189, 0.0775, 0.1415, 0.2711, -0.1904, -0.4027, 0.1522, -0.4051, -0.2448, 0.1064, -0.3242, 0.0704, -0.1576, 0.0393, -0.1646, 0.0868, 0.1781, 0.0399, -0.0596, -0.0033, -0.2042, 0.1555, -0.1676, -0.1798, -0.0479, 0.1565, 0.2066, -0.2163, -0.3277, 0.0304, 0.1589, 0.1162, 0.0344, 0.0456, 0.1303, -0.1012, -0.3005, 0.1279, -0.1458, 0.1546, -0.2619, -0.0358, 0.1976, 0.0839, 0.3011 }, { -0.0529, 0.1783, -0.4752, -0.1288, -0.138, -0.1081, 0.0768, 0.1766, -0.0547, -0.0603, -0.4742, 0.2534, -0.0062, -0.113, -0.0709, -0.237, -0.158, 0.1734, -0.1621, 0.1111, -0.1681, 0.2819, 0.0608, -0.2142, 0.2204, -0.0126, -0.443, 0.2477, 0.1927, -0.4271, 0.0506, 0.2997, 0.1754, 0.0892, -0.0665, -0.3825, -0.0591, -0.1739, -0.1495, -0.0486, -0.001, 0.125, -0.3469, 0.0637, 0.0522, -0.0755, -0.0956, -0.0744, -0.235, -0.3519 }, { 0.2343, -0.041, 0.0044, 0.0531, -0.1768, 0.4654, 0.4785, 0.4186, 0.2343, 0.2194, 0.1754, -0.0381, 0.2707, 0.2927, 0.1586, 0.3334, -0.1708, 0.0891, 0.0153, 0.1563, 0.0384, 0.0018, -0.3727, -0.4356, -0.677, -0.1427, -0.1264, -0.426, -0.4537, -0.6396, -0.3126, 0.0501, 0.1694, 0.9083, 1.2994, 1.0793, 0.7507, 0.9536, 1.1659, 0.442, -0.6747, 0.4724, 0.4168, 1.0053, 2.1641, 0.505, 0.3757, -0.5138, -2.6373, -1.6633 }, { -0.3876, 0.0946, -0.9623, -1.0435, -0.3241, 0.1519, 0.088, 0.4364, 0.487, 0.6293, 0.2392, 0.445, 0.4837, 0.5315, 0.3938, 0.1065, 0.41, 0.3134, 0.3194, 0.0082, 0.382, 0.1148, -0.0185, -0.1643, 0.0518, 0.4042, 0.7972, -0.1175, -0.4678, -0.8453, -0.8069, -0.1752, -0.0194, -0.1531, -0.3874, -0.4619, -0.8048, -0.6306, -0.6405, 0.2425, -0.3105, 0.0071, 0.6711, 1.1795, 1.9269, 1.8044, 2.0861, 0.7379, -2.6516, -1.7677 } };
  float a1[25];
  float W2[10][25] = { { -0.1031, 0.3084, 0.3811, -0.0225, -0.1858, 0.1278, 0.0795, 0.0282, -0.2332, -1.1754, -0.0369, -1.3671, 0.3673, -0.1881, 0.1472, 0.1455, 0.0785, -0.0433, 0.1416, -0.372, -0.3188, 0.2789, 0.214, -0.1542, 1.5634 }, { 0.0443, -0.3823, -0.4901, -0.1283, 0.2888, 0.0406, -0.1446, 0.0255, 0.0692, 0.2413, 0.0781, -0.4461, -0.1725, 0.2786, -0.2767, 0.0683, 0.2654, -0.2179, 0.2649, 0.0232, -0.36, -0.21, 0.0528, -0.3715, -0.5914 }, { 0.1982, -0.0184, 0.5164, -0.2346, -0.082, 0.0056, -0.2026, -0.2509, -0.3108, -0.728, 0.3812, 0.8671, 0.2138, 0.1633, -0.1394, -0.8898, -0.1345, -0.4291, 0.1149, -0.0816, 0.5426, -0.3568, -0.0547, -2.1485, -0.1517 }, { -0.3702, -0.2469, 0.1363, 0.3833, -0.3955, 0.1444, -0.1032, 0.3464, -0.0115, -0.4733, 0.3507, -0.224, 0.1542, 0.1358, 0.0464, -0.2685, 0.0279, -0.4915, 0.1423, 0.3241, -0.3724, 0.2916, 0.1211, 0.0354, -0.3495 }, { -0.4453, -0.333, -0.1574, 0.3398, 0.1887, -0.2173, 0.0825, -0.1112, -0.3296, 1.9248, -0.3813, -0.4052, 0.2862, 0.0551, -0.2428, -0.3354, 0.1683, 0.6345, -0.2297, 0.2433, 0.0421, 0.201, -0.4071, -1.594, -0.9756 }, { 0.3324, -0.3916, -1.1234, 0.2569, 0.0064, 0.1419, 0.1342, 0.0897, -0.3948, 1.3064, 0.325, 0.3112, -0.2327, 0.0064, 0.0513, -0.5443, 0.2272, 0.1591, 0.0902, -0.3392, -0.1557, -0.3778, -0.3804, 0.5214, -0.8373 }, { -0.3731, -0.1139, -0.8278, 0.2134, 0.2422, 0.3411, 0.0141, 0.0406, -0.2871, -1.8052, -0.3215, 0.6934, 0.4407, -0.0267, -0.2732, 0.532, -0.1755, -0.6483, 0.18, 0.1885, -0.3026, -0.2248, 0.1679, 0.3409, -0.6114 }, { -0.2094, 0.3624, 0.0363, 0.0143, 0.0158, -0.2953, -0.2231, 0.3273, 0.351, 1.4159, -0.2999, 0.6529, 0.194, -0.1065, -0.1346, -0.8931, 0.0372, -2.1091, 0.2221, 0.3474, 0.3417, 0.3812, 0.0586, 0.0476, 0.737 }, { 0.0244, -0.2174, 0.1832, -0.0784, 0.0015, 0.398, -0.4087, -0.0158, -0.35, -0.4786, -0.202, 1.4981, 0.1442, 0.1404, 0.3427, -0.6367, 0.158, 0.6273, -0.3482, -0.2237, -0.7691, 0.3733, 0.2811, -0.0207, -0.0722 }, { 0.3994, 0.0027, -0.4982, -0.1072, -0.191, 0.3065, 0.2842, 0.2721, -0.1214, 2.0278, 0.2881, -0.8576, -0.4596, -0.2342, -0.0694, -0.1087, -0.3479, -0.5115, 0.3012, -0.2754, -0.8987, 0.0134, -0.091, 0.6855, 1.2987 } };
  float a2[10];
  float b1[25] = { -0.0901, 0.0, 0.3984, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.0278, 0.0, -0.002, -0.0235, -0.0757, -0.0436, 0.304, -0.0285, 0.8204, 0.0, 0.0, 0.1211, 0.0, -0.0587, 0.2522, -0.0529 };
  float b2[10] = { -0.1843, -0.1281, -0.4104, -0.1167, -0.2755, -0.2255, -0.2435, -0.3616, 0.0797, -0.1318 };
  float aux = 0.0;

  for (int i = 0; i < 25; i++) {
    aux = 0.0;
    for (int j = 0; j < 50; j++) { aux = aux + W1[i][j] * a0[j]; }
    a1[i] = relu(aux + b1[i]);
  }
  for (int i = 0; i < 10; i++) {
    aux = 0.0;
    for (int j = 0; j < 25; j++) { aux = aux + W2[i][j] * a1[j]; }
    a2[i] = sigmoid(aux + b2[i]);
  }
  for(uint8_t i = 0; i < 10; i++){
    nnDiagnosis[i] = a2[i];
  }
}

void fillPumpLast()
{
  float W1[15][50] = {{0.097,0.204,0.412,-0.062,-0.456,-0.222,0.209,0.255,-0.414,0.323,-0.205,0.131,-0.173,-0.375,-0.199,0.027,0.391,0.483,0.255,0.312,0.443,-0.063,-0.112,-0.08,0.428,-0.233,-0.198,-0.003,-0.222,0.045,0.022,0.185,-0.076,-0.278,-0.141,0.009,-0.05,0.215,-0.181,0.283,-0.367,0.176,0.255,-0.121,0.032,-0.126,-0.189,-0.141,-0.373,-0.232},{0.522,0.634,0.116,0.806,0.089,-0.024,0.126,-0.389,-0.217,-0.427,0.047,-0.259,-0.679,-0.07,-0.766,-0.605,-0.447,-0.363,-0.556,-0.416,-0.672,-0.654,-0.696,-0.655,-0.606,-0.544,-0.095,-0.396,-0.645,-0.029,-0.487,-0.411,-0.393,-0.461,-0.721,-0.481,-0.446,-0.566,-0.423,0.132,0.342,-0.487,-0.291,-0.412,-0.327,-0.588,-0.296,-0.545,-0.192,-0.191},{-0.356,-0.036,0.466,-0.358,0.087,-0.077,-0.067,0.208,0.03,0.042,-0.114,0.113,-0.248,-0.028,0.089,0.079,0.25,-0.081,0.192,0.339,0.226,0.413,-0.183,0.021,0.269,0.284,0.115,-0.342,-0.006,-0.071,-0.134,-0.117,-0.096,0.302,-0.377,-0.443,0.094,0.14,-0.191,-0.083,-0.16,-0.103,-0.57,-0.179,-0.229,0.298,-0.285,0.286,-0.185,0.065},{-0.457,-0.061,-0.258,-0.065,-0.096,-0.055,0.217,-0.17,-0.278,0.133,0.037,-0.418,-0.21,0.065,-0.16,0.451,0.238,0.15,0.493,0.087,0.324,0.096,-0.283,0.434,0.123,0.046,0.419,0.248,-0.002,-0.008,-0.075,-0.138,0.372,0.004,-0.144,-0.099,-0.038,0.181,-0.044,-0.069,-0.449,0.032,-0.04,-0.005,-0.299,0.294,-0.203,0.236,-0.127,0.278},{0.135,0.32,0.001,0.341,0.136,-0.047,-0.011,0.172,-0.042,0.036,0.092,-0.053,0.291,-0.4,-0.038,0.068,0.142,-0.214,-0.12,-0.068,0.119,0.263,-0.032,0.17,-0.079,-0.059,0.285,-0.013,0.2,0.424,0.061,0.231,-0.142,-0.085,-0.334,-0.033,-0.037,0.321,0.287,0.182,-0.071,0.428,0.045,0.054,-0.037,-0.187,0.011,-0.447,0.162,-0.15},{-0.031,0.367,0.597,-0.237,0.245,-0.037,-0.2,-0.556,-0.291,-0.511,-0.138,0.528,0.285,0.314,-0.072,0.14,-0.055,0.095,0.302,-0.209,0.226,0.159,-0.026,0.387,-0.121,-0.089,-0.035,-0.006,-0.643,-0.238,-0.486,-0.069,-0.534,-0.497,-0.948,-0.214,-0.031,-0.061,-0.227,0.051,-0.66,0.575,0.42,0.402,0.17,-0.03,0.122,-0.659,-0.381,-0.537},{0.027,0.058,-0.128,0.036,0.083,-0.055,-0.341,0.027,0.078,0.066,0.091,0.047,-0.071,0.015,-0.219,-0.147,-0.151,0.007,-0.164,0.016,0.117,-0.066,-0.161,-0.27,-0.337,-0.342,0.152,0.049,0.149,0.049,0.156,-0.454,0.133,0.058,-0.291,0.203,0.198,0.122,-0.065,0.122,0.02,0.063,0.179,0.155,0.245,0.14,0.297,0.233,0.233,0.106},{-0.244,0.3,-0.37,0.084,-0.021,-0.229,0.021,0.007,-0.519,-0.195,0.313,0.299,0.328,-0.118,0.253,-0.034,-0.007,-0.194,0.051,0.325,-0.057,-0.066,0.307,-0.011,0.219,-0.141,-0.317,0.142,-0.031,-0.081,0.155,-0.018,0.116,0.207,0.007,-0.012,-0.019,0.146,-0.152,0.203,-0.072,-0.02,0.143,-0.139,-0.121,-0.358,0.25,-0.039,-0.242,-0.42},{-0.128,-0.116,0.103,-0.143,-0.412,0.263,0.267,-0.043,0.313,0.026,-0.439,-0.027,-0.081,0.301,0.067,-0.002,0.068,0.212,0.007,0.06,0.326,0.361,-0.17,0.136,0.04,-0.288,-0.415,0.05,0.041,-0.057,-0.142,0.172,0.312,-0.128,-0.268,-0.251,0.17,0.143,-0.081,0.083,0.206,0.222,0.07,-0.128,-0.118,-0.163,-0.593,0.063,-0.145,0.155},{0.196,0.221,0.552,0.673,0.314,0.128,-0.232,0.049,-0.629,0.094,-0.673,-0.677,-0.586,-0.722,-0.392,-0.3,-0.17,-0.45,-0.282,-0.426,-0.67,-0.056,-0.679,-0.503,-0.396,-0.572,-0.555,-0.347,-0.736,-0.331,-0.409,-0.766,-0.333,-0.513,-0.188,-0.24,-0.485,-0.286,0.094,-0.057,0.825,-0.187,-0.099,-0.739,-0.502,-0.27,-0.211,-0.547,-0.205,0.001},{0.03,-0.052,0.152,-0.052,0.034,0.021,-0.219,-0.116,0.141,-0.282,0.155,0.089,-0.235,0.135,0.367,-0.05,-0.235,-0.022,-0.22,-0.422,0.067,0.148,-0.143,0.258,0.022,-0.061,0.287,-0.05,-0.114,-0.307,-0.413,-0.243,0.044,0.194,0.382,0.24,0.015,0.105,-0.112,-0.025,-0.127,0.073,0.219,0.112,0.071,0.13,0.192,0.182,0.345,-0.218},{0.143,-0.476,-0.059,-0.202,0.041,-0.071,0.253,0.041,0.073,-0.06,0.134,-0.264,-0.098,-0.163,0.055,-0.199,0.172,-0.045,-0.082,0.398,-0.029,0.064,-0.297,-0.161,-0.21,-0.128,0.326,0.017,0.035,0.13,0.077,-0.333,-0.146,0.002,-0.125,0.084,0.298,-0.145,0.204,-0.04,0.066,0.232,-0.116,0.05,-0.121,0.473,-0.02,0.082,0.333,-0.004},{0.237,-0.039,-0.231,-0.147,-0.051,-0.466,0.184,-0.323,-0.241,0.021,-0.064,-0.13,-0.091,0.003,-0.4,0.227,0.329,0.064,0.116,0.14,-0.19,0.253,0.086,-0.228,-0.318,0.226,0.003,-0.376,0.182,0.128,-0.045,0.007,0.263,0.123,-0.133,-0.013,0.13,0.045,0.219,0.285,-0.285,-0.346,-0.003,-0.365,0.126,0.129,-0.213,0.206,0.229,0.257},{0.243,0.099,0.237,0.053,0.028,-0.274,-0.35,-0.087,-0.358,-0.111,0.02,-0.185,-0.11,0.185,-0.037,0.194,0.379,-0.416,0.16,0.212,0.216,-0.144,0.29,0.021,-0.104,0.23,0.022,0.389,-0.027,-0.007,0.058,0.098,-0.178,0.42,-0.123,0.108,0.113,-0.084,0.22,-0.071,0.306,0.151,-0.094,-0.094,-0.424,0.145,0.309,-0.603,-0.294,-0.068},{0.37,-0.2,-0.218,0.217,-0.273,0.117,0.497,-0.172,0.239,-0.383,0.446,-0.301,0.109,-0.277,0.341,0.378,0.016,-0.24,0.454,0.254,0.056,-0.203,-0.057,0.162,-0.066,-0.133,0.181,0.228,0.027,0.158,-0.166,-0.239,-0.488,0.131,-0.424,-0.219,0.029,0.217,0.006,-0.558,-0.268,-0.185,-0.418,0.195,0.735,0.195,-0.396,0.407,0.192,0.152}};
  float a1[15];
  float W2[1][15] = {{0.658,-0.49,0.355,0.354,-0.63,1.43,0.426,-0.358,0.223,-0.519,0.48,0.492,0.239,-0.472,0.535}};
  float a2[1]; 
  float b1[15]= {-0.149,0.324,0.003,-0.169,0.222,1.139,-0.117,0.123,-0.23,0.365,-0.019,-0.05,-0.048,0.211,0.114};
  float b2[1]= {-0.215};
  float aux = 0.0;

  /* ***** Neural network running ***** */
  for(int i = 0 ; i<15; i++ ) {aux=0.0;for(int j = 0 ; j <50 ; j++ ) { aux=aux+W1[i][j]*a0[j];} a1[i]=relu(aux+b1[i]);}
  for(int i = 0 ; i<1; i++ ) {aux=0.0;for(int j = 0 ; j <15 ; j++ ) { aux=aux+W2[i][j]*a1[j];} a2[i]=sigmoid(aux+b2[i]);}

  fill = a2[0];
}

/* ================================================== MAIN TASK ================================================== */
void mainTask() {
  leds("B",1);
  /* ============= E: adquisiton raw data ============= */
  float alphaL = 0.03;
  Average<float> rPos(nData);
  Average<uint16_t> rLoad(nData);

  uint16_t fLoad = scale.get_value(5) * 0.01;

  long ticTime = millis();
  oledDisplay(1, "Acquire data");

  for (uint16_t i = 0; i < nData; i++) {
    if(typeLidar == "0"){
      rPos.push(getDistance0() * 0.393701);  // ============== cm to inch factor =  0.393701
    }else{
      rPos.push(getDistance1() * 0.393701);  // ============== cm to inch factor =  0.393701
    }
    fLoad = (alphaL * scale.get_value(5) * 0.01) + ((1 - alphaL) * fLoad);
    rLoad.push(fLoad);
    delay(15);
  }
  long allTime = millis() - ticTime;
  oledDisplay(1, String(allTime));

  /* ----- */
  for (uint16_t i = 0; i < nData; i++) {
    Serial.print(rPos.get(i));
    Serial.print(",");
  }
  Serial.println("-");
  for (uint16_t i = 0; i < nData; i++) {
    Serial.print(rLoad.get(i));
    Serial.print(",");
  }
  Serial.println("-");
  /* ----- */

  leds("B",2);
  /*  F: Process data  */
  float maxPos = 0;
  float minPos = 0;
  float maxLoad = 0;
  float minLoad = 0;

  int max_pos_index = 0;
  int min_pos_index = 0;
  int max_load_index = 0;
  int min_load_index = 0;

  maxPos = rPos.maximum(&max_pos_index);
  minPos = rPos.minimum(&min_pos_index);
  maxLoad = rLoad.maximum(&max_load_index);
  minLoad = rLoad.minimum(&min_load_index);

  /* ====== MAIN PROCESS DATA ====== */
  if ((maxPos - minPos) <= 15) {
    status = 0;
    leds("R",1);
    Serial.println("stopped");
  } else {
    /* ************ Separe stroke ************ */
    status = 1;
    Serial.println("Separe stroke");

    float value;
    int16_t i_start = 0;
    int16_t i_flag = 0;
    int16_t i_end = 0;
    float tp = 10;
    float diff;
    float range = maxPos - minPos;

    for (int16_t i = 0; i < nData - 1; i++) {
      value = rPos.get(i);
      diff = value - minPos;
      if (diff < 0.2 * range && i_flag == 0 && i_end == 0) {
        if (diff < tp) {
          tp = diff;
          i_start = i;
        }
      } else if (i_start != 0 && diff > 0.8 * range && i_end == 0) {
        if (diff > tp) {
          tp = diff;
          i_flag = i;
        }
      } else if (i_flag != 0 && diff < 0.2 * range) {
        if (diff < tp) {
          tp = diff;
          i_end = i;
        }
      } else if (diff > 0.2 * range && i_end != 0) {
        break;
      }
    }
    Serial.println("Pos: " + String(minPos) + "," + String(maxPos) + "; min index " + String(i_start) + " min index: " + String(i_end));

    /* ************ detect stroke integrity ************ */
    if (i_start == 0 || i_end == 0) {
      Serial.println("Incomplete dynachart.");
      integrated = 0;
      leds("R",2);
    } else {
      /* ************ get SPM ************ */
      spm = 3e7 / (allTime * (i_end - i_start));
      Serial.println(spm);

      /* ************ get length ************ */
      sLength = range;

      /* ************ resize array to 50 ************ */
      float length = float(i_end - i_start) / 49.00;
      //uint16_t temp;
      uint16_t index;
      for (uint16_t i = 0; i < 49; i++) {
        index = i_start + i * length;
        //temp = mapfloat(load_raw.get(index),-40,30,6.5,0);
        uint16_t t_load = map(rLoad.get(index), 274, 440, 0, 188); // =============================================== OJO CALIBRACION SENSOR ==============================================<< 
        float t_pos = rPos.get(index) - minPos;
        load50.push(t_load);
        pos50.push(t_pos);
        //Serial.print(String(acc_raw.get(index)) + ",");
        a0[i] = t_load * 0.01;
      }

      uint16_t temp = map(rLoad.get(i_end),  274, 440, 0, 188);
      load50.push(temp);
      pos50.push(rPos.get(index) - minPos);

      /* ************ resize array to 20 ************ */
      length = float(i_end - i_start) / 19.00;
      for (uint16_t i = 0; i < 19; i++) {
        index = i_start + i * length;
        //temp = mapfloat(load_raw.get(index),-40,30,6.5,0);
        uint16_t t_load = map(rLoad.get(index), 274, 440, 0, 188);
        float t_pos = rPos.get(index) - minPos;
        load20.push(t_load);
        pos20.push(t_pos);
      }

      temp = map(rLoad.get(i_end), 274, 440, 0, 188);
      load20.push(temp);
      pos20.push(rPos.get(index) - minPos);

      /* ----- */
      for (uint16_t i = 0; i < 50; i++) {
        Serial.print(pos50.get(i));
        Serial.print(",");
      }
      Serial.println("-");
      for (uint16_t i = 0; i < 50; i++) {
        Serial.print(load50.get(i));
        Serial.print(",");
      }
      Serial.println("-");
    }
  }
}

/* ========================= MAIN SETUP ========================= */
void setup() {
  Serial.begin(115200);
  // diagnosis();
  // fillPump();

  pinMode(lR,OUTPUT);
  pinMode(lG,OUTPUT);
  pinMode(lB,OUTPUT);

  digitalWrite(lR,LOW);
  digitalWrite(lG,LOW);
  digitalWrite(lB,LOW);

  /*
  if(typeLidar == "0")
  {// =============== robot lidar sensor ===============
    Serial.println(" 1 *-* ----- ");
    lidarSerial.begin(115200, SERIAL_8N1, 19, 20);
  }else{
    Wire1.begin(SDA_ACC, SCL_ACC);
  }
  */

  lidarSerial.begin(115200, SERIAL_8N1, 19, 20);
  Wire1.begin(SDA_ACC, SCL_ACC);
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  pinMode(PIN_CONFIG, INPUT);
  pinMode(PIN_TX_MODE, INPUT);

  if (!mpu.begin(0x68, &Wire1)) {
    int cc = 0;
    Serial.println(" 2 *-* ----- ");
    Serial.println("Failed to find MPU6050 chip...");
    while (1) {
      Serial.println(" 3 *-* ----- ");
      delay(1000);
      if(cc >= 2){
        break;
      }
      cc++;
    }
    
  }

  Serial.println("MPU6050 Found!");
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.println(digitalRead(PIN_CONFIG));

  if(!digitalRead(PIN_CONFIG)){
    Serial.println("===== Setting mode >>>");
    digitalWrite(lB,HIGH);
    testTask();
  }else{
    Serial.println("===== Running mode >>>");
    mainTask();
  }

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  deviceState = DEVICE_STATE_INIT;

  String modeTx = readFile(SPIFFS, "/mode.txt");

  Serial.println(" ----- *-* ----- ");
  Serial.println(modeTx);

  if(modeTx == "WiFi"){
    Serial.println("TX wifi mode...");
    leds("B",2);
    wifiTx();
    delay(1000);
    esp_sleep_enable_timer_wakeup(appTxDutyCycle * 1000);
    esp_deep_sleep_start();
    /* HERE FINISH SCRIPT WHEN THE WIFI MODE IS ACTIVATED */
  }

  digitalWrite(lG,HIGH);
  delay(1000);
  digitalWrite(lG,LOW);
  /*
  if(digitalRead(PIN_TX_MODE) == 0){
    Serial.println("TX wifi mode...");
    wifiTx();
    delay(1000);
    esp_sleep_enable_timer_wakeup(appTxDutyCycle * 1000);
    esp_deep_sleep_start();
  }
  */
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
