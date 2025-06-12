/*weather station PRODUCTION 15-02-2025*/
#define ARDUINOJSON_STRING_LENGTH_SIZE 4

#include "time.h"
#include <driver/rtc_io.h>
#include <SPIFFS.h>

#include <ArduinoJson.h>

#include <esp_sleep.h>

/* ====================== DHT SETTINGS ======================== */
#include "DHT.h"
#define DHTPIN 22
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

/* ====================== WEBSERVER SETTINGS ======================== */
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

AsyncWebServer server(80);

const char* ssid = "WeatherStation";     // original ssid
const char* password = "12345678";  // original password

String RUNssid = "";
String RUNpass = "";

int port = 8000;


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


// LilyGO T-SIM7000G Pinout
#define UART_BAUD           115200
#define PIN_DTR             25
#define PIN_TX              27
#define PIN_RX              26
#define PWR_PIN             GPIO_NUM_4

#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13
#define LED_PIN             12

/* ============================== DEVICE CONFIGURATIONS ============================== */

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 60; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count
RTC_DATA_ATTR time_t sleep_enter_time;

unsigned long previousTime = 0;
boolean completed = false;
boolean status = false;
int pulsos = 0;

boolean flag = true;

//#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex wakw up by ext0

String SearchPath = "/devices/api/buscar-weather-station/?mac=";
String csrfPath = "/get-csrf/";
String postPath = "/data/api/post-weather-station";
int statusW = WL_IDLE_STATUS;

/* device settings */
String DeviceName = "";
String DeviceMacAddress = "";
boolean A_TH = false;
boolean A_WS = false;
boolean A_RS = false;

String csrfToken = "";

const char* PARAM_SSID = "ssidString";
const char* PARAM_PASS = "passString";
const char* PARAM_SERVER= "server";
const char* PARAM_PORT = "port";
const char* PARAM_RESET = "reset";
const char* PARAM_CONN_TYPE = "connType";

/* ====================== MQTT ======================== */
//const char *broker = "broker.emqx.io"; // broker.hivemq.com
const char *broker = "24.199.125.52";
const int mqtt_port = 1883;
const char *topicSubscribe = "jhpOandG/settings";
const char *topicPublish = "jhpOandG/data";

#include <PubSubClient.h>
TinyGsmClient client(modem); // GPRS
PubSubClient  simclient(client);

WiFiClient espClient;       // WIFI
PubSubClient wificlient(espClient);

/* ====================== DEVICE SETTINGS ======================== */
#define pinBattery 35           // pin Battery
#define pinWindDirection 36     // pin Wind direction
#define pinWindVelocity 19      // pin Wind direction
#define pinConfigAP 23          //enter configuration Acces point mode
#define RAINCOUNT GPIO_NUM_34   // wake up by rain counter pin 14 2^7 0x0080 
String connectionType = "";

/* ====================== RS485 SETTINGS ======================== */
#define RX_PIN 33  // Pin RX 16 del ESP32
#define TX_PIN 32  // Pin TX 17 del ESP32
#define BAUD_RATE 4800
#define SERIAL_MODE SERIAL_8N1
#define MAX_BUFFER 64
uint8_t buffer[MAX_BUFFER];
#define PINRS485 21

/* ====================== HTML PAGE ======================== */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
    <title>WEATHER STATION SETTING</title>
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

  <form action="/get" target="hidden-form">
    <tr>
      <td>Connection Type: </td>
      <td>
        <select name="connType">
          <option value="WIFI" %WIFI_SELECTED%>WiFi</option>
          <option value="LTE" %LTE_SELECTED%>LTE</option>
        </select>
      </td>
      <td><input type="submit" value="Save" onclick="submitMessage()"></td>
    </tr>
  </form>

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

String macA(){
  return String("B4:8A:0A:6E:F1:20");
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
    return macA();
  } else if (var == "value") {
    double hum = dht.readHumidity();
    double temp= dht.readTemperature();
    return String(hum,1) + " per | " + String(temp,1) + " C";
  } else if (var == "dato") {
    String vol = vBat();
    return vol;
  } else if(var == "WIFI_SELECTED") {
    String connType = readFile(SPIFFS, "/connType.txt");
    if(connType == "WIFI" || connType == "") {
      return "selected";
    } else {
      return "";
    }
  } else if(var == "LTE_SELECTED") {
    String connType = readFile(SPIFFS, "/connType.txt");
    if(connType == "LTE") {
      return "selected";
    } else {
      return "";
    }
  }
  return String();
}

