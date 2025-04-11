#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "Viscosimeter Dashboard";
const char* password = "12345678";
WebServer server(80);

/* ================== HX711 ======================= */
#include "HX711.h"
#define calibration_factor 100.0  //This value is obtained using the SparkFun_HX711_Calibration sketch
#define LOADCELL_DOUT_PIN 4
#define LOADCELL_SCK_PIN 5
HX711 scale;

#define SENSOR_PIN 34
#define SERVO_PIN 13
#define selectOnOff 19    // selectOnOff
#define selectHighLow 18  // selectHighLow
#define MAX_DATAPOINTS 6

bool isMonitoring = false;
bool isContinuous = false;
unsigned long startTime = 0;
unsigned long lastSampleTime = 0;
const int sampleInterval = 1000;    //1000
const int monitorDuration = 30000;  // 50000
int measurementCount = 0;
boolean RPMCHanged = true;

int rpmValues[MAX_DATAPOINTS] = { 600, 6, 200, 300, 3, 100 };
Servo myServo;

struct DataPoint {
  String label;
  unsigned long timestamp;
  float value;
};

DataPoint dataHistory[MAX_DATAPOINTS];
int dataCount = 0;
float currentValue = 0;
float lastValue = 0;

float readSensor() {
  //int rawValue = analogRead(SENSOR_PIN);
  setActuatorsForRPM(rpmValues[dataCount]);
  if (scale.is_ready()) {
    int rawLoad = scale.get_units(10);  //scale.read_average(5);
    //Serial.println(rawLoad);
    //return map(rawLoad, 0, 4095, 0, 50);
    return rawLoad;
  }
}

/* ================== SERVO ======================= */
void posServo(int pos) {
  switch (pos) {
    case 0:
      {
        myServo.write(105);
        break;
      }
    case 1:
      {
        myServo.write(85);
        break;
      }
    case 2:
      {
        myServo.write(55);
        break;
      }
    default:
      delay(100);
  }
}


void setActuatorsForRPM(int rpm) {
  if (RPMCHanged) {
    RPMCHanged = false;
    digitalWrite(selectOnOff, LOW);
    if (rpm == 600) {
      digitalWrite(selectHighLow, LOW);
      delay(1000);
      posServo(0);
    } else if (rpm == 6) {
      digitalWrite(selectHighLow, LOW);
      delay(1000);
      posServo(1);
    } else if (rpm == 200) {
      digitalWrite(selectHighLow, LOW);
      delay(1000);
      posServo(2);
    } else if (rpm == 300) {
      digitalWrite(selectHighLow, HIGH);
      delay(1000);
      posServo(0);
    } else if (rpm == 3) {
      digitalWrite(selectHighLow, HIGH);
      delay(1000);
      posServo(1);
    } else if (rpm == 100) {
      digitalWrite(selectHighLow, HIGH);
      delay(1000);
      posServo(2);
    }
  }
}

void addDataPoint(float value) {
  RPMCHanged = true;
  if (dataCount < MAX_DATAPOINTS) {
    dataHistory[dataCount].label = String(rpmValues[dataCount]) + " RPM";
    dataHistory[dataCount].timestamp = (millis() - startTime) / 1000;
    dataHistory[dataCount].value = value;

    //setActuatorsForRPM(rpmValues[dataCount]);

    dataCount++;
    measurementCount++;

    digitalWrite(selectOnOff, HIGH);
  }
}

