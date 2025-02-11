/*AMEBA PRODUCTION*/
#define ARDUINOJSON_STRING_LENGTH_SIZE 4

#include "time.h"
#include <driver/rtc_io.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "DHT.h"

#include <esp_sleep.h>
#include <DHT.h>;

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

const char* ssid = "uS-sensor";     // original ssid
const char* password = "12345678";  // original password

String RUNssid = "";
String RUNpass = "";

int port = 8000;

const char sHostname_buffer[] = "http://192.168.3.31";  //192.168.3.31:8000/devices/api/items/1/ 24.199.125.52

//char sHostname_buffer[50] = ;
String sHostname = "";

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
RTC_DATA_ATTR time_t sleep_enter_time;

unsigned long previousTime = 0;
boolean completed = false;
boolean status = false;
int pulsos = 0;

boolean flag = true;

#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex wakw up by ext0

String SearchPath = "/devices/api/buscar-weather-station/?mac=";
String csrfPath = "/get-csrf/";
String postPath = "/data/api/post-weather-station";
int statusW = WL_IDLE_STATUS;

/* device settings */
String DeviceName = "";
String DeviceMacAddress = "";
boolean A_TH = false;
boolean A_WP = false;
boolean A_RS = false;
uint32_t SleepTime = 60;

String csrfToken = "";

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
const char* PARAM_DEEP_SLEEP = "deepSleepTime";
const char* PARAM_TANK_HEIGHT = "tankHeight";
const char* PARAM_RESET = "reset";

/* ====================== DEVICE SETTINGS ======================== */
#define pinBattery A0           // pin Battery
#define pinWindDirection 35     // pin Wind direction
#define pinWindVelocity 26     // pin Wind direction
#define pinConfigAP 10          //enter configuration Acces point mode
#define RAINCOUNT GPIO_NUM_7   // wake up by rain counter pin 14 2^7 0x0080 

