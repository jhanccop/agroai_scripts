#include <driver/rtc_io.h>

#include <esp_now.h>
#include <WiFi.h>

/* ================= DEVICE SETTINGS ================= */
#define pinControl GPIO_NUM_25 // gateway control PIN 25

uint64_t uS_TO_S_FACTOR = 60000000ULL;
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count

const String device = "gateway";

int countDevices = 0;
int countResponses = 0;

// STATION MAC ADDRESS
uint8_t broadcastAddress[2][6] = {
  {0xE8, 0x6B, 0xEA, 0xF7, 0x0B, 0xD4}, //station0
  {0xD4, 0x8A, 0xFC, 0xC8, 0xD2, 0xB0}  //station1
};

// Structure example to send data must match the receiver structure
typedef struct struct_message {
  String dev;
  String mes;
  uint64_t sleepTime;
} struct_message;

struct_message myData;
struct_message dataRcv;

unsigned long previousTime=0;

// denergize and deep sleep

void deepSleepSystem(){
  // de-energize gateway
  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);

  /* =============== deep sleep start =============== */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush();
  Serial.println("Deep sleep start");
  Serial.println(TIME_TO_SLEEP);
  esp_deep_sleep_start();
}

// callbacks for sending and receiving data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(F("\r\nMaestro packet sent:\t"));
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&dataRcv, incomingData, sizeof(dataRcv));
  Serial.print("\r\nBytes received: ");
  Serial.println(len);
  Serial.print("device: ");
  Serial.println(dataRcv.dev);
  Serial.print("message: ");
  Serial.println(dataRcv.mes);
  Serial.println();

  if(dataRcv.mes == "station"){
    countDevices++;
  }
  
  if(dataRcv.mes == "finished"){
    countResponses++;
  }

  if(countResponses == countDevices){
    delay(1000);
    deepSleepSystem();
  }
}

void sendStartMessage(){
  //strcpy(myData.a, nom);
  myData.dev = device;
  myData.mes = "start";
  myData.sleepTime = 45;

  for(int i = 0; i < 2; i++){
    esp_err_t result0 = esp_now_send(broadcastAddress[i], (uint8_t *) &myData, sizeof(myData));
  }
  
}

void setup() {

  /* 0. initial settings */
  Serial.begin(115200);
  rtc_gpio_init(pinControl);
  rtc_gpio_set_direction(pinControl, RTC_GPIO_MODE_OUTPUT_ONLY);

  rtc_gpio_hold_dis(pinControl);
  rtc_gpio_set_level(pinControl, HIGH);

  // Set pin control gateway 
  //pinMode(pinControl, OUTPUT);
  //digitalWrite(pinControl, false);
  delay(1000);

  // digitalWrite(pinControl, true);
  
  // Set device as a Wi-Fi Station ESP32
  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      Serial.println(F("Error initializing ESP-NOW"));
      return;
  }
  Serial.print(F("Reciever initilized : "));
  Serial.println(device);
  
  // Define callback functions
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
      
  memcpy(peerInfo.peer_addr, broadcastAddress[0], 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
  }
  memcpy(peerInfo.peer_addr, broadcastAddress[1], 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
  }

  /* 1. send start message */
  sendStartMessage();
  
}
void loop() {
  if((millis() -previousTime)>45000){
     deepSleepSystem();
     previousTime=millis();
  }
}
