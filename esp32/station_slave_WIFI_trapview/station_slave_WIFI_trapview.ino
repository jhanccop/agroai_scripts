// {"type":"tank","mac":"48:E7:29:97:23:A0","count":3,"value":"3.862","temp":"84.987","sampleRate":60,"tankHeight":"5.000"}

// {"timeSleep":3575}

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include <driver/rtc_io.h>
#include <PubSubClient.h>
#include "SPIFFS.h"

#include <esp_sleep.h>

#include <DHT.h>;
#define DHTPIN 13
#define LED 2
#define DHTTYPE DHT21 //DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

AsyncWebServer server(80);

const char* ssid = "uS-sensor";     // original ssid
const char* password = "12345678";  // original password

String runssid = "";
String runpassword = "";

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
const char* PARAM_DEEP_SLEEP = "deepSleepTime";
const char* PARAM_TANK_HEIGHT = "tankHeight";
const char* PARAM_RESET = "reset";

/* ====================== MQTT ======================== */
//const char *broker = "broker.emqx.io"; // broker.hivemq.com
const char *broker = "24.199.125.52";
const int mqtt_port = 1883;
const char *topicSubscribe = "jhpOandG/settings";
const char *topicPublish = "jhpOandG/data";

WiFiClient espClient;
PubSubClient client(espClient);

/* ====================== DEVICE SETTINGS ======================== */
#define RUN GPIO_NUM_25         // RPI control PIN 25
#define pinBattery 39           // pin Battery
#define pinPanel 34             // pin Panel
#define pinWindDirection 35     // pin Wind direction
#define pinWindVelocity 26     // pin Wind direction
#define pinConfigAP 23          //enter configuration Acces point mode
#define RAINCOUNT GPIO_NUM_14   // wake up by rain counter pin 14 2^14 0x4000 
#define ESP32MAN GPIO_NUM_33    // wake up esp32manager pin
#define RPI_MANAGER GPIO_NUM_32 // RPI MANAGER 

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
RTC_DATA_ATTR time_t sleep_enter_time;

unsigned long previousTime = 0;
boolean completed = false;
boolean status = false;
boolean continuous = false;
boolean saveImage = false;
boolean runningNN = false;

String gateway = "";

int refresh = 10;
int pulsos = 0;

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
      <h4 style="color:DodgerBlue;">%value% </h4> 
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

int voltage(int pin){
  return map(analogRead(pin),698,2416,199,602);
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
    double hum = dht.readHumidity();
    double temp= dht.readTemperature();
    return String(hum,3) + " per | " + String(temp,3) + " C";
  } else if (var == "dato") {
    int vol = voltage(pinBattery);
    return String(vol);
  }
  return String();
}

void saveSetting(String value, String fil){
  String orValue = readFile(SPIFFS, fil.c_str());
  if(orValue != value ){
    writeFile(SPIFFS, fil.c_str(), value.c_str());
  }
}