#define DHTPIN 4
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

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
    float hum = dht.readHumidity();
    float temp= dht.readTemperature();
    return String(hum,1) + "per | " + String(temp,1) + " C";
  } else if (var == "dato") {
    String vol = vBat();
    return vol;
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

/* =========================== GET SETTINGS =============================*/
void getHttp() {
  int err = 0;

  HTTPClient http;
  String mac = WiFi.macAddress();
  SearchPath = SearchPath + mac;
  Serial.println(sHostname_buffer);
  Serial.println(SearchPath);
  Serial.println(port);

  String msg_in = "";
  String urlComplet = String(sHostname_buffer) + ":" + String(port) + SearchPath;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(urlComplet);
    http.begin(urlComplet);
    err = http.GET();
    if (err > 0) {
      msg_in = http.getString();
      Serial.println("Código HTTP: " + String(err));
      Serial.println("Respuesta: " + msg_in);
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
  boolean a_TH = docIn["A_TH"];
  boolean a_WP = docIn["A_WP"];
  boolean a_RS = docIn["A_RS"];
  int sleepTime = docIn["SleepTime"];

  DeviceName = String(deviceName);
  DeviceMacAddress = String(deviceMacAddress);
  A_TH = a_TH;
  A_WP = a_WP;
  A_RS = a_RS;
  SleepTime = sleepTime;

  Serial.println(DeviceName);
  Serial.println(DeviceMacAddress);
  Serial.println(A_TH);
  Serial.println(SleepTime);
  
}

void getcsrfHttp() {
  int err = 0;

  HTTPClient http;

  String msg_in = "";
  String urlComplet = String(sHostname_buffer) + ":" + String(port) + csrfPath;
  
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(urlComplet);
    err = http.GET();
    if (err > 0) {
      msg_in = http.getString();
      Serial.println("Código HTTP: " + String(err));
      Serial.println("Respuesta: " + msg_in);
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

String vBat(){
  long raw = map(analogRead(pinBattery),204,453,201,368);
  //long raw = analogRead(pinBattery);

  return String(raw*0.01,2);
}

/* =========================== POST DATA =============================*/
void postHttp() {

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

  if(A_WP){
    docOut["WindVelocity"] = WindVelocity();
    docOut["WindDirection"] = WindDirection();
    docOut["RainCounter"] = readFile(SPIFFS, "/ssidRainCounter.txt").toInt();
  }

  if(A_RS){
    docOut["Radiation"] = 32;
  }

  docOut["DeviceMacAddress"] = WiFi.macAddress();
  docOut["VoltageBattery"] = vBat();

  String jsonString;
  serializeJson(docOut, jsonString);

  Serial.println(jsonString);

  String request = "POST /data/api/post-weather-station HTTP/1.1\r\n";
  request += "Host: " + String(sHostname_buffer) + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "X-CSRFToken: " + csrfToken +  "\r\n";
  request += "Cookie: csrftoken=" + csrfToken + "\r\n";
  request += "Content-Length: " + String(jsonString.length()) + "\r\n";
  request += "Connection: keep-alive\r\n";
  request += "\r\n";
  request += jsonString;

  HTTPClient http;

  String urlComplet = String(sHostname_buffer) + ":" + String(port) + postPath;
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

  /*
  wifiClient.setTimeout(30000);
  if (wifiClient.connect(sHostname_buffer, port)) {
    
    wifiClient.print(request);
  }
  */
}

/* =========================== WIFI STATUS =============================*/
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

/* ========================= WEATHER FUNCTIONS ========================= */
void runRainCounter(){
  int rainCounter = 0;
    rainCounter = readFile(SPIFFS, "/ssidRainCounter.txt").toInt();
  rainCounter++;
  String counter = String(rainCounter);
  writeFile(SPIFFS, "/ssidRainCounter.txt", counter.c_str());
  delay(200);

  //digitalWrite(LED, LOW);

  time_t now;
  time(&now);

  int sleep_time = difftime(now, sleep_enter_time);
  Serial.printf("Tiempo en deep sleep: %d segundos\n", sleep_time);
  
  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 

  /*--- deep sleep start ***/ 
  Serial.println("sleep start RAIN COUNT");
  Serial.println(sleep_enter_time);
  Serial.println(now);
  uint64_t SLEEPTIME = TIME_TO_SLEEP * 60 - sleep_time;
  Serial.println(SLEEPTIME);
  esp_sleep_enable_timer_wakeup(SLEEPTIME * uS_TO_S_FACTOR);

  esp_deep_sleep_start();
}

int WindDirection(){
  int dir[8] = {0,45,90,135,180,225,270,315};
  int raw[8] = {3051, 1732, 234, 598, 1021, 2419, 3955, 3539};

  int tol = 100;
  int val = analogRead(pinWindDirection);
  if(val > 4090){
    return -90;
  }

  int value;
  for(int i = 0; i < 8; i++){
    if(abs(val - raw[i]) <= tol){
      value = dir[i];
      return value;
    }
  }

  return -180;
}

float WindVelocity(){
  pinMode(pinWindVelocity, INPUT_PULLUP);
  float velocity = 0;
  int counter = 0;
  boolean lastState = digitalRead(pinWindVelocity);
  for(int i = 0; i < 600; i++){
    if(digitalRead(pinWindVelocity) != lastState){
      lastState = digitalRead(pinWindVelocity);
      counter++;
      Serial.println(counter);
      delay(10);
    }
    delay(5);
  }
  velocity = counter/9.00;
  return velocity;
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
      Serial.println("Wakeup caused by rpi request");
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
      Serial.println("Wakeup caused by rain counter pin");
      runRainCounter();  // Rain counter
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
      //rtc_gpio_init(RUN);
      //rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
      //rtc_gpio_hold_dis(RUN);
      //rtc_gpio_set_level(RUN, LOW);
      //rtc_gpio_hold_en(RUN);
      init_running();
      break;
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
    RUNssid = readFile(SPIFFS, "/ssidString.txt");     // original ssid
    RUNpass = readFile(SPIFFS, "/passString.txt");  // original password

    int n = 0;
    WiFi.begin(RUNssid, RUNpass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.println("Connecting to WiFi..");
      if(n >= 5){
        ESP.restart();
      }
      n++;
    }

    Serial.println("Connected to wifi");
    printWifiStatus();

    getHttp();

    getcsrfHttp();

    Serial.println("start post------>");
    postHttp();

    delay(500);

    Serial.print("sleep for min: ");
    Serial.println(TIME_TO_SLEEP);
    uint64_t SLEEPTIME = TIME_TO_SLEEP;
    esp_deep_sleep_enable_gpio_wakeup(BIT(D5), ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_sleep_enable_timer_wakeup(SLEEPTIME * 60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  //readConfigFromSD();

  //rtc_gpio_pullup_en(RAINCOUNT);          // wake up by rain counter
  //esp_sleep_enable_ext1_wakeup(0x0080,ESP_EXT1_WAKEUP_ALL_LOW); // Pin GPIO7 to rain counter
  esp_deep_sleep_enable_gpio_wakeup(0x0080, ESP_GPIO_WAKEUP_GPIO_LOW);
  
  // INIT SPIFFS MODULE
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  wakeup_reason();
  
}

void loop() {
  
}


