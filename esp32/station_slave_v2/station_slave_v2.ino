#include <driver/rtc_io.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>

#include <ArduinoJson.h>

#include <DHT.h>;

// WiFi
const char *ssid = "xxxxx"; // Enter your Wi-Fi name
const char *password = "xxxxx";  // Enter Wi-Fi password

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic_pub = "emqx/esp32";
const char *topic_sub = "emqx/esp32";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// DHT21
#define DHTPIN 12
#define DHTPOWER 32
#define LED 2
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

  digitalWrite(LED, LOW);
  // de-energize gateway
  rtc_gpio_set_level(pinControl, HIGH);
  rtc_gpio_hold_en(pinControl);

  // deep sleep start 
  Serial.print("sleep for min ");
  Serial.print(TIME_TO_SLEEP);
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
  delay(1000);
  rtc_gpio_set_level(pinControl, LOW);
  delay(1000);
  rtc_gpio_set_level(pinControl, HIGH);

}

void disableRPI(){
  rtc_gpio_init(pinControl);
  rtc_gpio_set_direction(pinControl, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_hold_dis(pinControl);

  rtc_gpio_set_level(pinControl, HIGH);
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

/* ========================= SETUP WIFI ========================= */
void setup_wifi(){
  pinMode(DHTPOWER,OUTPUT);
  digitalWrite(DHTPOWER,HIGH);
  dht.begin(); // sensor

  while(true){
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
  }
}

/* ========================= RUN FUNCTION DEEP SLEEP ========================= */
void deepSleepTask(){
  pinMode(DHTPOWER,OUTPUT);
  digitalWrite(DHTPOWER,HIGH);
  dht.begin(); // sensor

  while(true){

  }
}

/* ========================= WAKE UP REASON ========================= */
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
      Serial.println("Wakeup caused by external signal using RTC_IO");

      break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER :
      Serial.println("Wakeup caused by timer");

      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD :
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP :
      Serial.println("Wakeup caused by ULP program");
      break;
    default :
      Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason);
      break;
  }
}

/* ========================= SETUP ========================= */
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  Serial2.begin(115200,SERIAL_8N1,16,17);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  pinMode(DHTPOWER,OUTPUT);
  digitalWrite(DHTPOWER,HIGH);
  dht.begin(); // sensor

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  print_wakeup_reason();
  
  enableRPI();
  //disableRPI();

  
    
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
      disableRPI(); // disable rpi after 10 seconds
      deepSleepSystem();
    }
  }

  if((millis() - previousTime) > 210000){
    Serial.println(millis() - previousTime);
    Serial.println("sleep for overtime");
    deepSleepSystem(); // poweroff
  }
}
