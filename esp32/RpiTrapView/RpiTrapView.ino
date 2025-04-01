/*
RPI + ESP32 + dht
GET AND POST ESP 32

MQTT CHUNK DATA DOR RPI
*/
// {"timeSleep":3575}

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include <driver/rtc_io.h>
#include "SPIFFS.h"

#include <esp_sleep.h>

/* ====================== AP HTTP SERVER ======================== */
#include <DHT.h>;
#define DHTPIN 13
#define LED 2
#define DHTTYPE DHT21 //DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

/* ====================== AP HTTP SERVER ======================== */
AsyncWebServer server(80);

const char* ssid = "uS-sensor";     // original ssid
const char* password = "12345678";  // original password

String runssid = "";
String runpassword = "";
String SERVER = "http://192.168.3.31";
int statusW = WL_IDLE_STATUS;
String Port = "8000";

//const char* sHostname_buffer = "http://192.168.3.31";
//String sHostname = "";
String csrfToken = "";

String SearchPath = "/devices/api/buscar/?mac=";
String csrfPath = "/get-csrf/";
String postPath = "/data/api/post";

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
const char* PARAM_SERVER= "server";
const char* PARAM_PORT = "port";
const char* PARAM_RESET = "reset";

/* ====================== DEVICE SETTINGS ======================== */
#define RUN GPIO_NUM_25         // RPI control PIN 25
#define pinBattery 39           // pin Battery
#define pinPanel 34             // pin Panel
#define pinConfigAP 23          //enter configuration Acces point mode
#define ESP32MAN GPIO_NUM_33    // wake up esp32manager pin

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
RTC_DATA_ATTR time_t sleep_enter_time;

unsigned long previousTime = 0;
boolean completed = false;
boolean statusCam = false;
boolean saveImage = false;
boolean runningNN = false;
int refresh = 10;
int pulsos = 0;

/* device settings */
String DeviceName = "";
String DeviceMacAddress = "";
boolean A_TH = false;
String Resolution = "0";
boolean flag = true;

/* ====================== HTML PAGE ======================== */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
    <title>US SENSOR FORM</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script>
      function submitMessage() {
        alert("Saved value to SENSOR IN SPIFFS");
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
      <h4 style="color:DodgerBlue;"> %value% </h4> 
      </td>
      <td> / </td>
      <td>
      <h3 style="color:MediumSeaGreen;"> %dato% V </h3> 
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
    <td>Server: </td>
    <td><input type="text" name="server" value = %server%></td>
    <td><input type="submit" value="Save" onclick="submitMessage()"></td>
    </tr>
    </form>

    <form action="/get" target="hidden-form">
    <tr>
    <td>Port: </td>
    <td><input type="text" name="port" value = %port%></td>
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

/* ========================= FUNCTIONS ========================= */
void deepSleepSystem(){

  digitalWrite(LED, LOW);

  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 

  // deep sleep start 
  Serial.print("sleep for min: ");
  //TIME_TO_SLEEP = 1;
  Serial.println(TIME_TO_SLEEP);
  uint64_t SLEEPTIME = TIME_TO_SLEEP;
  esp_sleep_enable_timer_wakeup(SLEEPTIME * 60 * uS_TO_S_FACTOR);
  //Serial.flush(); 
  esp_deep_sleep_start();
}

String vBat(){
  long raw = map(analogRead(pinBattery),698,2416,199,602);
  return String(raw*0.01,2);
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
  } else if (var == "server") {
    return readFile(SPIFFS, "/server.txt");
  } else if (var == "port") {
    return readFile(SPIFFS, "/port.txt");
  } else if (var == "MAC") {
    return WiFi.macAddress();
  } else if (var == "value") {
    double hum = dht.readHumidity();
    double temp= dht.readTemperature();
    return String(hum,3) + " per | " + String(temp,3) + " C";
  } else if (var == "dato") {
    String vol = vBat();
    return vol;
  }
  return String();
}

