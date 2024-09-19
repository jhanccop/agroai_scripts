// {"type":"tank","mac":"48:E7:29:97:23:A0","count":3,"value":"3.862","temp":"84.987","sampleRate":60,"tankHeight":"5.000"}

// {"timeSleep":3575}

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
//#include <Average.h>
#include <driver/rtc_io.h>
#include <PubSubClient.h>
#include "SPIFFS.h"

#include <DHT.h>;
#define DHTPIN 12
#define DHTPOWER 32
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
const char *broker = "broker.hivemq.com"; // broker.emqx.io
const int mqtt_port = 1883;
const char *topicSubscribe = "jhpOandG/settings";
const char *topicPublish = "jhpOandG/data";

WiFiClient espClient;
PubSubClient client(espClient);

/* ====================== DEVICE SETTINGS ======================== */
#define RUN GPIO_NUM_25 // RPI control PIN 25
#define pinBattery 39 // input voltage
#define pinConfigAP 23 // ACCESS SETTINGS
#define RPI_STATUS GPIO_NUM_26 // RPI STATUS 
#define RPI_MANAGER GPIO_NUM_4 // RPI MANAGER 

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex wakw up by ext0

unsigned long previousTime = 0;
boolean completed = false;
boolean status = false;
boolean continuous = false;
int refresh = 10;

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
      <h4 style="color:DodgerBlue;">%value% F </h4> 
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
  // de-energize RPI
  //rtc_gpio_set_level(RUN, HIGH);
  //(RUN);

  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 

  // deep sleep start 
  Serial.print("sleep for min: ");
  Serial.println(TIME_TO_SLEEP);
  uint64_t SLEEPTIME = TIME_TO_SLEEP;
  esp_sleep_enable_timer_wakeup(SLEEPTIME * 60 * uS_TO_S_FACTOR);
  //Serial.flush(); 
  esp_deep_sleep_start();
}

// enable RPI
void enableRPI(){
  rtc_gpio_init(RUN);
  rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(RUN);
  rtc_gpio_set_level(RUN, HIGH);
  delay(1000);
  rtc_gpio_set_level(RUN, LOW);
  delay(1000);
  rtc_gpio_set_level(RUN, HIGH);

}

void disableRPI(){
  rtc_gpio_init(RUN);
  rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(RUN);

  rtc_gpio_set_level(RUN, HIGH);
  rtc_gpio_hold_en(RUN);
}

int voltage(){
  return map(analogRead(pinBattery),1142,1271,508,582);
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
    return String(hum,3) + " / " + String(temp,3);
  } else if (var == "dato") {
    int vol = voltage();
    return String(vol);
  }
  return String();
}

/* ====================== CALLBACK ======================== */
void callback(char *topic, byte *payload, unsigned int length) {
  if (String(topic) == String(topicSubscribe))
  {
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje");
    Serial.println(msg_in);
    StaticJsonDocument<50> docIn;
    DeserializationError error = deserializeJson(docIn, msg_in);
    if (error) {
      //Serial.print(F("deserializeJson() failed: "));
      //Serial.println(error.f_str());
      return;
    }
    int Time = docIn["timesleep"];
    boolean Status = docIn["status"];
    boolean Continuous = docIn["continuous"];
    int Refresh = docIn["refresh"];

    TIME_TO_SLEEP = Time;
    status = Status;
    continuous = Continuous;
    refresh =Refresh;

    writeFile(SPIFFS, "/timesleep.txt", String(Time).c_str());
    writeFile(SPIFFS, "/status.txt", String(status).c_str());
    writeFile(SPIFFS, "/continuous.txt", String(continuous).c_str());
    writeFile(SPIFFS, "/refresh.txt", String(refresh).c_str());

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 60 * uS_TO_S_FACTOR);

    init_running_2();
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
void reconnect()
{
  int count = 0;
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str())) {
      //String topiSub = String(topicSubscribe) + "/" + String(WiFi.macAddress());
      //client.subscribe(topiSub.c_str());
      client.subscribe(topicSubscribe);
      Serial.println(topicSubscribe);
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
        //String topiSub = String(topicSubscribe) + "/" + String(WiFi.macAddress());
        //client.subscribe(topiSub.c_str());
        client.subscribe(topicSubscribe);
        Serial.println(topicSubscribe);
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
    docInit["type"] = "camVidSet";
    docInit["mac"] = WiFi.macAddress();

    String payloadInit = "";
    serializeJson(docInit, payloadInit);
    
    Serial.println(payloadInit);
    client.publish(topicPublish, payloadInit.c_str());

  }

}

