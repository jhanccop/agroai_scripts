// {"type":"tank","mac":"48:E7:29:97:23:A0","count":3,"value":"3.862","temp":"84.987","sampleRate":60,"tankHeight":"5.000"}

// {"timeSleep":3575}

/*weather station SIM7000 PRODUCTION 15-02-2025*/
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include <Average.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

/* ============================== DHT CONFIGURATIONS ============================== */
#include <DHT.h>;
#define DHTPIN1 35
#define DHTTYPE DHT21 //DHT 21 (AM2301)
DHT dht1(DHTPIN1, DHTTYPE); 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = -18000;

AsyncWebServer server(80);

/* ============================== SIM7000G CONFIGURATIONS ============================== */

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 4096 // Set RX buffer to 1Kb

#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""

const char apn[]      = "claro.pe";
const char gprsUser[] = "";
const char gprsPass[] = "";

#include <TinyGsmClient.h>
#include <SD.h>
#include "FS.h"

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGsmClient espClient(modem); // ACTIVAR PARA SIM

// LilyGO T-SIM7000G Pinout
#define UART_BAUD           115200
#define PIN_DTR             25
#define PIN_TX              27
#define PIN_RX              26
#define PWR_PIN             4

#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13
#define LED_PIN             12



const char* ssid = "DATAGREEN-WEATHER-STATION";     // original ssid
const char* password = "12345678";  // original password

String runssid = "";
String runpassword = "";

int port = 8000;

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
const char* PARAM_DEEP_SLEEP = "deepSleepTime";
const char* PARAM_TANK_HEIGHT = "tankHeight";
const char* PARAM_RESET = "reset";

/* ====================== DEVICE SETTINGS ======================== */
#define pinBattery 39           // pin Battery
#define pinWindDirection 35     // pin Wind direction
#define pinWindVelocity 26      // pin Wind direction
#define pinConfigAP 23          //enter configuration Acces point mode
#define RAINCOUNT GPIO_NUM_14   // wake up by rain counter pin 14 2^7 0x0080 

const int pinConfig = 23;

String idTest;

unsigned long waitListen = 48000;
unsigned long previousMillis = 0;

uint64_t TIME_TO_SLEEP = 1775; // 1775  5 minutes sleep
RTC_DATA_ATTR unsigned long bootCount = 0; // data counter
const uint64_t uS_TO_S_FACTOR = 1000000ULL;

String sHostname = "";

double valueConfig;
double tankHeight;

/* ====================== HTML PAGE ======================== */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
  <title>US SENSOR FORM</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      alert("Saved value to US SESNOR IN SPIFFS");
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
  </script>
  </head>
  <body>
  <div class="content">
  <h2>Enter parameters</h2>
  
  <p>MAC address: <b>%MAC%</b></p>

  <table>
  <tr>
  <td>
  <button type = "button" class="refreshButton" onclick="location.reload()">REFRESH</button>
  </td>

  <td>  &emsp; &emsp;</td>

  <td>
  <h4 style="color:DodgerBlue;">%value% F </h4> 
  </td>

  <td> / </td>
  
  <td>
  <h3 style="color:MediumSeaGreen;"> %dato% m </h3> 
  </td>
  
  </tr>
  </table>

  <p>Complete this form with wifi parameters:</p>
  <table>
  <form action="/get" target="hidden-form">
  <tr>
  <td>SSID: </td>
  <td><input type="text" name="ssidString" value = %ssidString%></td>
  <td><input type="submit" value="Save" onclick="submitMessage()"></td>
  </tr>
  </form>

  <form action="/get" target="hidden-form">
  <tr>
  <td>Pass: </td>
  <td><input type="text" name="passString" value = %passString%></td>
  <td><input type="submit" value="Save" onclick="submitMessage()"></td>
  </tr>
  </form>

  <form action="/get" target="hidden-form">
  <tr>
  <td>Rate sample (s): </td>
  <td><input type="number " name="deepSleepTime" value = %deepSleepTime%></td>
  <td><input type="submit" value="Save" onclick="submitMessage()"></td>
  </tr>
  </form>

  <form action="/get" target="hidden-form">
  <tr>
  <td>Tank Heiht (m): </td>
  <td><input type="number " name="tankHeight" value = %tankHeight%></td>
  <td><input type="submit" value="Save" onclick="submitMessage()"></td>
  </tr>
  </form>
  
  </table>

  <br>
  <br>
  
  <form action="/get" target="hidden-form">
  <input type="hidden" name="reset">
  <input type="submit" value="SAVE AND EXIT" onclick="submitMessage()">
  </form>

  <iframe style="display:none" name="hidden-form"></iframe>
  </div>
</body></html>)rawliteral";

/* ====================== HTML SCRIPTS ======================== */
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found --9844 ");
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

String processor(const String& var) {
  if (var == "ssidString") {
    return readFile(SPIFFS, "/ssidString.txt");
  } else if (var == "passString") {
    return readFile(SPIFFS, "/passString.txt");
  } else if (var == "deepSleepTime") {
    return readFile(SPIFFS, "/deepSleepTime.txt");
  } else if (var == "tankHeight") {
    return readFile(SPIFFS, "/tankHeight.txt");
  } else if (var == "MAC") {
    return WiFi.macAddress();
  } else if (var == "value") {
    double hum1 = dht1.readHumidity();
    double temp1= dht1.readTemperature();
    return String(hum1,3) + " / " + String(temp1,3);
  } else if (var == "dato") {
    //double hum2 = dht2.readHumidity();
    //double temp2= dht2.readTemperature();
    //return String(hum2,3) + " / " + String(temp2,3);
    return "0";
  }
  return String();
}