/* ====================== CALLBACK ======================== */
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("llegada de mensaje tpic ");
  Serial.println(topic);

  if (String(topic) == String(topicSubscribe))
  {
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje");
    Serial.println(msg_in);
    StaticJsonDocument<80> docIn;
    DeserializationError error = deserializeJson(docIn, msg_in);
    if (error) {
      return;
    }
    boolean Status = docIn["status"];
    int Time = docIn["timesleep"];
    boolean A_TH = docIn["A_TH"];
    boolean A_WS = docIn["A_TH"];
    boolean RunningNN = docIn["runningNN"];
    int Refresh = docIn["refresh"];

    TIME_TO_SLEEP = Time;
    status = Status;
    //continuous = Continuous;
    //saveImage = SaveImage;
    runningNN = RunningNN;
    refresh = Refresh;

    saveSetting(String(Status), "/status.txt");
    saveSetting(String(Time), "/timesleep.txt");
    saveSetting(String(A_TH), "/A_TH.txt");
    saveSetting(String(A_WS), "/A_WS.txt");
    saveSetting(String(RunningNN), "/runningNN.txt");

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 60 * uS_TO_S_FACTOR);

    init_running_2();
  }else if(String(topic) == String(topicSubscribe) + "/" + String(WiFi.macAddress())){
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje seccion DEVICE ");
    Serial.println(msg_in);
    StaticJsonDocument<80> docIn;
    DeserializationError error = deserializeJson(docIn, msg_in);
    if (error) {
      return;
    }

    const char* function = docIn["function"];

    if(String(function) == "setting"){
      Serial.println("setting-------------------");

      boolean Status = docIn["status"];
      int Time = docIn["timesleep"];
      boolean A_TH = docIn["A_TH"];
      boolean A_WS = docIn["A_WS"];
      boolean RunningNN = docIn["runningNN"];

      const char* Gateway = docIn["gateway"];

      gateway = Gateway;

      TIME_TO_SLEEP = Time;
      status = Status;

      saveSetting(String(Gateway), "/gateway.txt");
      saveSetting(String(status), "/status.txt");
      saveSetting(String(Time), "/timesleep.txt");
      //writeFile(SPIFFS, "/A_TH.txt", String(A_TH).c_str());
      //writeFile(SPIFFS, "/A_WS.txt", String(A_WS).c_str());
      saveSetting(String(A_TH), "/A_TH.txt");
      saveSetting(String(A_WS), "/A_WS.txt");
      saveSetting(String(RunningNN), "/runningNN.txt");

      init_running_2();

    }
  }else if(String(topic) == String(topicSubscribe) + "/" + gateway){
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje seccion GATEWAY ");
    Serial.println(msg_in);
    StaticJsonDocument<80> docIn;
    DeserializationError error = deserializeJson(docIn, msg_in);
    if (error) {
      return;
    }

    const char* function = docIn["function"];

    if(String(function) == "poweroff"){
      Serial.println("poweroff-------------------");
      deepSleepSystem();
    }
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
    writeFile(SPIFFS, "/ssidRainCounter.txt", "0");

    request->send(200, "text/text", inputMessage);
  });

  server.onNotFound(notFound);
  server.begin();

  //rtc_gpio_hold_dis(GPIO_NUM_15);

  while (1) {
    delay(25);
  }

}

