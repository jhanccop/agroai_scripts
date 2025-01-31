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

AsyncWebServer server(80);

const char* ssid = "uS-sensor";     // original ssid
const char* password = "12345678";  // original password

String runssid = "";
String runpassword = "";

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
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
#define MODEM_POWER GPIO_NUM_27         // RPI control PIN 27
#define pinBattery 39           // pin Battery
#define pinPanel 36           // pin Battery
#define LED 2           // pin Battery
#define pinConfigAP 16           // pin Battery
#define MODEM_MANAGER GPIO_NUM_25 // RPI MANAGER 

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
RTC_DATA_ATTR time_t sleep_enter_time;

unsigned long previousTime = 0;
unsigned long waitTime = 60000;
boolean status = false;
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
  poweroffdevices();
  delay(500);
  digitalWrite(LED, LOW);

  rtc_gpio_init(MODEM_POWER);
  rtc_gpio_set_direction(MODEM_POWER, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(MODEM_POWER);
  rtc_gpio_set_level(MODEM_POWER,LOW);
  rtc_gpio_hold_en(MODEM_POWER);

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

void poweroffdevices(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  StaticJsonDocument<50> docInit;
  // {"type":"camVidSet","name" : "cam1","mac":"d4:8a:fc:a5:7a:58"}
  docInit["function"] = "poweroff";

  String payloadInit = "";
  serializeJson(docInit, payloadInit);
  
  Serial.println(payloadInit);
  String topicPub = String(topicSubscribe) + "/" + gateway;
  Serial.println(topicPub);
  client.publish(topicPub.c_str(), payloadInit.c_str());
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
  } else if (var == "MAC") {
    return WiFi.macAddress();
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

  if(String(topic) == String(topicSubscribe) + "/" + String(WiFi.macAddress())){
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje seccion 2 ");
    Serial.println(msg_in);
    StaticJsonDocument<80> docIn;
    DeserializationError error = deserializeJson(docIn, msg_in);
    if (error) {
      return;
    }

    const char* function = docIn["function"];

    if(String(function) == "setting"){
      int Time = docIn["timesleep"];
      boolean Status = docIn["status"];
      const char* Gateway = docIn["gateway"];

      gateway = Gateway;

      saveSetting(String(Status), "/status.txt");
      saveSetting(String(Time), "/timesleep.txt");
      saveSetting(String(Gateway), "/gateway.txt");

      if(Status){
        waitTime = 120000;
      }

      sendData();

      //writeFile(SPIFFS, "/gateway.txt", String(Gateway).c_str());
      //writeFile(SPIFFS, "/status.txt", String(status).c_str());
    }
  }else if(String(topic) == String(topicSubscribe) + "/" + gateway){
    String msg_in = "";
    for (int i = 0; i < length; i++)
    {
      msg_in += String((char)payload[i]);
    }
    Serial.println("llegada de mensaje seccion 2 ");
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
    else if (request->hasParam(PARAM_RESET)) {
      ESP.restart();
    }
    else {
      inputMessage = "No message sent";
    }

    Serial.println(inputMessage);

    request->send(200, "text/text", inputMessage);
  });

  server.onNotFound(notFound);
  server.begin();

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
  docOut["type"] = "gatewayData";
  docOut["mac"] = WiFi.macAddress();                            // MacAddress();
  docOut["B"] = voltage(pinBattery);                            // battery Voltage 
  docOut["P"] = voltage(pinPanel);                              // panel Voltage

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

    // POWER MODEM
    rtc_gpio_init(MODEM_POWER);
    rtc_gpio_set_direction(MODEM_POWER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(MODEM_POWER);
    rtc_gpio_set_level(MODEM_POWER, HIGH);
    rtc_gpio_hold_en(MODEM_POWER);

    delay(500);

    // ACTIVATE MODEM
    rtc_gpio_init(MODEM_MANAGER);
    rtc_gpio_set_direction(MODEM_MANAGER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_hold_dis(MODEM_MANAGER);
    rtc_gpio_set_level(MODEM_MANAGER, LOW);

    delay(3500);

    rtc_gpio_set_level(MODEM_MANAGER, HIGH);
    rtc_gpio_hold_en(MODEM_MANAGER);

    // START RUNNING
    delay(20000);

    //Connect and send initial data to get the configuration
    runssid = readFile(SPIFFS, "/ssidString.txt");     // original ssid
    runpassword = readFile(SPIFFS, "/passString.txt");  // original password

    Serial.println(WiFi.macAddress());
    Serial.println(runssid);
    Serial.println(runpassword);

    int count = 0;

    WiFi.begin(runssid, runpassword);
    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.println("Connecting to WiFi...");
      if (count == 5)
        {
          //Serial.println("reset mqtt");
          ESP.restart();
        }
    }
    
    client.setServer(broker, mqtt_port);
    client.setCallback(callback);

    count = 0;

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
      }else{
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
    docInit["type"] = "gatewaySetting";
    docInit["function"] = "setting";
    docInit["mac"] = WiFi.macAddress();

    String payloadInit = "";
    serializeJson(docInit, payloadInit);
    
    Serial.println(payloadInit);
    client.publish(topicPublish, payloadInit.c_str());

  }

  // WAIT FOR COMPLETE TASK
  //delay(30000);

}

/* ====================== MAIN SETUP ======================== */
void setup() {

  Serial.begin(115200);

  // LED RUNNING INDICATOR
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // INIT SPIFFS MODULE
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  init_running();
}

void loop() {
  if (flag){
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    
    delay(200);
  }

  if((millis() - previousTime) > waitTime){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}
