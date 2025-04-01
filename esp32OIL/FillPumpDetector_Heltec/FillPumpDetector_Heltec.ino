#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "HX711.h"
#include "heltec.h"

#include <HardwareSerial.h>

#include "HT_SSD1306Wire.h"

/* =============== CONFIG SENSORS =============== */
HardwareSerial lidarSerial(1);
Adafruit_MPU6050 mpu;

unsigned char buf1[] = {0x5A,0x05,0x00,0x01,0x60};

const uint8_t SCL_ACC = 42;
const uint8_t SDA_ACC = 41;

const int LOADCELL_DOUT_PIN = 47;
const int LOADCELL_SCK_PIN = 48;
float alpha = 0.5;

float initRead = 145;
float currRead = 145;

int16_t rawLoad[500] = {};
int16_t filLoad[500] = {};

int16_t distance[500] = {};

HX711 scale;

#define INT_PIN 0 // Pin movement detector
#define PIN_CONFIG 46 // Pin config

/* =============== CONFIG OLED =============== */
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

/* =============== CONFIG LORA =============== */
#include "LoRaWan_APP.h"

/* OTAA para*/
uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x03 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 };

/* ABP para*/
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda, 0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef, 0x67 };
uint32_t devAddr = (uint32_t)0x007e6ae1;

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6] = { 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 300000;

/*OTAA or ABP*/
bool overTheAirActivation = true;  //OTAA security is better

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 2;

uint8_t confirmedNbTrials = 4;  //


/* ========================= GET DATA ========================= */
void getData() {
  boolean flag = false;

  uint16_t count = 0;
  while (true) {
    if (scale.is_ready()) {
      int16_t rawL = scale.get_value(5) * 0.01;
      rawLoad[count] = rawL;


      //currRead = (alpha * rawRead) + ((1 - alpha) * currRead);
      //Serial.println(String(currRead));
    } else {
      Serial.println("HX711 not found.");
    }
    delay(15);
  }
}

uint16_t getDistance() {
  //lidarSerial.write(buf1,5);
  //lidarSerial.flush();
  Serial.println("---------");
  int valorActual;

  while (1) {
    if (lidarSerial.available() > 0) {
      byte d = lidarSerial.read();
      uint8_t buf[9] = { 0 };
      lidarSerial.readBytes(buf, 9);  // Read 9 bytes of data
      if (buf[0] == 0x59 && buf[1] == 0x59) {
        valorActual = buf[2] + buf[3] * 256;
        break;
      }
    }
  }

  return valorActual;
}

/* ========================= PROCCESSING DATA ========================= */
void proccessData(){

  float Xi = 0;

  for(int i = 0; i < 500;i++){
    //currRead = (alpha * rawRead) + ((1 - alpha) * currRead);
  }
}

/* ========================= PROCCESSING DATA ========================= */
void proccessRun(){

}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

/* ========================= INIT RUN ========================= */
void initRun() {

  VextON();

  display.init();
  display.setFont(ArialMT_Plain_10);

  String SdevEui = "";
  for (int i = 0; i < 8; i++) {
    SdevEui += String(devEui[i], HEX);
    SdevEui += " ";
  }

  String SappSKey1 = "";
  String SappSKey2 = "";
  for (int i = 0; i < 8; i++) {
    SappSKey1 += String(appSKey[i], HEX);
    SappSKey1 += " ";
    SappSKey2 += String(appSKey[8 + i], HEX);
    SappSKey2 += " ";
  }

  while (digitalRead(PIN_CONFIG)) {

    uint16_t DIST= getDistance();

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
  
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(5, 0, String(SdevEui));
    display.drawString(5, 10, String(SappSKey1));
    display.drawString(5, 20, String(SappSKey2));
    display.drawString(5, 34, "DISTANCE : "); display.drawString(75, 34, String(DIST));
    display.drawString(5, 44, "LOAD 1000: "); display.drawString(75, 44, String(scale.get_value(5) * 0.01,0));
    display.drawString(5, 54, "ACCELERAT: "); display.drawString(75, 54, String(a.acceleration.x));
    display.display();
    delay(20);
  }
  ESP.restart();
}

/* ========================= PREPARE TX FRAME ========================= */
static void prepareTxFrame(uint8_t port) {
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
  *appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
  *if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
  *if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
  *for example, if use REGION_CN470, 
  *the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */
  appDataSize = 0;
  //appData[appDataSize++] = 0x04;

  /* status */

  float spm = 25.9;
  float fillPump = 34.6;
  float sLength = 85.52;
  float vBat = 4.55;
  uint8_t status = 0; /* 1: running, 0: stopped */

  int16_t spmInt = (int16_t)(spm * 10);
  appData[appDataSize++] = (spmInt >> 8) & 0xFF;  // byte alto
  appData[appDataSize++] = spmInt & 0xFF;         // byte bajo

  int16_t fillPumpInt = (int16_t)(fillPump * 10);
  appData[appDataSize++] = (fillPumpInt >> 8) & 0xFF;  // byte alto
  appData[appDataSize++] = fillPumpInt & 0xFF;         // byte bajo

  int16_t sLengthInt = (int16_t)(sLength * 10);
  appData[appDataSize++] = (sLengthInt >> 8) & 0xFF;  // byte alto
  appData[appDataSize++] = sLengthInt & 0xFF;         // byte bajo

  int16_t vBatInt = (int16_t)(vBat * 10);
  appData[appDataSize++] = (vBatInt >> 8) & 0xFF;  // byte alto
  appData[appDataSize++] = vBatInt & 0xFF;         // byte bajo

  // Humedad en 1 byte
  appData[appDataSize++] = status;
  Serial.println(appDataSize);
}

/* ========================= MAIN SETUP ========================= */
void setup() {
  Serial.begin(115200);
  lidarSerial.begin(115200, SERIAL_8N1, 19, 20);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  pinMode(PIN_CONFIG,INPUT_PULLUP);

  Wire1.begin(SDA_ACC, SCL_ACC);

  if (!mpu.begin(0x68, &Wire1)) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("MPU6050 Found!");


  if(digitalRead(PIN_CONFIG)){
    Serial.println("===== Setting mode >>>");
    initRun();


  }else{
    Serial.println("===== Running mode >>>");
  }

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  deviceState = DEVICE_STATE_INIT;
}

/* ========================= MAIN LOOP ========================= */
void loop() {
  switch (deviceState) {
    case DEVICE_STATE_INIT:
      {
#if (LORAWAN_DEVEUI_AUTO)
        LoRaWAN.generateDeveuiByChipID();
#endif
        LoRaWAN.init(loraWanClass, loraWanRegion);
        break;
      }
    case DEVICE_STATE_JOIN:
      {
        LoRaWAN.join();
        break;
      }
    case DEVICE_STATE_SEND:
      {
        prepareTxFrame(appPort);
        LoRaWAN.send();
        deviceState = DEVICE_STATE_CYCLE;
        break;
      }
    case DEVICE_STATE_CYCLE:
      {
        // Schedule next packet transmission
        txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
        break;
      }
    case DEVICE_STATE_SLEEP:
    {
      if(loraWanClass==CLASS_A)
      {
#ifdef WIRELESS_MINI_SHELL
        esp_deep_sleep_enable_gpio_wakeup(1<<INT_PIN,ESP_GPIO_WAKEUP_GPIO_LOW);
#else
        esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_PIN,0);
#endif
      }
      LoRaWAN.sleep(loraWanClass);
      break;
    }
    default:
      {
        deviceState = DEVICE_STATE_INIT;
        break;
      }
  }
}