/* ====================== SETTING ======================== */
void setting() {
  
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  //Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {

    String inputMessage;
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    if (request->hasParam(PARAM_SSID)) {
      inputMessage = request->getParam(PARAM_SSID)->value();
      writeFile(SPIFFS, "/ssidString.txt", inputMessage.c_str());
    }
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    else if (request->hasParam(PARAM_PASS)) {
      inputMessage = request->getParam(PARAM_PASS)->value();
      writeFile(SPIFFS, "/passString.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
    else if (request->hasParam(PARAM_DEEP_SLEEP)) {
      inputMessage = request->getParam(PARAM_DEEP_SLEEP)->value();
      writeFile(SPIFFS, "/deepSleepTime.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
    else if (request->hasParam(PARAM_TANK_HEIGHT)) {
      inputMessage = request->getParam(PARAM_TANK_HEIGHT)->value();
      writeFile(SPIFFS, "/tankHeight.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
    else if (request->hasParam(PARAM_RESET)) {
      ESP.restart();
    }
    else {
      inputMessage = "No message sent";
    }
    //Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });

  server.onNotFound(notFound);
  server.begin();

  //rtc_gpio_hold_dis(GPIO_NUM_15);

  while (1) {

    delay(25);
  }

}

/* ====================== SIM FUNCTIONS ======================== */
void modemPowerOn(){
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);
  digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff(){
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1500);
  digitalWrite(PWR_PIN, HIGH);
}

void modemRestart(){
  modemPowerOff();
  delay(1000);
  modemPowerOn();
}

/* ====================== RECONNECT ======================== */
void reconnect()
{
  int count = 0;
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      String topiSub = String(topicSubscribe) + "/" + String(WiFi.macAddress());
      client.subscribe(topiSub.c_str());
      Serial.println(topiSub);
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
}

/* ============================== SETUP GRPS ============================== */
bool setup_grps()
{
  Serial.println("gprs setup");

  modemPowerOn();

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);

  if (!modem.init()) {
    modemRestart();
    delay(2000);
    Serial.println("Failed to restart modem, attempting to continue without restarting");
    return false;
  }

  String modemInfo = modem.getModemInfo();
  Serial.println(modemInfo);

  if (modemInfo == "")
  {
    Serial.println("modem fail");
    return false;
  }
  /* */
  if (!modem.waitForNetwork(240000L))
  {
    Serial.println("nt fail");
    return false;
  }
  
  if (modem.isNetworkConnected())
  {
    Serial.println("connected");
  }
  else
  {
    return false;
  }

  if (!modem.gprsConnect(apn))
  //if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    Serial.println("grps fail");
    return false;
  }
  else
  {
    Serial.println("grps ok"); // sim  grps ok
    return true;
  }
}

/* ====================== RUNNING ======================== */
void normal_running() {

  rtc_gpio_hold_dis(GPIO_NUM_12);
  ++bootCount;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //TIME_TO_SLEEP = readFile(SPIFFS, "/deepSleepTime.txt").toInt();
  
  double hum1 = dht1.readHumidity();
  double temp1= dht1.readTemperature();
  
  StaticJsonDocument<200> docOut;
  docOut["type"] = "env";
  docOut["mac"] = WiFi.macAddress();
  docOut["count"] = bootCount;
  docOut["temp1"] = String(temp1,3);
  docOut["hum1"] = String(hum1,3);
  //docOut["temp2"] = String(temp2,3);
  //docOut["hum2"] = String(hum2,3);
  docOut["sampleRate"] = TIME_TO_SLEEP;

  //runssid = readFile(SPIFFS, "/ssidString.txt");     // original ssid
  //runpassword = readFile(SPIFFS, "/passString.txt");  // original password
  
  int count = 0;
  setup_grps(); 
  
  client.setServer(broker, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      Serial.println("Public EMQX MQTT broker connected");
      String topiSub = String(topicSubscribe) + "/" + String(WiFi.macAddress());
      client.subscribe(topiSub.c_str());
      Serial.println(topiSub);
      
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

  String payload = "";
  serializeJson(docOut, payload);
  
  Serial.println(payload);
  client.publish(topicPublish, payload.c_str());
  
  String Payload = String(bootCount) + "," +String(temp1,3) +"," + String(hum1,3)+ "\n";
  appendFile(SD, "/log.txt", Payload.c_str());

  //WiFi.disconnect(true);
  //WiFi.mode(WIFI_OFF);

  //modem.poweroff();

  //rtc_gpio_hold_en(GPIO_NUM_12);
  //Serial.println("start deep sleep");
  //delay(500);
  //esp_deep_sleep_start();
}

/* ====================== MAIN SDCARD ======================== */
void setup_SD(){
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)){
    Serial.println("SDCard MOUNT FAIL");
  }else{
    uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    String str = "SDCard Size: " + String(cardSize) + "MB";
    Serial.println(str);
  }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
/* ====================== MAIN SETUP ======================== */
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  setup_SD();

  dht1.begin(); // sensor1

  runssid = readFile(SPIFFS, "/ssidString.txt");
  runpassword = readFile(SPIFFS, "/passString.txt");
  tankHeight = readFile(SPIFFS, "/tankHeight.txt").toFloat();
  //TIME_TO_SLEEP = readFile(SPIFFS, "/deepSleepTime.txt").toInt();

  pinMode(pinConfig, INPUT_PULLUP);
  if (!digitalRead(pinConfig)) {
    Serial.println("====> Config mode");
    setting();
  } else {
    Serial.println("====> running mode");
    normal_running();
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= waitListen) {
    Serial.println("start deep sleep -- MAIN");
    delay(500);
    previousMillis = currentMillis;
    esp_deep_sleep_start();

  }
}
