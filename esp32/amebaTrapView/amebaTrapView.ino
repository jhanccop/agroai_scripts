
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

#define CHANNEL       0

/* DEEP SLEEP MODE WAKE UP SETTINGS */
// wake up by AON timer :   0
// wake up by AON GPIO  :   1
// wake up by RTC       :   2
#define WAKEUP_SOURCE 2

/* CONNECT SETTINGS */
char ssid[] = "ConectaLineal2500";  // your network SSID (name)
char pass[] = "19CERA@DER27@14";    // your network password (use for WPA, or use as key for WEP)

String RUNssid = "";
String RUNpass = "";

int keyIndex = 0;                   // your network key Index number (needed only for WEP)
int port = 8000;

char filename[] = "config.txt";

AmebaFatFS fs;

const char kHostname[] = "192.168.3.31";  //192.168.3.31:8000/devices/api/items/1/ 24.199.125.52

char sHostname_buffer[50];
String sHostname = "";

String SearchPath = "/devices/api/buscar/?mac=";
String csrfPath = "/get-csrf/";
const int kNetworkTimeout = 30 * 1000;
const int kNetworkDelay = 1000;
int status = WL_IDLE_STATUS;

int encodedLen;
char *encodedData;

VideoSetting config(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);

WiFiClient wifiClient;

uint32_t img_addr = 0;
uint32_t img_len = 0;

/* device settings */
String DeviceName = "";
String DeviceMacAddress = "";
boolean A_TH = false;
uint32_t SleepTime = 60;
boolean runningNN = false;
boolean statusDevice = false;
String resolution = "VIDEO_HD";

String csrfToken = "";

#define pinBattery A0
#define DHTPIN 8
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

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
  int err = 0;

  WiFiClient c;
  HttpClient http(c);

  String mac = macAddr();

  SearchPath = SearchPath + mac;
  Serial.println(sHostname_buffer);
  Serial.println(SearchPath);
  Serial.println(port);

  err = http.get(sHostname_buffer, port, SearchPath.c_str());
  if (err == 0) {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err);

      err = http.skipResponseHeaders();
      Serial.print("err updated");
      Serial.println(err);

      if (err >= 0) {
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
        Serial.println(err);
      }
    } else {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  } else {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
  
}

void getcsrfHttp() {
  int err = 0;

  WiFiClient c;
  HttpClient http(c);

  err = http.get(sHostname_buffer, port, csrfPath.c_str());
  if (err == 0) {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err);

      err = http.skipResponseHeaders();
      Serial.print("err updated");
      Serial.println(err);

      if (err >= 0) {
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
        const char* CsrfToken = docIn["csrfToken"];
        csrfToken = String(CsrfToken);

      } else {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    } else {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  } else {
    Serial.print("Connect failed: ");
    Serial.println(err);
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
void postHttp() {
  
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_G, HIGH);

  dht.begin();
  JsonDocument docOut;

  docOut["img64"] = "";
  
  if (statusDevice == 1) {

    Serial.println("ADQUIRE IMAGE");
    //VideoSetting config(VIDEO_HD, CAM_FPS, VIDEO_JPEG, 1);

    if(resolution = "1"){
      Serial.println("VIDEO FHD IMAGE");
      VideoSetting config(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);
    }else{
      Serial.println("VIDEO HD IMAGE");
    }

    Camera.configVideoChannel(CHANNEL, config);
    Camera.videoInit();
    Camera.channelBegin(CHANNEL);

    delay(2000);

    Camera.getImage(CHANNEL, &img_addr, &img_len);
    encodejpg();

    docOut["img64"] = encodedData;
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  int cont = 0;

  while (isnan(t) || isnan(h) || (h == 1.00 && t == 1.00)) 
  {
    Serial.println("Failed to read from DHT");
    h = dht.readHumidity();
    t = dht.readTemperature();
    delay(500);
    if(cont >= 5){
      break;
    }
    cont++;
  }
 
  docOut["DeviceMacAddress"] = macAddr();
  docOut["Humidity"] = String(h,2);
  docOut["Temperature"] = String(t,2);
  docOut["VoltageBattery"] = vBat();

  String jsonString;
  serializeJson(docOut, jsonString);

  /*
  if (wifiClient.connect(sHostname_buffer, port)) {
    wifiClient.setTimeout(20000);
    wifiClient.println("POST /data/api/post HTTP/1.1");
    wifiClient.println("Host: " + String(sHostname_buffer));
    wifiClient.println("Content-Type: application/json");                  // Use appropriate content type

    wifiClient.println("X-CSRFToken: " + csrfToken);
    wifiClient.println("Cookie: csrftoken=" + csrfToken);                    
    wifiClient.println("Content-Length: " + String(jsonString.length()));  // Specify the length of the content

    wifiClient.println("Connection: keep-alive");
    //wifiClient.print("Connection: close");
    wifiClient.println();          // Empty line indicates the end of headers
    wifiClient.print(jsonString);  // Send the Base64 encoded audio data directly
    Serial.println("Binary sent");
  }
  */

  if (wifiClient.connect(sHostname_buffer, port)) {
    wifiClient.setTimeout(30000);

    String request = "POST /data/api/post HTTP/1.1\r\n";
    request += "Host: " + String(sHostname_buffer) + " \r\n";
    request += "Content-Type: application/json\r\n";
    request += "X-CSRFToken: " + csrfToken +  " \r\n";
    request += "Cookie: csrftoken=" + csrfToken + " \r\n";
    request += "Content-Length: " + String(jsonString.length()) + "\r\n";
    request += "Connection: keep-alive\r\n";
    request += "\r\n";
    request += jsonString;
    wifiClient.println(request);

  }

  digitalWrite(LED_G, LOW);
  wifiClient.stop();
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
    delay(5000);

    if(n >= 5){
      sys_reset();
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


