#include <driver/rtc_io.h>

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <ArduinoJson.h>

#include <DHT.h>;
#define DHTPIN 23
#define DHTPOWER 19
#define DHTTYPE DHT21 //DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

/* ================= DEVICE SETTINGS ================= */
#define pinControl GPIO_NUM_25 // RPI control PIN 25
#define pinBattery 39 // input voltage

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count

const String device="station2";
uint8_t broadcastAddress[] = {0xD4, 0x8A, 0xFC, 0xA5, 0x7A, 0x58};// MASTER MAC ADDRESS
//{0xDC, 0x4F, 0x22, 0x58, 0xD2, 0xF5} //station0

//uint64_t rpiTime = 60; // process seconds RPI

// Structure example to send data must match the receiver structure
typedef struct struct_message {
  String dev;
  String mes;
  uint64_t sleepTime;
} struct_message;

struct_message dataSent;
struct_message dataRcv;
unsigned long previousTime = 0;
boolean completed = false;

/* ========================= FUNCTIONS ========================= */

void deepSleepSystem(){
  // de-energize gateway
  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);

  // deep sleep start 
  Serial.print("sleep for min ");
  uint64_t SLEEPTIME = (TIME_TO_SLEEP * 60) - 5 ;
  esp_sleep_enable_timer_wakeup(SLEEPTIME * uS_TO_S_FACTOR);
  Serial.flush(); 
  esp_deep_sleep_start();
}

// enable RPI
void enableRPI(){
  rtc_gpio_init(pinControl);
  rtc_gpio_set_direction(pinControl, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(pinControl);
  rtc_gpio_set_level(pinControl, HIGH);
}

void disableRPI(){
  rtc_gpio_init(pinControl);
  rtc_gpio_set_direction(pinControl, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(pinControl);

  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);
}

// callbacks for sending and receiving data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\n"+device+" packet sent:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void sendEspNow(String msg){
  dataSent.dev = device;
  dataSent.mes = msg;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &dataSent, sizeof(dataSent));
}

String readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);

  char buffer[20];
  if (ret == ESP_OK) {
    sprintf(buffer,"%02x:%02x:%02x:%02x:%02x:%02x",baseMac[0], baseMac[1], baseMac[2],baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
  return String(buffer);
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&dataRcv, incomingData, sizeof(dataRcv));
  Serial.print("\r\nBytes received: ");
  Serial.println(len);
  Serial.print("device: ");
  Serial.println(dataRcv.dev);
  Serial.print("mes: ");
  Serial.println(dataRcv.mes);
  Serial.println();

  if(dataRcv.mes == "wakeup"){
    TIME_TO_SLEEP = dataRcv.sleepTime;
    sendEspNow("started"); // respuesta
    enableRPI();
  }else if(dataRcv.mes == "sleep"){
    deepSleepSystem();
  }
}

void infoESP32(){
  JsonDocument doc;
  doc["mac"] =  readMacAddress();
  doc["voltage"] = map(analogRead(pinBattery),1142,1271,508,582); //5.08 v - 5.82 v
  doc["T"] = dht.readTemperature();
  doc["H"] = dht.readHumidity();

  //serializeJson(doc, Serial);
  char output[256];
  serializeJson(doc, output);
  Serial2.write(output);
  Serial2.write("\n");
  Serial.println(output);
}

/* ========================= SETUP ========================= */
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  Serial2.begin(115200,SERIAL_8N1,16,17);
  //enableRPI();
  disableRPI();

  dht.begin(); // sensor1

  //pinMode(25,OUTPUT);
  //digitalWrite(25,HIGH);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      Serial.println(F("Error initializing ESP-NOW"));
      return;
  }
  Serial.print(F("Reciever initialized : "));
  
  // Define callback functions
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
}

/* ========================= LOOP ========================= */
void loop() {

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

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    const char* msg = doc["msg"];

    if(String(msg) == "info"){
      infoESP32();
    }else if(msg == "completed"){
      sendEspNow("completed");
      completed = true;

      previousTime = millis(); // init count for 10 seconds

      //disableRPI();
      
    }

  }

  if((millis() - previousTime) > 10000 && completed){
    Serial.println("power off rpi");
    disableRPI(); // disable rpi after 10 seconds
  }

  if((millis() - previousTime) > 180000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff after 2 minutes
  }
}
