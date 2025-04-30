
/* ========================== CAMERA CONFIG ============================*/
#include "StreamIO.h"
#include "VideoStream.h"
#include "MP4Recording.h"

#define CHANNEL 0

VideoSetting config(CHANNEL);
MP4Recording mp4;
StreamIO videoStreamer(1, 1);  // 1 Input Video -> 1 Output RTSP

/* ========================== DHT CONFIG ============================*/
#include "DHT.h"

#define pinBattery A0
#define DHTPIN 8
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

/* ========================== SD CONFIG ============================*/
#include "AmebaFatFS.h"
AmebaFatFS fs;

String filename = "data.json";
String filenameT = "time.txt";
String filenameC = "configCamDatalogger.json";

#include <ArduinoJson.h>

/* ========================== POWER CONFIG ============================*/
#include "PowerMode.h"

// wake up by AON timer :   0
// wake up by AON GPIO  :   1
// wake up by RTC       :   2
#define WAKEUP_SOURCE 2

/* ========================== variables CONFIG ============================*/
String DeviceName = "";

unsigned long timeoutStart = 0;
float timeCicle = 1;
float timeRecording = 60;
float tempTrigger = 18;

#define PIN_SET 12

/* ========================== RTC CONFIG ============================*/
#include <stdio.h>
#include <time.h>
#include "rtc.h"

long long seconds = 0;
struct tm *timeinfo;

/* ========================== READ sdCARD CONFIG ============================*/
void readConfigFromSD(){
  char buf[200];
  char path[256];

  fs.begin();
  sprintf(path, "%s%s", fs.getRootPath(), filenameC.c_str());
  File file = fs.open(path);

  memset(buf, 0, sizeof(buf));
  file.read(buf, sizeof(buf));
  file.close();
  fs.end();

  //{"name":"CAM1_","timeCicle_H":1,"timeRecorder_M",50}
  JsonDocument doc;
  deserializeJson(doc, buf);
  const char* NAME = doc["name"];
  float TIMECICLEH = doc["timeCicle_H"];
  float TIMERECORDERM = doc["timeRecorder_M"];
  float TEMPTRIGGER = doc["TEMPTRIGGER"];

  DeviceName = String(NAME);
  timeCicle = TIMECICLEH;
  timeRecording = TIMERECORDERM;
  tempTrigger = TEMPTRIGGER;

  Serial.println("read from sd --------------------------");
  Serial.println(buf);

}


/* ========================== SAVE DATA ============================*/
void saveData(int count) {
  dht.begin();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  for (uint8_t i = 0; i < 10; i++) {
    if (t > 1 && h > 1) {
      break;
    } else {
      h = dht.readHumidity();
      t = dht.readTemperature();
    }
    delay(500);
  }

  JsonDocument docOut;

  docOut["id"] = count;
  docOut["Time"] = (millis() - timeoutStart) * 0.001 + int((timeCicle * 3600) * (1 - timeRecording * 0.01));;
  docOut["Humidity"] = String(h, 2);
  docOut["Temperature"] = String(t, 2);

  String jsonString;
  serializeJson(docOut, jsonString);

  writeFile(filename, jsonString, true);
}

int readCounter()
{
  char buf[128];
  char path[128];
  sprintf(path, "%s%s", fs.getRootPath(), filenameT.c_str());
  fs.begin();
  File file = fs.open(path);
  memset(buf, 0, sizeof(buf));
  file.read(buf, sizeof(buf));
  file.close();
  return String(buf).toInt();
  fs.end();
}

void writeFile(String fileName, String payload, boolean newLine) {

  Serial.println("///////////////////////////////////");
  Serial.println(payload);

  char path[128];
  char buff[128];
  fs.begin();

  sprintf(path, "%s%s", fs.getRootPath(), fileName.c_str());
  File file = fs.open(path);

  if(newLine){
    sprintf(buff, "%s%s",payload.c_str(),",");
    file.seek(file.size()); 
    file.println(buff);
  }else{
    file.print(payload.c_str());
  }
  file.close();
  fs.end();
}

/* ========================== SETUP ============================*/
void setup() {
  timeoutStart = millis();
  Serial.begin(115200);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_G, HIGH);
  
  Serial.println("START ---------------------------------<>");
  readConfigFromSD();

  dht.begin();

  float t = dht.readTemperature();

  for (uint8_t i = 0; i < 10; i++) {
    if (t > 1) {
      break;
    } else {
      t = dht.readTemperature();
    }
    delay(500);
  }

  uint32_t ALARM_DAY = 0;
  uint32_t ALARM_HOUR = 0;
  uint32_t ALARM_MIN = 29;
  if(timeCicle >= 1){
    ALARM_HOUR = int(timeCicle);
  }else{
    ALARM_MIN = int(timeCicle * 60);
  }
  
  uint32_t ALARM_SEC = 0;

  int count = readCounter();

  Serial.println("+++++++++++++++++++++++ t");
  Serial.println(t,2);

  if( t > tempTrigger ){
    Serial.println(timeCicle);
    Serial.println(timeRecording);

    int tRecording = timeCicle * 3600.00 * timeRecording * 0.01;

    Serial.println("tRecording+++++++++++++++++++++++++++");
    Serial.println(tRecording);

    Camera.configVideoChannel(CHANNEL, config);
    Camera.videoInit();

    mp4.configVideo(config);
    
    mp4.setRecordingDuration(tRecording);
    mp4.setRecordingFileCount(1);

    String videoFile = DeviceName + String(count + 1);
    mp4.setRecordingFileName(videoFile);
    mp4.setRecordingDataType(STORAGE_VIDEO);  // Set MP4 to record video only

    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(mp4);
    if (videoStreamer.begin() != 0) {
      Serial.println("StreamIO link start failed");
    }

    // Start data stream from video channel
    Camera.channelBegin(CHANNEL);
    // Start recording MP4 data to SD card
    mp4.begin();

    delay(1000);

    delay(tRecording * 1000);

    ALARM_MIN = int((timeCicle * 60) * (1 - timeRecording * 0.01));
  }

  saveData(count);

  uint32_t PM_rtc_Alarm[4] = {ALARM_DAY, ALARM_HOUR, ALARM_MIN, ALARM_SEC};
  #define WAKUPE_SETTING (uint32_t)(PM_rtc_Alarm)

  count++;

  writeFile(filenameT,String(count),false);

  digitalWrite(LED_G, LOW);

  PowerMode.begin(DEEPSLEEP_MODE, WAKEUP_SOURCE, WAKUPE_SETTING);
  Serial.println("********* START SLEEP *********");
  Serial.println(ALARM_MIN);
  PowerMode.start();
  //PowerMode.start(YEAR, MONTH, DAY, HOUR, MIN, SEC);
}

void loop() {
  // do nothing
}
