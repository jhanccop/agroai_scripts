#include "Arduino.h"
#include "esp_camera.h"
#include <base64.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#include "camera_pins.h"

// AI Thinker Model - LED Driving Pins
#define ESP32CAM_LED_INBUILT 33
#define ESP32CAM_LED_FLASH 4

// SIM800
#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon

const char apn[] = "claro.pe"; 
const char gprsUser[] = "claro";
const char gprsPass[] = "claro";





// Replace the next variables with your SSID/Password combination
const char *ssid = "iPhone de Joel";
const char *password = "20080076c";

// Add your MQTT Broker IP address, example:
// const char* mqtt_server = "192.168.1.144";
//const char* mqtt_server = "broker.emqx.io";
//const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_server = "24.199.125.52";
// const char *mqtt_server = "io.adafruit.com";

const char *mqtt_clientid = "mqtt_clientid";
const char *mqtt_username = "mqtt_username"; // Adafruit Username
const char *mqtt_password = "mqtt_password"; // Adafruit AIO Key
const char *mqtt_publish_topic = "jhp/atad/camera";
// const char *mqtt_subscribe_topic = "username/feeds/sensor";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
// char msg[50];
// int value = 0;

#define uS_TO_S_FACTOR 1000000UL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1800        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

void callback(char *topic, byte *message, unsigned int length);
void reconnect();
void setup_wifi();

void setup()
{
  Serial.begin(115200);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  // default settings
  // (you can also pass in a Wire library object like &Wire2)

  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("MYCO CAMERA V1");
  Serial.println();

  // FLASH LED
  pinMode(ESP32CAM_LED_FLASH, OUTPUT);
  pinMode(ESP32CAM_LED_INBUILT, OUTPUT);
  digitalWrite(ESP32CAM_LED_INBUILT, LOW);

  // buffer.reserve(32000);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // was at 20
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_quality(s,4);
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientid, mqtt_username, mqtt_password))
    {
      Serial.println("connected");
      // Subscribe Topic
      // client.subscribe(mqtt_subscribe_topic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 30000)
  {

    // Capture picture
    //digitalWrite(ESP32CAM_LED_INBUILT, HIGH);
    //digitalWrite(ESP32CAM_LED_FLASH, LOW);
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();

    if (!fb)
    {
      Serial.println("Camera capture failed");
      return;
    }
    else
    {
      Serial.println("Camera Captured");
    }
    //digitalWrite(ESP32CAM_LED_INBUILT, LOW);
    //digitalWrite(ESP32CAM_LED_FLASH, HIGH);
    // delay(1000);

    // size_t size = fb->len;
    String buffer = base64::encode((uint8_t *)fb->buf, fb->len);
    // String buffer = base64::encode(fb->buf, fb->len);
    // Serial.println(buffer);

    unsigned int length();
    Serial.println("Buffer Length: ");
    Serial.print(buffer.length());
    Serial.println("");

    if (buffer.length() > 102400)
    {
      Serial.println("Image size too big");
      return;
    }

    Serial.print("Publishing...");
    if (client.publish(mqtt_publish_topic, buffer.c_str()))
    {
      Serial.print("Published");

      delay(1000);

      Serial.flush();
      Serial.println("start deep sleep");
      esp_deep_sleep_start();
    }
    else
    {
      Serial.println("Error");
    }
    Serial.println("");

    

    lastMsg = now;
  }

  
}