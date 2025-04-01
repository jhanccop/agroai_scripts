/*
Runing
- A: wakeup motion detected and timmer.
- B: motion?
- x C: select wifi or lora mode
- D: read configuration
- x E: adquisiton raw data
- x F: proccess data
- x G: running NN
- x H: make payload
- x I: send data
- J: deep sleep start

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

String runssid = "ConectaLineal2500";
String runpassword = "19CERA@DER27@14";
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

String typeLidar = "0"; // 0: DFROBOT  1: LUNA

/* =============== CONFIG OLED =============== */

static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
//static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);

/* =============== CONFIG LORA =============== */
#include "LoRaWan_APP.h"

#define LORA_BANDWIDTH  0
#define LORA_SPREADING_FACTOR 12
//#define LORAWAN_DEVEUI_AUTO true

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
//float vBat = 4.55;
uint8_t status = -1; /* 1: running, 0: stopped */
uint8_t integrated = 1;

const uint64_t uS_TO_S_FACTOR = 1000000ULL;

Average<float> load20(20);
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
    DIST = getDistance0();
  }else{
    DIST = getDistance1();
  }


  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float xacc = a.acceleration.x;
  float load = scale.get_value(5) * 0.01;

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

/* =========================== GET SETTINGS ============================= */
void getHttp() {
  int err = 0;

  HTTPClient http;
  uint64_t chipId = ESP.getEfuseMac();
  String mac = String(chipId,HEX);
  SearchPath = SearchPath + mac;

  String SERVER = "192.168.3.31";
  String Port = "8000";

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
  const char* resolution = docIn["Resolution"];
  int sleepTime = docIn["SamplingRate"];

  appTxDutyCycle = sleepTime * 1000;

  Serial.println(deviceName);
  Serial.println(deviceMacAddress);
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
  return raw*0.01;
}

/* ========================= INIT RUN ========================= */
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