void resetMeasurements() {
  dataCount = 0;
  measurementCount = 0;
  isMonitoring = false;
  isContinuous = false;
  digitalWrite(selectOnOff, HIGH);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>\n";
  html += "<meta charset='utf-8' name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "<title>Viscosimeter Dashboard</title>\n";
  html += "<style>\n";
  html += "* {box-sizing: border-box;}\n";
  html += "html, body {height: 100%; margin: 0; padding: 0; font-family: Arial, sans-serif; background: #121212; color: #e0e0e0;}\n";
  html += ".container {height: 100%; width: 100%; padding: 20px; overflow-y: auto; display: flex; flex-wrap: wrap;}\n";
  html += ".column {flex: 1; padding: 10px; min-width: 300px;}\n";
  html += "h1, h2 {color: #e0e0e0;}\n";
  html += "h1 {text-align: center; margin-top: 0; width: 100%;}\n";
  html += ".card {background: #1e1e1e; border-radius: 8px; padding: 15px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.3);}\n";
  html += "table {width: 100%; border-collapse: collapse; margin-bottom: 20px;}\n";
  html += "table, th, td {border: 1px solid #333;}\n";
  html += "th, td {padding: 10px; text-align: center;}\n";
  html += "th {background: #333; color: #e0e0e0;}\n";
  html += "canvas {width: 100%; height: 300px; margin-bottom: 20px; background: #232323;}\n";
  html += "button {background: #4CAF50; border: none; color: white; padding: 12px 24px; text-align: center; font-size: 16px; margin: 10px 5px; cursor: pointer; border-radius: 5px;}\n";
  html += "button:hover {background: #45a049;}\n";
  html += "button:disabled {background: #555; cursor: not-allowed;}\n";
  html += ".status {text-align: center; margin: 10px 0; font-weight: bold; color: #4CAF50;}\n";
  html += ".real-time-value {font-size: 20px; text-align: center; margin: 20px 0; color: #4CAF50;}\n";
  html += ".measurement-counter {font-size: 18px; text-align: center; margin: 10px 0; color: #e0e0e0;}\n";
  html += ".progress-bar {width: 100%; background-color: #333; border-radius: 5px; margin-bottom: 20px;}\n";
  html += ".progress {height: 20px; background-color: #4CAF50; border-radius: 5px; width: 0%;}\n";
  html += "#stopBtn {background: #f44336;}\n";
  html += "#stopBtn:hover {background: #d32f2f;}\n";
  html += "#resetBtn {background: #ff9800;}\n";
  html += "#resetBtn:hover {background: #fb8c00;}\n";
  html += "#continuousBtn {background: #2196F3;}\n";
  html += "#continuousBtn:hover {background: #1976D2;}\n";
  html += ".info-box {font-size: 18px; text-align: center; margin: 10px 0; color: #2196F3;}\n";
  html += ".buttons-container {display: flex; flex-wrap: wrap; justify-content: center;}\n";
  html += "</style>\n";
  html += "</head><body>\n";
  html += "<div class='container'>\n";
  html += "<div class='column'>\n";
  html += "<div class='card'>\n";
  html += "<div class='real-time-value'>Value: <span id='currentValue'>--</span> TO Current RPM: <span id='currentRPM'>--</span></div>\n";
  html += "<div class='status'>Status: <span id='statusText'>\n";
  html += isMonitoring ? (isContinuous ? "Continuous mode active" : "Reading... " + String((monitorDuration - (millis() - startTime)) / 1000) + "s remaining")
                       : "Waiting for start\n";
  html += "</span></div>\n";
  html += "<div class='measurement-counter'>Measurements completed: <span id='measureCount'>" + String(measurementCount) + "</span>/6</div>\n";
  html += "<div class='progress-bar'><div class='progress' id='progressBar'></div></div>\n";
  html += "<canvas id='realTimeChart'></canvas>\n";
  html += "</div>\n";

  html += "<div class='card'>\n";
  html += "<h4>Measurement Table</h4>\n";
  html += "<table id='dataTable'><thead><tr><th>RPM</th><th>Time (s)</th><th>Value</th></tr></thead><tbody>\n";

  for (int i = 0; i < dataCount; i++) {
    html += "<tr><td>" + dataHistory[i].label + "</td><td>" + String(dataHistory[i].timestamp) + "</td><td>" + String(dataHistory[i].value, 1) + "</td></tr>\n";
  }

  html += "</tbody></table>\n";
  html += "</div>\n";
  html += "</div>\n";

  html += "<div class='column'>\n";
  html += "<div class='card'>\n";
  html += "<h4>Plots of measurements</h4>\n";
  html += "<canvas id='historyChart'></canvas>\n";
  html += "<div id='pendienteInfo' class='info-box'></div>\n";
  html += "</div>\n";

  html += "<div class='card'>\n";
  html += "<h4>Results</h4>\n";
  html += "<div id='resultados' class='info-box'>";
  float pendiente = 0;
  float corteY = 0;
  bool puntosExisten = false;

  // Calcular diferencia entre 600 y 300
  float diff600_300 = 0;
  float diff100_200 = 0;
  float media6 = 0;

  for (int i = 0; i < dataCount; i++) {
    if (rpmValues[i] == 600) {
      for (int j = 0; j < dataCount; j++) {
        if (rpmValues[j] == 300) {
          pendiente = (dataHistory[j].value - dataHistory[i].value) / (300 - 600);
          corteY = dataHistory[i].value - pendiente * 600;
          puntosExisten = true;
          diff600_300 = abs(dataHistory[i].value - dataHistory[j].value);
          // Buscar valores para 100 y 200
          for (int k = 0; k < dataCount; k++) {
            if (rpmValues[k] == 100) {
              for (int l = 0; l < dataCount; l++) {
                if (rpmValues[l] == 200) {
                  diff100_200 = abs(dataHistory[k].value - dataHistory[l].value);
                }
              }
            }
          }
          // Calcular media a 6 por 2654
          float sum = 0;
          int count = 0;
          for (int m = 0; m < dataCount; m++) {
            sum += dataHistory[m].value;
            count++;
          }
          media6 = (sum / count) * 2654;

          break;
        }
      }
      if (puntosExisten) break;
    }
  }

  if (puntosExisten) {
    html += "<strong>Pendiente de la recta entre 600 y 300 RPM:</strong> " + String(pendiente, 5) + "<br>";
    html += "<strong>Point of intersection with the Y axis:</strong> " + String(corteY, 2) + "<br>";
    html += "<strong>μp = θ(600) -  θ(300): </strong> " + String(diff600_300, 2) + "<br>";
    html += "<strong>Diferencia entre 100 y 200 RPM:</strong> " + String(diff100_200, 2) + "<br>";
    html += "<strong>Media a 6 por 2654:</strong> " + String(media6, 2) + "<br>";
    //html += "<strong>Ecuación de la recta:</strong> y = " + String(pendiente, 5) + "x + " + String(corteY, 2);
  } else {
    html += "The 600 RPM and 300 RPM points are needed to calculate results.";
  }
  html += "</div>";
  html += "</div>\n";

  html += "<div class='card'>\n";
  html += "<h4>Control</h4>\n";
  html += "<div class='buttons-container'>\n";
  bool disableStartBtn = isMonitoring || measurementCount >= MAX_DATAPOINTS;
  html += "<button id='startBtn' " + String(disableStartBtn ? "disabled" : "") + ">Reading</button>\n";
  html += "<button id='stopBtn' " + String(!isMonitoring ? "disabled" : "") + ">Stop</button>\n";
  html += "<button id='resetBtn'>Restart</button>\n";
  html += "<button id='continuousBtn' " + String((isMonitoring || measurementCount >= MAX_DATAPOINTS) ? "disabled" : "") + ">Continuous reading</button>\n";
  html += "</div>\n";

  if (measurementCount >= MAX_DATAPOINTS) {
    html += "<p style='color: #ff9800; text-align: center;'>All 6 measurements have been completed. Use the 'Reset' button to start over.</p>\n";
  }

  html += "</div>\n";
  html += "</div>\n";

  html += "</div>\n";

  html += "<script>\n";

  html += "function drawRealTimeChart(canvas, data) {\n";
  html += "  if (!canvas.getContext) return;\n";
  html += "  const ctx = canvas.getContext('2d');\n";
  html += "  const width = canvas.width;\n";
  html += "  const height = canvas.height;\n";
  html += "  ctx.clearRect(0, 0, width, height);\n";
  html += "  if (!data || data.length === 0) return;\n";

  html += "  let min = 1000, max = 0;\n";
  html += "  for (let i = 0; i < data.length; i++) {\n";
  html += "    min = Math.min(min, data[i].value);\n";
  html += "    max = Math.max(max, data[i].value);\n";
  html += "  }\n";
  html += "  min = Math.max(0, min - 5);\n";
  html += "  max = Math.min(100, max + 5);\n";

  html += "  ctx.strokeStyle = '#555';\n";
  html += "  ctx.lineWidth = 1;\n";
  html += "  ctx.beginPath();\n";
  html += "  ctx.moveTo(30, 10);\n";
  html += "  ctx.lineTo(30, height - 30);\n";
  html += "  ctx.lineTo(width - 10, height - 30);\n";
  html += "  ctx.stroke();\n";

  html += "  ctx.strokeStyle = '#4CAF50';\n";
  html += "  ctx.lineWidth = 2;\n";
  html += "  ctx.beginPath();\n";

  html += "  for (let i = 0; i < data.length; i++) {\n";
  html += "    const x = 30 + (i / (data.length - 1)) * (width - 40);\n";
  html += "    const y = height - 30 - ((data[i].value - min) / (max - min)) * (height - 40);\n";
  html += "    if (i === 0) ctx.moveTo(x, y);\n";
  html += "    else ctx.lineTo(x, y);\n";
  html += "  }\n";
  html += "  ctx.stroke();\n";

  html += "  ctx.fillStyle = '#e0e0e0';\n";
  html += "  ctx.font = '10px Arial';\n";
  html += "  ctx.textAlign = 'right';\n";
  html += "  ctx.fillText('0', 25, height - 30);\n";
  html += "  ctx.fillText('50', 25, height - 30 - (height - 40) / 2);\n";
  html += "  ctx.fillText('100', 25, 15);\n";
  html += "}\n";

  html += "function drawHistoryChart(canvas) {\n";
  html += "  if (!canvas.getContext) return;\n";
  html += "  const ctx = canvas.getContext('2d');\n";
  html += "  const width = canvas.width;\n";
  html += "  const height = canvas.height;\n";
  html += "  ctx.clearRect(0, 0, width, height);\n";

  html += "  const table = document.getElementById('dataTable');\n";
  html += "  const rows = table.getElementsByTagName('tbody')[0].getElementsByTagName('tr');\n";
  html += "  if (rows.length === 0) return;\n";

  html += "  const data = [];\n";
  html += "  const rpmValues = [600, 6, 200, 300, 3, 100];\n";

  html += "  for (let i = 0; i < rows.length; i++) {\n";
  html += "    const cells = rows[i].getElementsByTagName('td');\n";
  html += "    data.push({\n";
  html += "      label: cells[0].textContent,\n";
  html += "      rpm: rpmValues[i],\n";
  html += "      time: parseFloat(cells[1].textContent),\n";
  html += "      value: parseFloat(cells[2].textContent)\n";
  html += "    });\n";
  html += "  }\n";

  html += "  let minVal = 1000, maxVal = 0;\n";
  html += "  for (let i = 0; i < data.length; i++) {\n";
  html += "    minVal = Math.min(minVal, data[i].value);\n";
  html += "    maxVal = Math.max(maxVal, data[i].value);\n";
  html += "  }\n";
  html += "  minVal = Math.max(0, minVal - 5);\n";
  html += "  maxVal = Math.min(100, maxVal + 5);\n";

  html += "  let minRPM = Math.min(...rpmValues);\n";
  html += "  let maxRPM = Math.max(...rpmValues);\n";
  html += "  const rpmPadding = (maxRPM - minRPM) * 0.1;\n";
  html += "  minRPM = Math.max(0, minRPM - rpmPadding);\n";
  html += "  maxRPM = maxRPM + rpmPadding;\n";

  html += "  ctx.strokeStyle = '#555';\n";
  html += "  ctx.lineWidth = 1;\n";
  html += "  ctx.beginPath();\n";
  html += "  ctx.moveTo(30, 10);\n";
  html += "  ctx.lineTo(30, height - 30);\n";
  html += "  ctx.lineTo(width - 10, height - 30);\n";
  html += "  ctx.stroke();\n";

  html += "  for (let i = 0; i < data.length; i++) {\n";
  html += "    const x = 30 + ((data[i].rpm - minRPM) / (maxRPM - minRPM)) * (width - 40);\n";
  html += "    const y = height - 30 - ((data[i].value - minVal) / (maxVal - minVal)) * (height - 40);\n";

  html += "    ctx.fillStyle = '#4CAF50';\n";
  html += "    ctx.beginPath();\n";
  html += "    ctx.arc(x, y, 6, 0, Math.PI * 2);\n";
  html += "    ctx.fill();\n";
  html += "    ctx.strokeStyle = '#e0e0e0';\n";
  html += "    ctx.lineWidth = 1;\n";
  html += "    ctx.stroke();\n";

  html += "    ctx.fillStyle = '#e0e0e0';\n";
  html += "    ctx.font = '10px Arial';\n";
  html += "    ctx.textAlign = 'center';\n";
  html += "    ctx.fillText(data[i].rpm + ' RPM', x, y - 10);\n";
  html += "    ctx.fillText(data[i].value.toFixed(1), x, y + 20);\n";
  html += "  }\n";

  html += "  const point600 = data.find(d => d.rpm === 600);\n";
  html += "  const point300 = data.find(d => d.rpm === 300);\n";

  html += "  if (point600 && point300) {\n";
  html += "    const x1 = 30 + ((point600.rpm - minRPM) / (maxRPM - minRPM)) * (width - 40);\n";
  html += "    const y1 = height - 30 - ((point600.value - minVal) / (maxVal - minVal)) * (height - 40);\n";
  html += "    const x2 = 30 + ((point300.rpm - minRPM) / (maxRPM - minRPM)) * (width - 40);\n";
  html += "    const y2 = height - 30 - ((point300.value - minVal) / (maxVal - minVal)) * (height - 40);\n";

  html += "    ctx.beginPath();\n";
  html += "    ctx.moveTo(x1, y1);\n";
  html += "    ctx.lineTo(x2, y2);\n";
  html += "    ctx.strokeStyle = '#2196F3';\n";
  html += "    ctx.lineWidth = 2;\n";
  html += "    ctx.stroke();\n";

  html += "    const pendiente = (point300.value - point600.value) / (300 - 600);\n";
  html += "    const corteY = point600.value - pendiente * 600;\n";

  html += "    const yIntercept = corteY;\n";

  html += "    const xIntercept = 30; // Posición del eje Y\n";
  html += "    const yInterceptPos = height - 30 - ((yIntercept - minVal) / (maxVal - minVal)) * (height - 40);\n";
  html += "    ctx.strokeStyle = '#FF9800';\n";
  html += "    ctx.setLineDash([5, 5]); // Línea punteada\n";
  html += "    ctx.beginPath();\n";
  html += "    ctx.moveTo(x1, y1);\n";
  html += "    ctx.lineTo(xIntercept, yInterceptPos);\n";
  html += "    ctx.stroke();\n";
  html += "    ctx.setLineDash([]); // Resetear línea\n";

  html += "    ctx.fillStyle = '#FF9800';\n";
  html += "    ctx.fillText(`Intersección Y: ${yIntercept.toFixed(2)}`, xIntercept + 10, yInterceptPos);\n";

  html += "    //document.getElementById('pendienteInfo').innerHTML = `<strong>Pendiente entre 600 RPM y 300 RPM:</strong> ${pendiente.toFixed(5)}<br><strong>Ecuación:</strong> y = ${pendiente.toFixed(5)}x + ${corteY.toFixed(2)}`;\n";
  html += "  } //else {\n";
  html += "  //  document.getElementById('pendienteInfo').textContent = 'Se necesitan los puntos de 600 RPM y 300 RPM para calcular la pendiente';\n";
  html += "  //}\n";

  html += "  ctx.fillStyle = '#e0e0e0';\n";
  html += "  ctx.font = '12px Arial';\n";
  html += "  ctx.textAlign = 'center';\n";
  html += "  ctx.fillText('RPM', width / 2, height - 10);\n";
  html += "  ctx.save();\n";
  html += "  ctx.translate(15, height / 2);\n";
  html += "  ctx.rotate(-Math.PI / 2);\n";
  html += "  ctx.textAlign = 'center';\n";
  html += "  ctx.fillText('Valor', 0, 0);\n";
  html += "  ctx.restore();\n";
  html += "}\n";

  html += "function startMonitoring() {\n";
  html += "  fetch('/start').then(response => {\n";
  html += "    if (response.ok) {\n";
  html += "      document.querySelector('#startBtn').disabled = true;\n";
  html += "      document.querySelector('#stopBtn').disabled = false;\n";
  html += "      document.querySelector('#continuousBtn').disabled = true;\n";
  html += "      document.getElementById('statusText').textContent = 'Monitoreando... 50s restantes';\n";
  html += "      pollSensorValue();\n";
  html += "    }\n";
  html += "  });\n";
  html += "}\n";

  html += "function stopMonitoring() {\n";
  html += "  fetch('/stop').then(response => {\n";
  html += "    if (response.ok) {\n";
  html += "      document.querySelector('#stopBtn').disabled = true;\n";
  html += "      document.getElementById('statusText').textContent = 'Monitoreo detenido';\n";
  html += "      location.reload();\n";
  html += "    }\n";
  html += "  });\n";
  html += "}\n";

  html += "function resetMeasurements() {\n";
  html += "  fetch('/reset').then(response => {\n";
  html += "    if (response.ok) {\n";
  html += "      location.reload();\n";
  html += "    }\n";
  html += "  });\n";
  html += "}\n";

  html += "function startContinuous() {\n";
  html += "  fetch('/continuous').then(response => {\n";
  html += "    if (response.ok) {\n";
  html += "      document.querySelector('#startBtn').disabled = true;\n";
  html += "      document.querySelector('#stopBtn').disabled = false;\n";
  html += "      document.querySelector('#continuousBtn').disabled = true;\n";
  html += "      document.getElementById('statusText').textContent = 'Modo continuo activo';\n";
  html += "      pollSensorValue();\n";
  html += "    }\n";
  html += "  });\n";
  html += "}\n";

  html += "let realtimeData = [];\n";
  html += "function pollSensorValue() {\n";
  html += "  fetch('/value').then(response => response.json()).then(data => {\n";
  html += "    document.getElementById('currentValue').textContent = data.value.toFixed(1);\n";
  html += "    document.getElementById('currentRPM').textContent = data.currentRPM || '--';\n";
  html += "    \n";
  html += "    if (data.continuous) {\n";
  html += "      document.getElementById('statusText').textContent = 'Modo continuo activo';\n";
  html += "    } else if (data.monitoring) {\n";
  html += "      document.getElementById('statusText').textContent = 'Monitoreando... ' + data.remaining + 's restantes';\n";
  html += "    }\n";
  html += "    \n";
  html += "    document.getElementById('measureCount').textContent = data.measureCount;\n";

  html += "    if (data.monitoring && !data.continuous) {\n";
  html += "      const progressPercent = 100 - (data.remaining / 50 * 100);\n";
  html += "      document.getElementById('progressBar').style.width = progressPercent + '%';\n";
  html += "    } else if (data.continuous) {\n";
  html += "      document.getElementById('progressBar').style.width = '100%';\n";
  html += "    }\n";

  html += "    realtimeData.push({time: data.time, value: data.value});\n";
  html += "    if (realtimeData.length > 50) realtimeData.shift();\n";
  html += "    drawRealTimeChart(document.getElementById('realTimeChart'), realtimeData);\n";

  html += "    if (data.monitoring || data.continuous) {\n";
  html += "      setTimeout(pollSensorValue, 500);\n";
  html += "    } else {\n";
  html += "      document.getElementById('statusText').textContent = 'Medición completada';\n";
  html += "      document.querySelector('#stopBtn').disabled = true;\n";

  html += "      if (data.measureCount >= 6) {\n";
  html += "        document.getElementById('statusText').textContent = 'Todas las mediciones completadas';\n";
  html += "        document.querySelector('#startBtn').disabled = true;\n";
  html += "        document.querySelector('#continuousBtn').disabled = true;\n";
  html += "      } else {\n";
  html += "        document.querySelector('#startBtn').disabled = false;\n";
  html += "        document.querySelector('#continuousBtn').disabled = false;\n";
  html += "      }\n";

  html += "      location.reload();\n";
  html += "    }\n";
  html += "  }).catch(error => {\n";
  html += "    console.error('Error polling sensor:', error);\n";
  html += "    setTimeout(pollSensorValue, 2000);\n";
  html += "  });\n";
  html += "}\n";

  html += "window.onload = function() {\n";
  html += "  document.getElementById('startBtn').addEventListener('click', startMonitoring);\n";
  html += "  document.getElementById('stopBtn').addEventListener('click', stopMonitoring);\n";
  html += "  document.getElementById('resetBtn').addEventListener('click', resetMeasurements);\n";
  html += "  document.getElementById('continuousBtn').addEventListener('click', startContinuous);\n";

  html += "  const realTimeCanvas = document.getElementById('realTimeChart');\n";
  html += "  realTimeCanvas.width = realTimeCanvas.clientWidth;\n";
  html += "  realTimeCanvas.height = realTimeCanvas.clientHeight;\n";

  html += "  const historyCanvas = document.getElementById('historyChart');\n";
  html += "  historyCanvas.width = historyCanvas.clientWidth;\n";
  html += "  historyCanvas.height = historyCanvas.clientHeight;\n";
  html += "  drawHistoryChart(historyCanvas);\n";

  html += "  if (document.getElementById('statusText').textContent.includes('Monitoreando') || \n";
  html += "      document.getElementById('statusText').textContent.includes('continuo')) {\n";
  html += "    pollSensorValue();\n";
  html += "  }\n";
  html += "};\n";

  html += "</script>\n";

  html += "</body></html>\n";

  server.send(200, "text/html", html);
}

