#include <driver/rtc_io.h>

#include <WiFi.h>
#include <esp_wifi.h>

#include <ArduinoJson.h>

#include <DHT.h>;
#define DHTPIN 12
#define DHTPOWER 32
#define DHTTYPE DHT21 //DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

/* ================= DEVICE SETTINGS ================= */
#define pinControl GPIO_NUM_25 // RPI control PIN 25
#define pinBattery 39 // input voltage

uint64_t uS_TO_S_FACTOR = 1000000UL; // factor to time sleep
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count

unsigned long previousTime = 0;
boolean completed = false;

/* ========================= FUNCTIONS ========================= */
void deepSleepSystem( ){
  // de-energize gateway
  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);

  // deep sleep start 
  Serial.print("sleep for min ");
  uint64_t SLEEPTIME = TIME_TO_SLEEP * 60 ;
  esp_sleep_enable_timer_wakeup(SLEEPTIME * uS_TO_S_FACTOR);
  //Serial.flush(); 
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

void infoESP32(){
  JsonDocument doc;
  doc["mac"] = readMacAddress();
  doc["voltage"] = map(analogRead(pinBattery),1142,1271,508,582); //5.08 v - 5.82 v
  doc["T"] = String(dht.readTemperature(),2);
  doc["H"] = String(dht.readHumidity(),2);

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
  
  enableRPI();
  //disableRPI();

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();
  
  pinMode(DHTPOWER,OUTPUT);
  digitalWrite(DHTPOWER,HIGH);
  dht.begin(); // sensor
  
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
    TIME_TO_SLEEP = doc["timesleep"];

    if(String(msg) == "info"){
      infoESP32();
      digitalWrite(DHTPOWER,LOW);
    }else if(String(msg) == "completed"){
      Serial.println("Start down count");
      completed = true;
      previousTime = millis(); // init count for 10 seconds
      //disableRPI();
    }

  }

  if((millis() - previousTime) > 15000 && completed){
    Serial.println("power off rpi");
    disableRPI(); // disable rpi after 10 seconds
    deepSleepSystem();
  }

  if((millis() - previousTime) > 210000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}