/* ========================= PREPARE TX FRAME ========================= */
static void prepareTxFrame(uint8_t port) {

  oledDisplay(1, "LORAWAN mode");
  appDataSize = 0;
  /* status */
  int16_t spmInt = (int16_t)(spm * 10);
  appData[appDataSize++] = (spmInt >> 8) & 0xFF;
  appData[appDataSize++] = spmInt & 0xFF;

  int16_t fillPumpInt = (int16_t)(fill * 100);
  appData[appDataSize++] = (fillPumpInt >> 8) & 0xFF;
  appData[appDataSize++] = fillPumpInt & 0xFF;

  int16_t sLengthInt = (int16_t)(sLength * 100);
  appData[appDataSize++] = (sLengthInt >> 8) & 0xFF;
  appData[appDataSize++] = sLengthInt & 0xFF;

  int16_t vBatInt = (int16_t)(vBat() * 100);
  appData[appDataSize++] = (vBatInt >> 8) & 0xFF;
  appData[appDataSize++] = vBatInt & 0xFF;

  appData[appDataSize++] = status;

  if(status == 1 && integrated == 1){
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

void wifiTx(){
  oledDisplay(1, "WIFI mode");
  JsonDocument docOut;
  String sLoad = "";
  String sPos = "";
  for(uint8_t i = 0; i < 49; i++){
    sLoad += String(load50.get(i)) + ",";
    sPos += String(pos50.get(i),2) + ",";
  }
  sLoad += String(load50.get(49));
  sPos += String(pos50.get(49),2);

  uint64_t chipId = ESP.getEfuseMac();

  docOut["type"] = "wellAnalizer";
  docOut["devEUI"] = String(chipId, HEX);
  docOut["vBat"] = String(vBat(),2);
  docOut["fillPump"] = String(fill,2);
  docOut["sLength"] = String(sLength,2);
  docOut["status"] = String(status);

  if(status == 1 && integrated == 1){
    docOut["load"] = sLoad;
    docOut["pos"] = sPos;
  }

  String payload = "";
  serializeJson(docOut, payload);

  int count = 0;
  WiFi.begin(runssid.c_str(), runpassword.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    if (count == 5){
      ESP.restart();
    }
    delay(5000);
    count++;
  }
  client.setServer(broker, mqtt_port);
  getHttp();

  count = 0;

  while (!client.connected()) {
    String client_id = "esp32-client-";
    
    client_id += String(chipId,HEX);
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      Serial.println("Public EMQX MQTT broker connected");
      //String topiSub = String(topicSubscribe) + "/" + String(WiFi.macAddress());
      //client.subscribe(topiSub.c_str());
      //Serial.println(topiSub);
      
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

void fillPump()
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

/* ========================= MAIN TASK ========================= */
void mainTask() {
  /*  E: adquisiton raw data  */
  float alphaP = 0.02;
  float alphaL = 0.02;
  Average<float> rPos(nData);
  Average<uint16_t> rLoad(nData);

  uint16_t fLoad = scale.get_value(5) * 0.01;

  long ticTime = millis();
  oledDisplay(1, "Acquire data");

  for (uint16_t i = 0; i < nData; i++) {
    fLoad = (alphaL * scale.get_value(5) * 0.01) + ((1 - alphaL) * fLoad);
    if(typeLidar == "0"){
      rPos.push(getDistance0() * 0.01);
    }else{
      rPos.push(getDistance1() * 0.01);
    }
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
  if ((maxPos - minPos) <= 0.2) {
    status = 0;
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
        uint16_t t_load = map(rLoad.get(index), 1000, 2000, 0, 650);
        float t_pos = rPos.get(index) - minPos;
        load50.push(t_load);
        pos50.push(t_pos);
        //Serial.print(String(acc_raw.get(index)) + ",");
        a0[i] = t_load * 0.01;
      }

      uint16_t temp = map(rLoad.get(i_end), 1000, 2000, 0, 650);
      load50.push(temp);
      pos50.push(rPos.get(i_end) - minPos);

      /* ************ resize array to 20 ************ */
      length = float(i_end - i_start) / 19.00;
      for (uint16_t i = 0; i < 19; i++) {
        index = i_start + i * length;
        //temp = mapfloat(load_raw.get(index),-40,30,6.5,0);
        uint16_t t_load = map(rLoad.get(index), 1000, 2000, 0, 650);
        float t_pos = rPos.get(index) - minPos;
        load20.push(t_load);
        pos20.push(t_pos);
      }

      temp = map(rLoad.get(i_end), 1000, 2000, 0, 650);
      load20.push(temp);
      pos20.push(rPos.get(i_end) - minPos);

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
      /* ----- */

      // a0[49] = temp;

      /* ************ NN for diagnosis ************ */
      //diagnosis();
      /*
    for (int i = 0; i < 10; i++)
    {
      Serial.print(String(v_diagnosis[i]) + ",");
    }
    */

      //Serial.println();

      /* ************ NN for fillpump ************ */
      fillPump();
      Serial.println(String(fill));
    }
  }
}

/* ========================= MAIN SETUP ========================= */
void setup() {
  Serial.begin(115200);

  pinMode(lR,OUTPUT);
  pinMode(lG,OUTPUT);
  pinMode(lB,OUTPUT);

  digitalWrite(lR,LOW);
  digitalWrite(lG,LOW);
  digitalWrite(lB,LOW);

  if(typeLidar == "0"){
    lidarSerial.begin(115200, SERIAL_8N1, 19, 20);
  }
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  pinMode(PIN_CONFIG,INPUT);
  pinMode(PIN_TX_MODE,INPUT);

  Wire1.begin(SDA_ACC, SCL_ACC);

  if (!mpu.begin(0x68, &Wire1)) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(1000);
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

  Serial.println("---*-*-----");
  Serial.println(modeTx);

  if(modeTx == "WiFi"){
    Serial.println("TX wifi mode...");
    digitalWrite(lB,HIGH);
    delay(400);
    digitalWrite(lB,LOW);
    wifiTx();
    delay(1000);
    esp_sleep_enable_timer_wakeup(appTxDutyCycle * 1000);
    esp_deep_sleep_start();
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