void handleStart() {
  if (!isMonitoring && measurementCount < MAX_DATAPOINTS) {
    isMonitoring = true;
    isContinuous = false;
    startTime = millis();
    lastSampleTime = startTime;
    lastValue = 0;
    digitalWrite(selectOnOff, LOW);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No se puede iniciar la medición");
  }
}

void handleContinuous() {
  if (!isMonitoring && measurementCount < MAX_DATAPOINTS) {
    isMonitoring = true;
    isContinuous = true;
    startTime = millis();
    lastSampleTime = startTime;
    lastValue = 0;
    digitalWrite(selectOnOff, LOW);
    scale.tare();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No se puede iniciar la medición continua");
  }
}

void handleStop() {
  if (isMonitoring) {
    if (isContinuous || (millis() - startTime) < monitorDuration) {
      addDataPoint(currentValue);
    }
    isMonitoring = false;
    isContinuous = false;
    digitalWrite(selectOnOff, HIGH);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No hay monitoreo activo");
  }
}

// Función para reiniciar todas las mediciones
void handleReset() {
  resetMeasurements();
  digitalWrite(selectOnOff, HIGH);
  server.send(200, "text/plain", "OK");
}

// Función para obtener el valor actual del sensor
void handleGetValue() {
  float value = readSensor();
  currentValue = value;  // Actualizar valor actual

  // Actualizar último valor
  if (isMonitoring) {
    lastValue = value;
  }

  unsigned long currentTime = millis();

  // Obtener el RPM actual basado en el índice de medición
  int currentRPM = 0;
  if (measurementCount < MAX_DATAPOINTS) {
    currentRPM = rpmValues[measurementCount];
  }

  // Verificar condiciones de monitoreo según el modo
  bool stillMonitoring = false;

  if (isContinuous) {
    // En modo continuo, seguimos monitoreando hasta que se detenga manualmente
    stillMonitoring = isMonitoring;

    // En modo continuo, tomar una medición cada vez que se alcance el intervalo
    if (isMonitoring && (currentTime - lastSampleTime) >= monitorDuration) {
      addDataPoint(lastValue);
      lastSampleTime = currentTime;  // Actualizar último tiempo de muestreo

      // Si alcanzamos el máximo de muestras, detener
      if (measurementCount >= MAX_DATAPOINTS) {
        isMonitoring = false;
        isContinuous = false;

        //digitalWrite(selectOnOff, HIGH);
      }
    }
  } else {
    // En modo normal, verificamos si seguimos dentro del tiempo de monitoreo
    stillMonitoring = isMonitoring && (currentTime - startTime) < monitorDuration;

    // Si terminó el monitoreo normal, guardar la medición y detenerlo
    if (!stillMonitoring && isMonitoring) {
      addDataPoint(lastValue);  // Guardar el último valor obtenido del período

      //digitalWrite(selectOnOff, HIGH);
      isMonitoring = false;
    }
  }

  // Calcular tiempo restante (solo en modo normal)
  int remainingSeconds = (!isContinuous && stillMonitoring) ? (monitorDuration - (currentTime - startTime)) / 1000 : 0;

  // Crear respuesta JSON
  String json = "{";
  json += "\"value\":" + String(value);
  json += ",\"time\":" + String((currentTime - startTime) / 1000);
  json += ",\"remaining\":" + String(remainingSeconds);
  json += ",\"monitoring\":" + String(stillMonitoring ? "true" : "false");
  json += ",\"continuous\":" + String(isContinuous ? "true" : "false");
  json += ",\"measureCount\":" + String(measurementCount);
  json += ",\"currentRPM\":" + String(currentRPM);  // Añadir el RPM actual
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(selectHighLow, OUTPUT);
  pinMode(selectOnOff, OUTPUT);
  digitalWrite(selectHighLow, HIGH);
  digitalWrite(selectOnOff, HIGH);

  myServo.attach(SERVO_PIN);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);

  scale.tare();

  // Configurar el pin del sensor
  pinMode(SENSOR_PIN, INPUT);

  // Inicializar el AP
  Serial.println("Configurando Access Point...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("Dirección IP del AP: ");
  Serial.println(IP);

  // Configurar rutas del servidor
  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/continuous", HTTP_GET, handleContinuous);
  server.on("/value", HTTP_GET, handleGetValue);

  // Inicializar variables
  resetMeasurements();

  // Iniciar el servidor
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}