void init_running_2(){
  /* ================================================================================= */
  int vB = voltage();

  //if(status && vB >= 400){ /* RPI RUNNING */
  if(status){ /* RPI RUNNING */
    // RUN WRITE HIGH TO CONINUE RUNNING

    rtc_gpio_init(RUN);
    rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(RUN);
    rtc_gpio_set_level(RUN, HIGH);
    rtc_gpio_hold_en(RUN);

    // MANAGER WHITE LOW TO CONINUE RUNNING
    rtc_gpio_init(RPI_MANAGER);
    rtc_gpio_set_direction(RPI_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(RPI_MANAGER);
    rtc_gpio_set_level(RPI_MANAGER, LOW);
    rtc_gpio_hold_en(RPI_MANAGER);

    Serial.println(continuous);

    
  }else{ /* RPI SHUTDOWN */

    if(continuous){
      // MANAGER WHITE LOW TO CONINUE RUNNING
      rtc_gpio_init(RPI_MANAGER);
      rtc_gpio_set_direction(RPI_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RPI_MANAGER);
      rtc_gpio_set_level(RPI_MANAGER, HIGH);
      rtc_gpio_hold_en(RPI_MANAGER);

      delay(15000);

      // RUN WRITE LOW TO CONINUE RUNNING
      rtc_gpio_init(RUN);
      rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
      rtc_gpio_hold_dis(RUN);
      rtc_gpio_set_level(RUN, LOW);
      rtc_gpio_hold_en(RUN);
    }

  }

  // Send message, sensors ann keep alive data
  StaticJsonDocument<200> docOut;
  docOut["type"] = "camVid";
  docOut["mac"] = WiFi.macAddress();//readMacAddress();
  docOut["T"] = String(dht.readTemperature(),2);
  docOut["H"] = String(dht.readHumidity(),2);
  docOut["B"] = voltage();
  docOut["S"] = !digitalRead(RPI_STATUS);

  String payload = "";
  serializeJson(docOut, payload);
  
  Serial.println(payload);
  client.publish(topicPublish, payload.c_str());

  delay(1000);

  //rpiRequest();

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
void infoESP32(){
  // READ CURRENTLY SETTING
  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt();
  status = readFile(SPIFFS, "/status.txt").toInt();
  continuous = readFile(SPIFFS, "/continuous.txt").toInt();
  refresh = readFile(SPIFFS, "/refresh.txt").toInt();

  JsonDocument doc;
  doc["mac"] = WiFi.macAddress(); //readMacAddress();
  doc["T"] = String(dht.readTemperature(),2);
  doc["H"] = String(dht.readHumidity(),2);
  doc["B"] = voltage(); //5.08 v - 5.82 v

  doc["timesleep"] = TIME_TO_SLEEP;
  doc["status"] = bool(status);
  doc["continuous"] = bool(continuous);
  doc["refresh"] = refresh;

  //serializeJson(doc, Serial);
  char output[256];
  serializeJson(doc, output);

  // SEND DATA BY SERIAL TO RASPBERRY
  Serial2.write(output);
  Serial2.write("\n");
  Serial.println(output);
}

void rpiRequest(){
  while (true){
    previousTime = millis();
    if (Serial2.available())
    {
      char char_array[256];  // assumption: it will never exceed 255 characters, based on the example output
      uint8_t length = 0;
      String msgin = "";
      while (Serial2.available() && length < 255) {
        char ch = Serial2.read();
        msgin += (char)ch;
      }

      Serial.println("msg arrived");
      Serial.println(msgin);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, msgin);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        break;
        return;
      }

      const char* msg = doc["msg"];
      if(String(msg) == "info"){
        infoESP32();
        digitalWrite(DHTPOWER,LOW);
        deepSleepSystem();
      }else if(String(msg) == "completed"){

        deepSleepSystem();
      }
      else if(String(msg) == "wifi"){
        infoWifi();
      }
      else if(String(msg) == "sleep"){
        rtc_gpio_init(RUN);
        rtc_gpio_set_direction(RUN, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_hold_dis(RUN);
        rtc_gpio_set_level(RUN, LOW);
        rtc_gpio_hold_en(RUN);

        delay(15);

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
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
      flag = false;
      Serial.println("Wakeup caused by rpi request");
      // function for response RPI
      rpiRequest();
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER :
      Serial.println("Wakeup caused by timer");
      init_running();
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD :
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP :
      Serial.println("Wakeup caused by ULP program");
      break;
    default :
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
  Serial2.begin(115200,SERIAL_8N1,16,17); // SERIAL CONNECT TO RPI
  rtc_gpio_pulldown_en(GPIO_NUM_33);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1);

  // LED RUNNING INDICATOR
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // START DHT SENSOR
  pinMode(DHTPOWER,OUTPUT);
  digitalWrite(DHTPOWER,HIGH);
  dht.begin(); // sensor

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
}