void saveSetting(String value, String fil){
  String orValue = readFile(SPIFFS, fil.c_str());
  if(orValue != value ){
    writeFile(SPIFFS, fil.c_str(), value.c_str());
  }
}

/* =========================== GET SETTINGS =============================*/
void getHttp() {
  int err = 0;

  HTTPClient http;
  String mac = WiFi.macAddress();
  SearchPath = SearchPath + mac;

  String SERVER = readFile(SPIFFS, "/server.txt");
  String Port = readFile(SPIFFS, "/port.txt");

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
  boolean a_TH = docIn["A_TH"];
  boolean RunningNN = docIn["runningNN"];
  boolean Status = docIn["status"];
  int sleepTime = docIn["SleepTime"];

  DeviceName = String(deviceName);
  DeviceMacAddress = String(deviceMacAddress);
  Resolution = String(resolution);
  A_TH = a_TH;
  runningNN = RunningNN;
  statusCam = Status;
  TIME_TO_SLEEP = sleepTime;

  Serial.println(DeviceName);
  Serial.println(DeviceMacAddress);
  Serial.println(Resolution);
  Serial.println(A_TH);
  Serial.println(runningNN);
  Serial.println(statusCam);
  Serial.println(TIME_TO_SLEEP);

  saveSetting(String(Resolution), "/resolution.txt");
  saveSetting(String(runningNN), "/runningNN.txt");
}

void getcsrfHttp() {
  int err = 0;

  HTTPClient http;

  String SERVER = readFile(SPIFFS, "/server.txt");
  String Port = readFile(SPIFFS, "/port.txt");

  String msg_in = "";
  String urlComplet = "http://" + String(SERVER) + ":" + String(Port) + csrfPath;
  
  if (WiFi.status() == WL_CONNECTED) {
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
  const char* CsrfToken = docIn["csrfToken"];
  csrfToken = String(CsrfToken);
 
}

/* =========================== POST DATA =============================*/
void sendDataByPost() {

  JsonDocument docOut;

  if(A_TH){
    dht.begin();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.println("************");
    Serial.println(h);
    Serial.println(t);
    
    int cont = 0;
    while (isnan(t) || isnan(h) || (h == 1.00 && t == 1.00)) 
    {
      dht.begin();
      Serial.println("Failed to read from DHT");
      h = dht.readHumidity();
      t = dht.readTemperature();
      
      if(cont >= 10){
        break;
      }
      cont++;
      delay(2000);
    }

    docOut["Humidity"] = String(h,2);
    docOut["Temperature"] = String(t,2);
  }

  docOut["DeviceMacAddress"] = WiFi.macAddress();
  docOut["VoltageBattery"] = vBat();
  docOut["img64"] = "";

  String SERVER = readFile(SPIFFS, "/server.txt");
  String Port = readFile(SPIFFS, "/port.txt");

  String jsonString;
  serializeJson(docOut, jsonString);

  Serial.println(jsonString);

  String request = "POST /data/api/post HTTP/1.1\r\n";
  request += "Host: http://" + String(SERVER) + "\r\n";
  //request += "Content-Type: application/json\r\n";
  request += "X-CSRFToken: " + csrfToken +  "\r\n";
  request += "Cookie: csrftoken=" + csrfToken + "\r\n";
  //request += "Content-Length: " + String(jsonString.length()) + "\r\n";
  //request += "Connection: keep-alive\r\n";
  request += "\r\n";
  request += jsonString;

  HTTPClient http;

  String urlComplet = "http://" + String(SERVER) + ":" + String(Port) + postPath;
  http.begin(urlComplet);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-CSRFToken",csrfToken);
  http.addHeader("Cookie: csrftoken=",csrfToken);
  http.addHeader("Content-Length",String(jsonString.length()));
  http.addHeader("Connection","keep-alive");

  int httpCode = http.POST(jsonString);
    
  if (httpCode > 0) {
    String respuesta = http.getString();
    Serial.println("Código HTTP: " + String(httpCode));
    Serial.println("Respuesta: " + respuesta);
  } else {
    Serial.println("Error en petición POST");
  }
  
  http.end();

  if(httpCode < 0 ){
    Serial.println("********************* fail http RESTART**************");
    ESP.restart();
  }
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
    else if (request->hasParam(PARAM_SERVER)) {
      inputMessage = request->getParam(PARAM_SERVER)->value();
      writeFile(SPIFFS, "/server.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
    else if (request->hasParam(PARAM_PORT)) {
      inputMessage = request->getParam(PARAM_PORT)->value();
      writeFile(SPIFFS, "/port.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
    else if (request->hasParam(PARAM_RESET)) {
      ESP.restart();
    }
    else {
      inputMessage = "No message sent";
    }

    request->send(200, "text/text", inputMessage);
  });

  server.onNotFound(notFound);
  server.begin();

  //rtc_gpio_hold_dis(GPIO_NUM_15);

  while (1) {
    delay(25);
  }

}

/* ====================== RUNNING ======================== */
void init_running() {
  // SETTING MODE 
  pinMode(pinConfigAP, INPUT_PULLUP);
  if (!digitalRead(pinConfigAP)) {
    Serial.println("====> Config mode");
    setting();
  } else {
    Serial.println("====> running mode");
    //Connect and send initial data to get the configuration
    runssid = readFile(SPIFFS, "/ssidString.txt");     // original ssid
    runpassword = readFile(SPIFFS, "/passString.txt");  // original password

    int n = 0;
    WiFi.begin(runssid, runpassword);
    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.println(runssid);
      Serial.println(runpassword);
      Serial.println("Connecting to WiFi..");
      if(n >= 5){
        ESP.restart();
      }
      n++;
    }
    
    getHttp();

    if(statusCam){
      rtc_gpio_init(RUN);
      rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RUN);
      rtc_gpio_set_level(RUN, HIGH);
      rtc_gpio_hold_en(RUN);
    }else{
      getcsrfHttp();
      sendDataByPost();

      delay(15000);

      rtc_gpio_init(RUN);
      rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RUN);
      rtc_gpio_set_level(RUN, LOW);
      rtc_gpio_hold_en(RUN);
    }

  delay(1000);
  //rpiRequest();
  Serial.println("start deep sleep RUNING 2");
  
  deepSleepSystem();

  }
}

