uint8_t Com[8] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09 };
float tem, hum, ph;
int ec;


/* ====================== RS485 SETTINGS ======================== */
#define RX_PIN 16  // Pin RX del ESP32
#define TX_PIN 17  // Pin TX del ESP32
#define BAUD_RATE 9600
#define SERIAL_MODE SERIAL_8N1
#define MAX_BUFFER 64
uint8_t buffer[MAX_BUFFER];


/* ====================== READ LEVEL RS485 ======================== */
int radRS485() {
  Serial2.begin(4800, SERIAL_MODE, RX_PIN, TX_PIN);

  uint8_t command[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
  size_t len = sizeof(command);
  Serial2.write(command, len);
  Serial2.flush(); 

  int bytesLeidos = 0;
  unsigned long tiempoInicio = millis();
  
  while (millis() - tiempoInicio < 1000) {  // Timeout de 1 segundo
    if (Serial2.available()) {
      buffer[bytesLeidos] = Serial2.read();
      bytesLeidos++;
      
      if (bytesLeidos >= MAX_BUFFER) {
        break;
      }
    }
  }

  int int_numero = -1;

  if (bytesLeidos > 0) {
    int_numero = (byte)buffer[3] << 8 | (byte)buffer[4];
  }

  return int_numero;
}


void setup() {
  Serial.begin(115200);
  //Serial2.begin(BAUD_RATE, SERIAL_MODE, RX_PIN, TX_PIN);
}
void loop() {
  readHumitureECPH();
  Serial2.end();
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("************* SOIL ************");
  Serial.print("T = ");
  Serial.print(tem, 1);
  Serial.println("°C  ");
  Serial.print("H = ");
  Serial.print(hum, 1);
  Serial.println(" %");
  Serial.print("EC = ");
  Serial.print(ec, 1);
  Serial.println(" us/cm");
  Serial.print("PH = ");
  Serial.println(ph, 1);
  delay(50);
  Serial.println("*********** RADIATION **********");
  int rad = radRS485();
  Serial2.end();
  Serial.print("Rad = ");
  Serial.println(rad);
  delay(5000);
}
void readHumitureECPH(void) {

  Serial2.begin(BAUD_RATE, SERIAL_MODE, RX_PIN, TX_PIN);
  uint8_t Data[13] = { 0 };
  uint8_t ch = 0;
  bool flag = 1;
  while (flag) {
    delay(100);
    Serial2.write(Com, 8);
    delay(10);
    if (readN(&ch, 1) == 1) {
      if (ch == 0x01) {
        Data[0] = ch;
        if (readN(&ch, 1) == 1) {
          if (ch == 0x03) {
            Data[1] = ch;
            if (readN(&ch, 1) == 1) {
              if (ch == 0x08) {
                Data[2] = ch;
                if (readN(&Data[3], 10) == 10) {
                  if (CRC16_2(Data, 11) == (Data[11] * 256 + Data[12])) {
                    hum = (Data[3] * 256 + Data[4]) / 10.00;
                    tem = (Data[5] * 256 + Data[6]) / 10.00;
                    ec = Data[7] * 256 + Data[8];
                    ph = (Data[9] * 256 + Data[10]) / 10.00;
                    flag = 0;
                  }
                }
              }
            }
          }
        }
      }
    }
    Serial2.flush();
  }
}

uint8_t readN(uint8_t *buf, size_t len) {
  size_t offset = 0, left = len;
  int16_t Tineout = 500;
  uint8_t *buffer = buf;
  long curr = millis();
  while (left) {
    if (Serial2.available()) {
      buffer[offset] = Serial2.read();
      offset++;
      left--;
    }
    if (millis() - curr > Tineout) {
      break;
    }
  }
  return offset;
}

unsigned int CRC16_2(unsigned char *buf, int len) {
  unsigned int crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (unsigned int)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  crc = ((crc & 0x00ff) << 8) | ((crc & 0xff00) >> 8);
  return crc;
}