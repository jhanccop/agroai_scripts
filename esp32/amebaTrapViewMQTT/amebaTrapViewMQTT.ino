/*AMEBA PRODUCTION*/
#define ARDUINOJSON_STRING_LENGTH_SIZE 4

#include "PowerMode.h"

#include "StreamIO.h"
#include "VideoStream.h"
#include <HttpClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "Base64.h"
#include <ArduinoJson.h>
#include "DHT.h"
#include "AmebaFatFS.h"

/* ======== DEEP SLEEP SETTINGS ======== */
/* DEEP SLEEP MODE WAKE UP SETTINGS */
// wake up by AON timer :   0
// wake up by AON GPIO  :   1
// wake up by RTC       :   2
#define WAKEUP_SOURCE 2

/* ======== WIFI SETTINGS ======== */
String RUNssid = "";
String RUNpass = "";
int status = WL_IDLE_STATUS;

char sHostname_buffer[50];
String sHostname = "";
String SearchPath = "/devices/api/buscar/?mac=";
String csrfPath = "/get-csrf/";
const int kNetworkTimeout = 30 * 1000;
const int kNetworkDelay = 1000;

WiFiClient wifiClient;

int port = 8000;
String csrfToken = "";

/* ======== MQTT SETTINGS ======== */
#include <PubSubClient.h>
char mqttServer[] = "24.199.125.52"; //24.199.125.52  broker.hivemq.com
String clientId = "amb";
char imageTopic[] = "jhpOandG/data/trapViewImage";
char publishPayload[] = "hello world";
char subscribeTopic[] = "inTopic";
//#define MQTT_MAX_PACKET_SIZE 50000

PubSubClient client(wifiClient);
const int chunkSize = 8192;

/* ======== SDCARD SETTINGS ======== */
char filename[] = "config.txt";
AmebaFatFS fs;

/* ======== CAMERA SETTINGS ======== */

#define CHANNEL       0
int encodedLen;
char *encodedData;

CameraSetting configCam;
VideoSetting config(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);

uint32_t img_addr = 0;
uint32_t img_len = 0;

/* ======== DEVICE SETTINGS ======== */
String DeviceName = "";
String DeviceMacAddress = "";
boolean A_TH = false;
uint32_t SleepTime = 60;
boolean runningNN = false;
boolean statusDevice = false;
String resolution = "VIDEO_HD";

#define pinBattery A0
#define DHTPIN 8
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)(payload[i]));
    }
    Serial.println();
}