void infoWifi(){
  String ssid = readFile(SPIFFS, "/ssidString.txt");
  String pw = readFile(SPIFFS, "/passString.txt");

  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["pw"] = pw;
  char output[256];
  serializeJson(doc, output);

  // SEND DATA BY SERIAL TO RASPBERRY
  Serial2.write(output);
  Serial2.write("\n");
  Serial.println(output);
}

/* ========================= SEND INFO ESP32 TO RPI ========================= */
void stationSetup(){
  // READ CURRENTLY SETTING
  
  saveImage = readFile(SPIFFS, "/saveImage.txt").toInt();
  runningNN = readFile(SPIFFS, "/runningNN.txt").toInt();
  Resolution = readFile(SPIFFS, "/resolution.txt");
  SERVER = readFile(SPIFFS, "/server.txt");
  Port = readFile(SPIFFS, "/port.txt");

  JsonDocument doc;

  doc["saveImage"] = bool(saveImage);
  doc["runningNN"] = bool(runningNN);
  doc["resolution"] = String(Resolution);
  doc["server"] = String(SERVER);
  doc["port"] = String(Port);

  //serializeJson(doc, Serial);
  char output[600];
  serializeJson(doc, output);

  // SEND DATA BY SERIAL TO RASPBERRY
  Serial2.write(output);
  Serial2.write("\n");
  Serial.println("settings to rpi");
  Serial.println(output);
}