/* ========================= FUNCTIONS ========================= */
void deepSleepSystem(){

  digitalWrite(LED_PIN, HIGH);
  TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 
  // deep sleep start 
  Serial.print("sleep for min: **************************");
  //TIME_TO_SLEEP = 1;
  Serial.println(TIME_TO_SLEEP);
  uint64_t SLEEPTIME = TIME_TO_SLEEP;
  esp_sleep_enable_timer_wakeup(SLEEPTIME * 60 * uS_TO_S_FACTOR);
  //Serial.flush();

  modemPowerOff();
  esp_deep_sleep_start();
}

/* ====================== SETTING ======================== */
void setting() {
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  delay(1000);

  IPAddress IP = WiFi.softAPIP();
  Serial.println(IP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {

    String inputMessage;
    // GET connection type value
    if (request->hasParam(PARAM_CONN_TYPE)) {
      inputMessage = request->getParam(PARAM_CONN_TYPE)->value();
      writeFile(SPIFFS, "/connType.txt", inputMessage.c_str());
    }
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    else if (request->hasParam(PARAM_SSID)) {
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

  //uint32_t lastKeepAlive = millis();
  //while (1) {
  // Mantener viva la conexión sin bloquear completamente
  //  if (millis() - lastKeepAlive > 1000) {
      //lastKeepAlive = millis();
      // Opcionalmente, implementa alguna lógica aquí
    //}
    //delay(25); // Pequeña pausa para permitir que otras tareas se ejecuten
  //}

  while (true) {
    delay(100);
  }

}

String vBat(){
  int rawValue = analogRead(pinBattery);
  float voltage = rawValue * (3.3 / 4095.0) * 3.0;

  //long raw = map(analogRead(pinBattery),698,2416,199,602);
  return String(voltage);
}

/* ====================== READ LEVEL RS485 ======================== */
int radRS485() {

  pinMode(PINRS485,OUTPUT);

  digitalWrite(PINRS485, HIGH);

  Serial2.begin(4800, SERIAL_MODE, RX_PIN, TX_PIN);

  uint8_t command[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  size_t len = sizeof(command);
  Serial2.write(command, len);
  Serial2.flush(); 

  int bytesLeidos = 0;
  unsigned long tiempoInicio = millis();
  
  while (millis() - tiempoInicio < 1000) {  // Timeout de 1 segundo
    if (Serial2.available()) {
      buffer[bytesLeidos] = Serial2.read();
      bytesLeidos++;
      
      if (bytesLeidos >= MAX_BUFFER) {
        break;
      }
    }
  }

  digitalWrite(PINRS485, LOW);

  int int_numero = -1;

  if (bytesLeidos > 0) {
    int_numero = (byte)buffer[3] << 8 | (byte)buffer[4];
  }

  return int_numero;
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
  delay(500);

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
  //int raw[8] = {3051, 1732, 234, 598, 1021, 2419, 3955, 3539};
  int raw[8] = {2770, 1618, 226, 575, 963, 2224, 3388, 3145};

  int raws[4][8] = {
    {2770, 1618, 226, 575, 963, 2224, 3388, 3145},
    {2480, 1618, 234, 575, 790, 2224, 2930, 3145},
    {1410, 1618, 240, 575, 805, 2224, 2917, 3145},
    {3790, 1618, 125, 575, 352, 2224, 2917, 3145}};

  int tol = 40;
  int val = analogRead(pinWindDirection);

  if(val > 4090){
    return -90;
  }

  int value = -180;

  for(int i = 0; i < 3; i++){
    for(int j = 0; j < 8; j++){
      if(abs(val - raws[i][j]) <= tol ){
        value = dir[j];
        break;
      }
    }
  }


  //for(int i = 0; i < 8; i++){
  //  if(abs(val - raw[i]) <= tol){
  //    value = dir[i];
  //    return value;
  //  }
  //}

  return value;
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

/* ====================== SIM FUNCTIONS ======================== */
void modemPowerOn(){
  //rtc_gpio_init(PWR_PIN);
  //rtc_gpio_set_direction(PWR_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
  //rtc_gpio_hold_dis(PWR_PIN);

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

  //rtc_gpio_init(PWR_PIN);
  //rtc_gpio_set_direction(PWR_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
  //rtc_gpio_set_level(PWR_PIN, HIGH);  // Establecer en ALTO antes del deep sleep
  
  // Mantener el estado del pin durante el deep sleep
  //gpio_hold_en(PWR_PIN);
  //gpio_deep_sleep_hold_en();

  if (connectionType == "LTE") {
    modem.poweroff();
  }
  
}

void modemRestart(){
  modemPowerOff();
  delay(1000);
  modemPowerOn();
}

/* ====================== CALLBACK ======================== */
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("llegada de mensaje tpic ");
  Serial.println(topic);

  if(String(topic) == String(topicSubscribe) + "/" + macA()){
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

    boolean Status = docIn["status"];
    int Time = docIn["timesleep"];
    boolean a_TH = docIn["A_TH"];
    boolean a_WS = docIn["A_WS"];
    boolean a_RS = docIn["A_RD"];
    TIME_TO_SLEEP = Time;
    status = Status;

    A_TH = a_TH;
    A_WS = a_WS;
    A_RS = a_RS;

    saveSetting(String(status), "/status.txt");
    saveSetting(String(Time), "/timesleep.txt");
    saveSetting(String(A_TH), "/A_TH.txt");
    saveSetting(String(A_WS), "/A_WS.txt");
    saveSetting(String(A_RS), "/A_RD.txt");

    sendData();
  }
}

/* ====================== RECONNECT ======================== */
void setupMQTT() {
  if (connectionType == "WIFI") {
    wificlient.setServer(broker, mqtt_port);
    wificlient.setCallback(callback); // Reasignar el callback
  } else if (connectionType == "LTE") {
    simclient.setServer(broker, mqtt_port);
    simclient.setCallback(callback); // Reasignar el callback
  }
}

void wifiReconnect(){
  int count = 0;
  while (!wificlient.connected()) {
    String client_id = "esp32-client-";
    client_id += String(macA());
    if (wificlient.connect(client_id.c_str())) {
      String topiSub = String(topicSubscribe) + "/" + macA();
      wificlient.subscribe(topiSub.c_str());
      Serial.println(topiSub);
      wificlient.setCallback(callback);
    } else {
      Serial.print("failed with state ");
      Serial.print(wificlient.state());
      if (count == 5)
      {
        ESP.restart();
      }
      delay(5000);
    }
    count++;
  }
}

void simReconnect(){
  int count = 0;
  while (!simclient.connected()) {
    String client_id = "esp32-client-";
    client_id += String(macA());
    //Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (simclient.connect(client_id.c_str())) {
      String topiSub = String(topicSubscribe) + "/" + macA();
      simclient.subscribe(topiSub.c_str());
      Serial.println(topiSub);
      simclient.setCallback(callback);
    } else {
      Serial.print("failed with state ");
      Serial.print(simclient.state());
      if (count == 3)
      {
        ESP.restart();
      }
      delay(5000);
    }
    count++;
  }
}

bool checkMQTT() {
  if (connectionType == "WIFI") {
    return wificlient.connected();
  } else if (connectionType == "LTE") {
    return simclient.connected();
  }
  return false;
}

void getConfig() {
  StaticJsonDocument<50> docInit;
  docInit["type"] = "weatherStationSetting";
  docInit["function"] = "setting";
  docInit["mac"] = macA();

  String payloadInit = "";
  serializeJson(docInit, payloadInit);

  Serial.println(payloadInit);
  String topiSub = String(topicSubscribe) + "/" + macA();

  if (connectionType == "WIFI") {
    wifiReconnect();
    
    if (wificlient.subscribe(topiSub.c_str())) {
      Serial.println("Suscripción MQTT exitosa");
    } else {
      Serial.println("Fallo en suscripción MQTT");
    }
    wificlient.publish(topicPublish, payloadInit.c_str());

  } else if (connectionType == "LTE") {
    simReconnect();
    if (simclient.subscribe(topiSub.c_str())) {
      Serial.println("Suscripción MQTT exitosa");
    } else {
      Serial.println("Fallo en suscripción MQTT");
    }
    simclient.publish(topicPublish, payloadInit.c_str());
  }
}

/* ============================== SETUP GRPS ============================== */
bool setup_grps()
{
  WiFi.disconnect(true);  // Desconectar de la red actual
  WiFi.mode(WIFI_OFF);    // Establecer modo WiFi a OFF

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
    simclient.setServer(broker, mqtt_port);
    simclient.setCallback(callback);
    return true;
  }
}

/* ============================== SETUP WIFI ============================== */
void setup_wifi() {
  //Connect and send initial data to get the configuration
  RUNssid = readFile(SPIFFS, "/ssidString.txt");  // original ssid
  RUNpass = readFile(SPIFFS, "/passString.txt");  // original password

  int n = 0;
  WiFi.begin(RUNssid, RUNpass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("Connecting to WiFi..");
    if (n >= 5) {
      ESP.restart();
    }
    n++;
  }

  Serial.println("Connected to wifi");
  wificlient.setServer(broker, mqtt_port);
  wificlient.setCallback(callback);
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

      init_running();
      break;
  }
}

void sendData() {

  JsonDocument docOut;

  if (status) {
    docOut["type"] = "weatherStationData";
    if (A_TH) {
      dht.begin();
      float h = dht.readHumidity();
      float t = dht.readTemperature();

      int cont = 0;
      while (isnan(t) || isnan(h) || (h == 1.00 && t == 1.00)) {
        dht.begin();
        Serial.println("Failed to read from DHT");
        h = dht.readHumidity();
        t = dht.readTemperature();

        if (cont >= 10) {
          break;
        }
        cont++;
        delay(2000);
      }

      docOut["H"] = String(h, 2);
      docOut["T"] = String(t, 2);
    }

    if (A_WS) {
      docOut["WV"] = WindVelocity();
      docOut["WD"] = WindDirection();
      docOut["RA"] = readFile(SPIFFS, "/ssidRainCounter.txt").toInt();
    }

    if (A_RS) {


      int rad = radRS485();
      int counter = 0;
      while (rad < 0) {
        rad = radRS485();
        delay(1000);
        if (counter >= 5) {
          break;
        }
        counter++;
      }
      docOut["RS"] = rad;
    }

    docOut["mac"] = macA();
    docOut["vB"] = vBat();

    String jsonString;
    serializeJson(docOut, jsonString);

    Serial.println(jsonString);

    if (connectionType == "WIFI") {
      wifiReconnect();
      wificlient.publish(topicPublish, jsonString.c_str());
    } else if (connectionType == "LTE") {
      simReconnect();
      simclient.publish(topicPublish, jsonString.c_str());
    }
  }

  //delay(6000);
  //deepSleepSystem();
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

    connectionType = readFile(SPIFFS, "/connType.txt");

    if (connectionType == "") {
      connectionType = "WIFI";
      writeFile(SPIFFS, "/connType.txt", connectionType.c_str());
    }

    if (connectionType == "WIFI") {
      Serial.println("wifi mode ------>");
      modemPowerOff();
      setup_wifi();
      delay(1000); // Esperar para estabilizar conexión
      setupMQTT(); // Configurar MQTT
      delay(1000);
      getConfig();
    } else if (connectionType == "LTE") {
      Serial.println("lte mode ------>");
      setup_grps();
      delay(1000); // Esperar para estabilizar conexión
      setupMQTT(); // Configurar MQTT
      delay(1000);
      getConfig();
    }


    delay(500);

    //deepSleepSystem();

    /*Serial.print("sleep for min: ");
    Serial.println(TIME_TO_SLEEP);

    TIME_TO_SLEEP = readFile(SPIFFS, "/timesleep.txt").toInt(); 
    uint64_t SLEEPTIME = TIME_TO_SLEEP;
    //esp_deep_sleep_enable_gpio_wakeup(BIT(D5), ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_sleep_enable_timer_wakeup(SLEEPTIME * 60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();*/
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(BAUD_RATE, SERIAL_MODE, RX_PIN, TX_PIN);
  dht.begin();
  analogReadResolution(12);

  pinMode(DHTPIN, INPUT_PULLUP);
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LOW);

  pinMode(PINRS485,OUTPUT);
  digitalWrite(PINRS485,LOW);

  //while(true){
  //  Serial.println(WindDirection());
  //  delay(1000);
  //}
  
  //rtc_gpio_pullup_en(RAINCOUNT);          // wake up by rain counter
  esp_sleep_enable_ext1_wakeup(0x400000000,ESP_EXT1_WAKEUP_ALL_LOW); // Pin 34 to rain counter
  
  // INIT SPIFFS MODULE
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  wakeup_reason();
  
}

void loop() {
  static unsigned long lastCheck = 0;

  if (!checkMQTT()) {
    if (millis() - lastCheck > 5000) {
      if (connectionType == "WIFI") wifiReconnect();
      else if (connectionType == "LTE") simReconnect();
      lastCheck = millis();
    }
  }

  if (connectionType == "WIFI") {
    if (!wificlient.connected()) {
      wifiReconnect();
    }
    wificlient.loop();
    
    delay(50);
  } else if (connectionType == "LTE") {
    if (!simclient.connected()) {
      simReconnect();
    }
    simclient.loop();
    
    delay(50);
  }

  if ((millis() - previousTime) > 90000) {
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem();  // poweroff
  }
}


