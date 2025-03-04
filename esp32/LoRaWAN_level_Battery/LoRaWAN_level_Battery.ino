/* Heltec Automation LoRaWAN communication example
 *
 * Function:
 * 1. Upload Heltec_ESP32 node data to the server using the standard LoRaWAN protocol.
 *  
 * Description:
 * 1. Communicate using LoRaWAN protocol.
 * 
 * HelTec AutoMation, Chengdu, China
 * 成都惠利特自动化科技有限公司
 * www.heltec.org
 *
 * this project also realess in GitHub:
 * https://github.com/HelTecAutomation/Heltec_ESP32
 
 * the development environment of this project in Arduion:
 * https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series
 * */

#include "LoRaWan_APP.h"

const int PWM_INPUT_PIN = 7;
const float SOUND_SPEED_0C = 331.3;

volatile unsigned long pwmHighTime = 0;
volatile unsigned long pwmLowTime = 0;
volatile unsigned long lastRiseTime = 0;
volatile unsigned long pulseWidth = 0;
volatile bool measurementComplete = false;

float distancia = 0;
const float MAX_DISTANCE = 750.0; // Distancia máxima en cm

uint8_t devEui[] = { 0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x03 };
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 };

/* ABP para*/
uint8_t nwkSKey[] = { 0x15, 0xb1, 0xd0, 0xef, 0xa4, 0x63, 0xdf, 0xbe, 0x3d, 0x11, 0x18, 0x1e, 0x1e, 0xc7, 0xda,0x85 };
uint8_t appSKey[] = { 0xd7, 0x2c, 0x78, 0x75, 0x8c, 0xdc, 0xca, 0xbf, 0x55, 0xee, 0x4a, 0x77, 0x8d, 0x16, 0xef,0x67 };
uint32_t devAddr =  ( uint32_t )0x007e6ae1;

/*LoraWan channelsmask, default channels 0-7*/ 
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 60000;

/*OTAA or ABP*/
bool overTheAirActivation = true;//OTAA security is better

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 2;
/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 4;//

/* Prepares the payload of the frame */
static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
  *appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
  *if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
  *if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
  *for example, if use REGION_CN470, 
  *the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */

  if (measurementComplete) {
    // Calcular ciclo de trabajo (duty cycle)
    unsigned long totalPeriod = pulseWidth + pwmLowTime;
    float dutyCycle = (float)pulseWidth / totalPeriod * 100.0;
    
    // Convertir PWM a distancia
    // Asumiendo que el duty cycle va de 0-100% para 0-400cm
    distancia = (dutyCycle / 100.0) * MAX_DISTANCE;
    
    // Mostrar resultados
    Serial.print("Ancho de pulso (us): ");
    Serial.print(pulseWidth);
    Serial.print(" | Duty Cycle (%): ");
    Serial.print(dutyCycle);
    Serial.print(" | Distancia (cm): ");
    Serial.println(distancia);
    
    // Indicador visual de lectura
    digitalWrite(LED, !digitalRead(LED));
    
    measurementComplete = false;
  }

  unsigned char *puc;
  appDataSize = 0;
  appData[appDataSize++] = 0x04;
  appData[appDataSize++] = 0x00;
  appData[appDataSize++] = 0x0A;

/* The upper four digits represent the subID, and the lower four digits represent significant decimal places */
  appData[appDataSize++] =0X02;
  
  //print out the values you read:
  puc = (unsigned char *)(&distancia);
  appData[appDataSize++] = puc[0];
  appData[appDataSize++] = puc[1];
  appData[appDataSize++] = puc[2];
  appData[appDataSize++] = puc[3];

  // read the analog / millivolts value for pin 1:
  int analogValue = analogRead(1);
  int analogVolts = analogReadMilliVolts(1);
  Serial.printf("ADC analog value = %d\n",analogValue);
  Serial.printf("ADC millivolts value = %d\n",analogVolts);

/*Sensor parent ID and sensor length */
  appData[appDataSize++] = 0x00;
  appData[appDataSize++] = 0x00;
  appData[appDataSize++] = 0x02;

/*The higher 4 digits represent the subID, and the lower 4 digit represents the data type */
 appData[appDataSize++] = (uint8_t)0X05;

  //print out the values you read:
  puc = (unsigned char *)(&analogValue);
  appData[appDataSize++] = (uint8_t)0X05;
  appData[appDataSize++] =((uint8_t)(int)analogValue  / 3.7);
  
}

void IRAM_ATTR pwmIInterrupt() {
  // Lectura del estado actual del pin
  int pinState = digitalRead(PWM_INPUT_PIN);
  
  unsigned long currentTime = micros();
  
  if (pinState == HIGH) {
    // Flanco ascendente
    lastRiseTime = currentTime;
    if (pwmLowTime != 0) {
      pwmLowTime = currentTime - pwmLowTime;
    }
  } else {
    // Flanco descendente
    if (lastRiseTime != 0) {
      pulseWidth = currentTime - lastRiseTime;
      measurementComplete = true;
    }
    pwmLowTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(PWM_INPUT_PIN, INPUT);
  
  // Configurar interrupción
  attachInterrupt(digitalPinToInterrupt(PWM_INPUT_PIN), pwmIInterrupt, CHANGE);  

  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
  deviceState = DEVICE_STATE_INIT;
  }

void loop()
{
 switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
      LoRaWAN.init(loraWanClass,loraWanRegion);
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      prepareTxFrame( appPort );
      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
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