// ========================================================================
/*
float filtrarMedicion(float nuevaMedicion) {
  static float medicionesPrevias[5] = {0};
  static int indice = 0;
  
  // Guardar nueva medición
  medicionesPrevias[indice] = nuevaMedicion;
  indice = (indice + 1) % 5;
  
  // Calcular promedio
  float suma = 0;
  for(int i = 0; i < 5; i++) {
    suma += medicionesPrevias[i];
  }
  
  return suma / 5;
}

Adafruit_MPU6050 mpu;

void setup(void) {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit MPU6050 test!");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  //setupt motion detection
  mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
  mpu.setMotionDetectionThreshold(1);
  mpu.setMotionDetectionDuration(100);
  mpu.setInterruptPinLatch(true);	// Keep it latched.  Will turn off when reinitialized.
  mpu.setInterruptPinPolarity(true);
  mpu.setMotionInterrupt(true);

  Serial.println("");
  delay(100);
}

void loop() {

  if(mpu.getMotionInterruptStatus()) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    Serial.print("AccelX:");
    Serial.print(a.acceleration.x);
    Serial.print(",");
    Serial.print("AccelY:");
    Serial.print(a.acceleration.y);
    Serial.print(",");
    Serial.print("AccelZ:");
    Serial.print(a.acceleration.z);
    Serial.print(", ");
    Serial.print("GyroX:");
    Serial.print(g.gyro.x);
    Serial.print(",");
    Serial.print("GyroY:");
    Serial.print(g.gyro.y);
    Serial.print(",");
    Serial.print("GyroZ:");
    Serial.print(g.gyro.z);
    Serial.println("");
  }

  delay(10);
}
*/