/* ====================== RECONNECT ======================== */
void reconnect(){
  int count = 0;
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      String topiSub = String(topicSubscribe) + "/#";
      client.subscribe(topiSub.c_str());
      //client.subscribe(topicSubscribe);
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

/* ====================== SEND DATA ESP32 ======================== */
void sendData(){

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  StaticJsonDocument<200> docOut;
  docOut["type"] = "trapView";
  docOut["mac"] = WiFi.macAddress();                            // MacAddress();
  docOut["B"] = voltage(pinBattery);                            // battery Voltage 
  docOut["P"] = voltage(pinPanel);                                 // panel Voltage

  int ATH = readFile(SPIFFS, "/A_TH.txt").toInt();
  int AWS = readFile(SPIFFS, "/A_WS.txt").toInt();

  if(ATH){
    // START DHT SENSOR
    dht.begin(); // sensor
    docOut["T"] = String(dht.readTemperature(),2);
    docOut["H"] = String(dht.readHumidity(),2);
  }

  if(AWS){
    docOut["R"] = readFile(SPIFFS, "/ssidRainCounter.txt").toInt();  // Rain counter
    docOut["V"] = String(WindVelocity(),2);
    docOut["D"] = WindDirection();
  }

  String payload = "";
  serializeJson(docOut, payload);
  
  Serial.println(payload);
  client.publish(topicPublish, payload.c_str());
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

    WiFi.begin(runssid, runpassword);
    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.println("Connecting to WiFi..");
    }
    
    client.setServer(broker, mqtt_port);
    client.setCallback(callback);

    int count = 0;

    while (!client.connected()) {
      String client_id = "esp32-client-";
      client_id += String(WiFi.macAddress());
      //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
      if (client.connect(client_id.c_str())) {
        Serial.println("broker connected");
        String topiSub = String(topicSubscribe) + "/#";
        client.subscribe(topiSub.c_str());
        //client.subscribe(topicSubscribe);
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

    StaticJsonDocument<50> docInit;
    // {"type":"camVidSet","name" : "cam1","mac":"d4:8a:fc:a5:7a:58"}
    docInit["type"] = "trapViewSetting";
    docInit["function"] = "setting";
    docInit["mac"] = WiFi.macAddress();

    String payloadInit = "";
    serializeJson(docInit, payloadInit);
    Serial.print("ARRIVE SETTING ");
    Serial.println(payloadInit);
    client.publish(topicPublish, payloadInit.c_str());
    previousTime = millis();
  }
}

void init_running_2(){
  /* ================================================================================= */
  int vB = voltage(pinBattery);

  //if(status && vB >= 400){ /* RPI RUNNING */
  if(status){ /* RPI RUNNING */
    // RUN WRITE HIGH TO CONINUE RUNNING

    rtc_gpio_init(RUN);
    rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(RUN);
    rtc_gpio_set_level(RUN, LOW);

    delay(1000);

    rtc_gpio_set_level(RUN, HIGH);
    rtc_gpio_hold_en(RUN);

    Serial.println(continuous);

    if(continuous){
      // MANAGER WHITE LOW TO CONINUE RUNNING
      rtc_gpio_init(RPI_MANAGER);
      rtc_gpio_set_direction(RPI_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RPI_MANAGER);
      rtc_gpio_set_level(RPI_MANAGER, LOW);
      rtc_gpio_hold_en(RPI_MANAGER);

      // Send message, sensors ann keep alive data
      sendData();

    }else{
      // MANAGER WHITE LOW TO CONINUE RUNNING
      rtc_gpio_init(RPI_MANAGER);
      rtc_gpio_set_direction(RPI_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RPI_MANAGER);
      rtc_gpio_set_level(RPI_MANAGER, HIGH);
      rtc_gpio_hold_en(RPI_MANAGER);
    }

  }else{
    
    /* RPI SHUTDOWN */
    rtc_gpio_init(RPI_MANAGER);
    rtc_gpio_set_direction(RPI_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(RPI_MANAGER);
    rtc_gpio_set_level(RPI_MANAGER, HIGH);
    rtc_gpio_hold_en(RPI_MANAGER);
    
    // Send message, sensors ann keep alive data
    sendData();
    delay(15000);

    // RUN WRITE LOW TO CONINUE RUNNING
    rtc_gpio_init(RUN);
    rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(RUN);
    rtc_gpio_set_level(RUN, LOW);
    rtc_gpio_hold_en(RUN);
  }
  delay(1000);
  //rpiRequest();
  Serial.println("start deep sleep RUNING 2");
  
  esp_deep_sleep_start();
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
  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt();
  status = readFile(SPIFFS, "/status.txt").toInt();
  continuous = readFile(SPIFFS, "/continuous.txt").toInt();
  refresh = readFile(SPIFFS, "/refresh.txt").toInt();
  saveImage = readFile(SPIFFS, "/saveImage.txt").toInt();
  runningNN = readFile(SPIFFS, "/runningNN.txt").toInt();

  JsonDocument doc;

  doc["timesleep"] = TIME_TO_SLEEP;
  doc["status"] = bool(status);
  doc["continuous"] = bool(continuous);
  doc["refresh"] = refresh;
  doc["gateway"] = gateway;
  doc["saveImage"] = bool(saveImage);
  doc["runningNN"] = bool(runningNN);
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

  JsonDocument docOut;
  docOut["mac"] = WiFi.macAddress();
  docOut["B"] = voltage(pinBattery);                            // battery Voltage 5.08 v - 5.82 v
  docOut["P"] = voltage(pinPanel);                                 // panel Voltage

  int ATH = readFile(SPIFFS, "/A_TH.txt").toInt();
  int AWS = readFile(SPIFFS, "/A_WS.txt").toInt();

  if(ATH){
    // START DHT SENSOR
    dht.begin(); // sensor
    docOut["T"] = String(dht.readTemperature(),2);
    docOut["H"] = String(dht.readHumidity(),2);
  }

  if(AWS){
    docOut["R"] = readFile(SPIFFS, "/ssidRainCounter.txt").toInt();  // Rain counter
    docOut["V"] = String(WindVelocity(),2);
    docOut["D"] = WindDirection();
  }
  
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

        Serial.println("start deep sleep");

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

  rtc_gpio_pullup_en(RAINCOUNT);          // wake up by rain counter
  esp_sleep_enable_ext1_wakeup(0x4000,ESP_EXT1_WAKEUP_ALL_LOW); // Pin 14 to rain counter

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
  if (flag){
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    
    delay(200);
  }

  if((millis() - previousTime) > 30000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}