void reconnect()
{
    // Loop until we're reconnected
    while (!(client.connected())) {
        Serial.print("\r\nAttempting MQTT connection...");
        // Attempt to connect
        clientId += macAddr();
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement and resubscribe
            //client.subscribe(subscribeTopic);
        } else {
            Serial.println("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

String macAddr(){
  byte mac[6];
  String result = "";
  WiFi.macAddress(mac);
  result =  String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
  Serial.println(result);
  return result;
}

/* =========================== GET SETTINGS =============================*/
void getHttp() {
  int err0 = 0;
  int err1 = 0;
  int err2 = 0;

  WiFiClient c;
  HttpClient http(c);

  String mac = macAddr();

  SearchPath = SearchPath + mac;
  Serial.println(sHostname_buffer);
  Serial.println(SearchPath);
  Serial.println(port);

  err0 = http.get(sHostname_buffer, port, SearchPath.c_str());

  if (err0 == 0) {
    Serial.println("startedRequest ok");

    err1 = http.responseStatusCode();
    if (err1 >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err1);

      err2 = http.skipResponseHeaders();
      Serial.print("err updated");
      Serial.println(err2);

      if (err2 >= 0) {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows:");

        unsigned long timeoutStart = millis();
        char c;

        String msg_in = "";
        while ((http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout)) {
          if (http.available()) {
            c = http.read();
            msg_in += String(c);
            bodyLen--;
            timeoutStart = millis();
          } else {
            delay(kNetworkDelay);
          }
        }

        Serial.println(msg_in);

        JsonDocument docIn;
        DeserializationError error = deserializeJson(docIn, msg_in);
        if (error) {
          return;
        }
        const char* deviceName = docIn["DeviceName"];
        const char* deviceMacAddress = docIn["DeviceMacAddress"];
        boolean a_TH = docIn["A_TH"];
        boolean RunningNN = docIn["runningNN"];
        boolean Status = docIn["status"];
        int sleepTime = docIn["SleepTime"];
        const char* Resolution = docIn["Resolution"];

        DeviceName = String(deviceName);
        DeviceMacAddress = String(deviceMacAddress);
        A_TH = a_TH;
        SleepTime = sleepTime;
        runningNN = RunningNN;
        statusDevice = Status;
        resolution = String(Resolution);

        Serial.println(DeviceName);
        Serial.println(DeviceMacAddress);
        Serial.println(A_TH);
        Serial.println(SleepTime);
        Serial.println(runningNN);
        Serial.println(statusDevice);
        Serial.println(resolution);

      } else {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err2);
      }
    } else {
      Serial.print("Getting response failed: ");
      Serial.println(err1);
    }
  } else {
    Serial.print("Connect failed: ");
    Serial.println(err0);
  }

  if(err0 != 0 || err1 < 0 || err2 < 0 ){
    Serial.println("********************* fail http RESTART**************");
    sys_reset();
  }

  http.stop();
  
}

/* =========================== ENCODE IMAGE =============================*/
void encodejpg()
{
    encodedLen = base64_enc_len(img_len);
    encodedData = (char *)malloc(encodedLen);
    base64_encode(encodedData, (char *)img_addr, img_len);
}

String vBat(){
  long raw = map(analogRead(pinBattery),204,453,201,368);
  //long raw = analogRead(pinBattery);

  return String(raw*0.01,2);
}

/* =========================== POST DATA =============================*/
void sendData() {
  
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_G, HIGH);

  JsonDocument docOut;

  docOut["img64"] = "";
  
  if (statusDevice == 1) {

    Serial.println("ADQUIRE IMAGE");

    if(resolution = "1"){
      Serial.println("VIDEO FHD IMAGE**************");
      

    }else{
      Serial.println("VIDEO HD IMAGE----------------");
      
    }

    //configCam.setExposureTime(3);
    //configCam.se
    //configCam.;
    configCam.setSharpness(25);
    configCam.setLDC(1);

    Camera.configVideoChannel(CHANNEL, config);
    Camera.videoInit();
    Camera.channelBegin(CHANNEL);

    delay(2500);

    Camera.getImage(CHANNEL, &img_addr, &img_len);
    encodejpg();

    Camera.channelEnd(CHANNEL);

    docOut["img64"] = encodedData;
  }

  if(A_TH){
    dht.begin();
    delay(2000);
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.println("get Temperature an humity");
    Serial.println(h);
    Serial.println(t);
    
    int cont = 0;
    while (isnan(t) || isnan(h) || (h == 1.00 && t == 1.00)) 
    {
      digitalWrite(LED_B, HIGH);
      dht.begin();
      Serial.println("Failed to read from DHT");
      h = dht.readHumidity();
      t = dht.readTemperature();
      
      if(cont >= 10){
        break;
      }
      cont++;
      digitalWrite(LED_B, LOW);
      delay(2000);
    }

    docOut["Humidity"] = String(h,2);
    docOut["Temperature"] = String(t,2);
    digitalWrite(LED_B, LOW);
  }

  docOut["DeviceMacAddress"] = macAddr();
  docOut["VoltageBattery"] = vBat();

  String jsonString;
  serializeJson(docOut, jsonString);

  Serial.println("-------------------");

  int sizeString = jsonString.length();
  int Nparts = (sizeString / chunkSize) + (sizeString % chunkSize != 0);

  Serial.println(String(sizeString));
  Serial.println(String(Nparts));

  reconnect();

  for (int i = 0; i < Nparts; i++) {
    char fragmento[chunkSize + 1];
    strncpy(fragmento, jsonString.c_str() + (i * chunkSize), chunkSize);
    fragmento[chunkSize] = '\0'; 

    char topicFragmentado[50];
    sprintf(topicFragmentado, "%s/%s/%d/%d",imageTopic,macAddr().c_str(),Nparts,i);

    client.publish(topicFragmentado,fragmento);
    delay(100);  // Pequeña pausa entre envíos
  }

  //client.publish(publishTopic, jsonString.c_str());

  digitalWrite(LED_G, LOW);

  
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

/* =========================== WIFI STATUS =============================*/
void readConfigFromSD(){
  char buf[200];
  char path[128];

  fs.begin();
  sprintf(path, "%s%s", fs.getRootPath(), filename);
  File file = fs.open(path);

  memset(buf, 0, sizeof(buf));
  file.read(buf, sizeof(buf));
  file.close();

  JsonDocument doc;
  deserializeJson(doc, buf);
  const char* SSID = doc["ssid"];
  const char* PASS = doc["pass"];
  const char* Serverlocal = doc["serverlocal"];
  const char* Server = doc["server"];
  const char* Servermode = doc["servermode"];
  int PORT = doc["port"];
  int LOCALPORT = doc["portlocal"];

  if(String(Servermode) == "0"){ // "0" local server
    sHostname = String(Serverlocal);
    sHostname.toCharArray(sHostname_buffer, sizeof(sHostname_buffer));
    port = LOCALPORT;
  }else{
    sHostname = String(Server);
    sHostname.toCharArray(sHostname_buffer, sizeof(sHostname_buffer));
    port = PORT;
  }


  Serial.println("read from sd --------------------------");
  Serial.println(buf);

  RUNssid = String(SSID);
  RUNpass = String(PASS);

}

void setup() {
  Serial.begin(115200);
  dht.begin();
  readConfigFromSD();

  int n = 0;
  
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(RUNssid);

    char ssid_buffer[50];
    RUNssid.toCharArray(ssid_buffer, sizeof(ssid_buffer));

    char pass_buffer[50];
    RUNpass.toCharArray(pass_buffer, sizeof(pass_buffer));

    status = WiFi.begin(ssid_buffer, pass_buffer);
    delay(2000);

    if(n >= 5){
      Serial.println("*********************RESTART**************");
      sys_reset();
    }
    n++;
  }

  Serial.println("Connected to wifi");
  printWifiStatus();

  getHttp();

  wifiClient.setNonBlockingMode();

  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  if (!(client.connected())) {
    reconnect();
  }
  client.loop();

  //getcsrfHttp();

  sendData();

  delay(2500);

  wifiClient.stop();

  uint32_t ALARM_DAY = 0;
  uint32_t ALARM_HOUR = 0;
  uint32_t ALARM_MIN = 0;
  uint32_t ALARM_SEC = 0;

  switch(SleepTime){
    case 2:
      ALARM_MIN = 2;
      break;
    case 5:
      ALARM_MIN = 5;
      break;
    case 30:
      ALARM_MIN = 30;
      break;
    case 60:
      ALARM_HOUR = 1;
      break;
    case 120:
      ALARM_HOUR = 2;
      break;
    default:
      ALARM_MIN = 60;
      break;
  }

  uint32_t PM_rtc_Alarm[4] = {ALARM_DAY, ALARM_HOUR, ALARM_MIN, ALARM_SEC};
  #define WAKUPE_SETTING (uint32_t)(PM_rtc_Alarm)

  PowerMode.begin(DEEPSLEEP_MODE, WAKEUP_SOURCE, WAKUPE_SETTING);

  PowerMode.start();
}

void loop() {
  
}