void dataEsp(){
  // START DHT SENSOR
  dht.begin(); // sensor

  JsonDocument docOut;
  docOut["mac"] = WiFi.macAddress();  
  docOut["T"] = String(dht.readTemperature(),2);
  docOut["H"] = String(dht.readHumidity(),2);
  docOut["B"] = vBat();                            // battery Voltage 5.08 v - 5.82 v

  String payload = "";
  serializeJson(docOut, payload);

  // SEND DATA BY SERIAL TO RASPBERRY
  Serial2.write(payload.c_str());
  Serial2.write("\n");
  Serial.println("data from esp 32");
  Serial.println(payload);

  time_t now;
  time(&now);

  int sleep_time = difftime(now, sleep_enter_time);
  Serial.printf("Tiempo en deep sleep: %d segundos\n", sleep_time);
  
  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 

  /*--- deep sleep start ***/ 
  Serial.println("sleep start DATA");
  Serial.println(sleep_enter_time);
  Serial.println(now);
  uint64_t SLEEPTIME = TIME_TO_SLEEP * 60 - sleep_time;
  Serial.println(SLEEPTIME);
  esp_sleep_enable_timer_wakeup(SLEEPTIME * uS_TO_S_FACTOR);

  esp_deep_sleep_start();


}

void rpiRequest(){
  while (true){
    previousTime = millis();
    if (Serial2.available())
    {
      uint8_t length = 0;
      String msgin = "";
      while (Serial2.available() && length < 255) {
        char ch = Serial2.read();
        msgin += (char)ch;
      }

      Serial.println("msg from rpi");

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, msgin);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        break;
        return;
      }

      const char* msg = doc["msg"];
      if(String(msg) == "settings"){
        stationSetup();
        deepSleepSystem();
      }else if(String(msg) == "completed"){
        deepSleepSystem();
      }else if(String(msg) == "data"){
        dataEsp();
        //deepSleepSystem();
      }else if(String(msg) == "wifi"){
        infoWifi();
        deepSleepSystem();
      }
      else if(String(msg) == "sleep"){
        rtc_gpio_init(RUN);
        rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_hold_dis(RUN);
        rtc_gpio_set_level(RUN, LOW);
        rtc_gpio_hold_en(RUN);

        delay(15000);

        deepSleepSystem();
      }
    }

    // Deep sleep start before 10 seconds
    if((millis() - previousTime) > 10000){
      Serial.println(millis() - previousTime);
      Serial.println("sleep for overtime");
      deepSleepSystem(); // poweroff
    }
  }
  if((millis() - previousTime) > 10000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}

/* ========================= WAKE UP REASON ========================= */
void wakeup_reason(){
  time_t now;
  time(&now);
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
      flag = false;
      //sleep_enter_time = now; // OJO EN PRUEBA
      Serial.println("Wakeup caused by rpi request");
      rpiRequest();     // function for response RPI
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
      Serial.println("Wakeup caused by rain counter pin");
      //runRainCounter();  // Rain counter
      //deepSleepSystem();
      break;
    case ESP_SLEEP_WAKEUP_TIMER :
      sleep_enter_time = now;
      Serial.println("Wakeup caused by timer");
      init_running();
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD :
      sleep_enter_time = now;
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP :
      sleep_enter_time = now;
      Serial.println("Wakeup caused by ULP program");
      break;
    default :
      sleep_enter_time = now;
      Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason);
      rtc_gpio_init(RUN);
      rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RUN);
      rtc_gpio_set_level(RUN, LOW);
      rtc_gpio_hold_en(RUN);
      init_running();
      break;
  }
}

/* ====================== MAIN SETUP ======================== */
void setup() {

  Serial.begin(115200);
  Serial2.begin(115200,SERIAL_8N1,16,17);   // SERIAL CONNECT TO RPI

  // WAKE UP SETTINGS
  rtc_gpio_pulldown_en(ESP32MAN);           // wake up by rpi requests
  esp_sleep_enable_ext0_wakeup(ESP32MAN,1);

  // LED RUNNING INDICATOR
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // INIT SPIFFS MODULE
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  wakeup_reason();
}

void loop() {
  
  if((millis() - previousTime) > 30000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}


