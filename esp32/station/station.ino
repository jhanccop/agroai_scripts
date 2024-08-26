#include <driver/rtc_io.h>

#include <esp_now.h>
#include <WiFi.h>

/* ================= DEVICE SETTINGS ================= */
#define pinControl GPIO_NUM_25 // gateway control PIN 25

uint64_t uS_TO_S_FACTOR = 60000000ULL;
uint64_t TIME_TO_SLEEP = 25; // time deep sleep - minutes
RTC_DATA_ATTR int bootCount = 0; // count

const String device="station2";
uint8_t broadcastAddress[] = {0xD4, 0x8A, 0xFC, 0xA5, 0x7A, 0x58};// MASTER MAC ADDRESS
//{0xDC, 0x4F, 0x22, 0x58, 0xD2, 0xF5} //station0

uint64_t processTime = 30; // process seconds RPI

// Structure example to send data must match the receiver structure
typedef struct struct_message {
  String dev;
  String mes;
  uint64_t sleepTime;
} struct_message;

struct_message dataSent;
struct_message dataRcv;
unsigned long previousTime=0;

void deepSleepSystem(){
  // de-energize gateway
  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);

  /* =============== deep sleep start =============== */
  Serial.print("sleep for min ");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush(); 
  esp_deep_sleep_start();
}

// Active RPI
void RPI(){
  rtc_gpio_init(pinControl);
  rtc_gpio_set_direction(pinControl, RTC_GPIO_MODE_OUTPUT_ONLY);

  rtc_gpio_hold_dis(pinControl);
  rtc_gpio_set_level(pinControl, HIGH);

  delay(20000);

  rtc_gpio_set_level(pinControl, LOW);
  rtc_gpio_hold_en(pinControl);
}


// callbacks for sending and receiving data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\n"+device+" packet sent:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void sendStartMessage(){
  dataSent.dev = device;
  dataSent.mes = "started";
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &dataSent, sizeof(dataSent));
}

void sendEndMessage(){
  dataSent.dev = device;
  dataSent.mes = "finished";
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &dataSent, sizeof(dataSent));
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

  if(dataRcv.mes == "start"){
    delay(500);
    TIME_TO_SLEEP = dataRcv.sleepTime;
    sendStartMessage();

    RPI();

    deepSleepSystem();
  }
}
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      Serial.println(F("Error initializing ESP-NOW"));
      return;
  }
  Serial.print(F("Reciever initialized : "));
  //Serial.println(WiFi.macAddress());
  
  // Define callback functions
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
  // Register peer
  /*
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println(F("Failed to add peer"));
      return;
  }
  */
}
void loop() {
  if((millis() -previousTime)>1500){
      // strcpy(dataSent.a, nom);
      dataSent.dev = device;
      dataSent.mes = "station";
  
      // Send message via ESP-NOW
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &dataSent, sizeof(dataSent));
  
      previousTime=millis();
  }
}
