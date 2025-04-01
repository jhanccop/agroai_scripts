#include <HardwareSerial.h> // Reference the ESP32 built-in serial port library
HardwareSerial lidarSerial(2); // Using serial port 2
#define RXD2 16
#define TXD2 17

const int VENTANA_MUESTRAS = 10;    // Tamaño de la ventana para promediar
const float UMBRAL_DELTA = 0.02;    // Umbral para detectar cambios de dirección (ajustar según señal)

// Variables para el procesamiento
float muestras[VENTANA_MUESTRAS];   // Buffer circular de muestras
int indice_muestra = 0;             // Índice actual en el buffer
float ultimo_promedio = 0;          // Último valor promedio calculado
float tendencia_anterior = 0;       // Dirección anterior de la señal

// Variables para el cálculo del periodo
unsigned long tiempo_ultimo_pico = 0;
unsigned long periodo_actual = 0;
bool primer_pico = true;
int valorActual;

int distance(){
  if (lidarSerial.available() > 0) {
    uint8_t buf[9] = {0};
    lidarSerial.readBytes(buf, 9); // Read 9 bytes of data
    if( buf[0] == 0x59 && buf[1] == 0x59)
    {
      valorActual = buf[2] + buf[3] * 256;
    }
  }
  return valorActual;
}

void setup() {
  Serial.begin(115200);
  // Primera lectura para inicializar
  lidarSerial.begin(115200, SERIAL_8N1, RXD2, TXD2); // Initializing serial port
  //valorAnterior = distance();

  for(int i = 0; i < VENTANA_MUESTRAS; i++) {
    muestras[i] = 0;
  }
}

void loop() {
  // Lectura del sensor
  valorActual = distance();

  Serial.println(valorActual);

  muestras[indice_muestra] = valorActual;
  indice_muestra = (indice_muestra + 1) % VENTANA_MUESTRAS;
  
  // 2. Calcular promedio móvil
  float suma = 0;
  for(int i = 0; i < VENTANA_MUESTRAS; i++) {
    suma += muestras[i];
  }
  float promedio_actual = suma / VENTANA_MUESTRAS;
  
  // 3. Calcular tendencia actual
  float tendencia_actual = promedio_actual - ultimo_promedio;
  
  // 4. Detectar picos y valles
  if (abs(tendencia_actual) > UMBRAL_DELTA) {  // Asegurarse que el cambio es significativo
    // Detectar pico (cambio de tendencia positiva a negativa)
    if (tendencia_anterior > 0 && tendencia_actual < 0) {
      if (!primer_pico) {
        // Calcular periodo entre picos
        unsigned long tiempo_actual = millis();
        periodo_actual = tiempo_actual - tiempo_ultimo_pico;
        tiempo_ultimo_pico = tiempo_actual;
        
        Serial.print("Pico detectado! Periodo: ");
        Serial.print(periodo_actual);
        Serial.println(" ms");
      } else {
        tiempo_ultimo_pico = millis();
        primer_pico = false;
      }
    }
    // Detectar valle (cambio de tendencia negativa a positiva)
    else if (tendencia_anterior < 0 && tendencia_actual > 0) {
      Serial.println("Valle detectado!");
    }
    
    tendencia_anterior = tendencia_actual;
  }
  
  ultimo_promedio = promedio_actual;
  
  // Pequeño delay para estabilidad
  delay(10);
}