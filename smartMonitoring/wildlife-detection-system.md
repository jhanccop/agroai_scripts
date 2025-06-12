## 9. Nuevas Funcionalidades Implementadas

### 9.1 Configuración Remota via MQTT

El sistema ahora puede recibir configuración desde un servidor remoto a través de MQTT. Los parámetros configurables incluyen:

- **Horario de operación**: `operation_start_hour` y `operation_end_hour`
- **Frecuencia de envío**: `send_frequency_minutes` (en minutos)
- **Configuración de red**: `broker` (dirección del broker MQTT)
- **Resoluciones**: `operation_resolution` y `server_image_resolution`
- **Parámetros de detección**: `confidence_threshold`, `nms_threshold`, `target_class`
- **Servidor web**: `web_server_url`
- **Tiempos de operación**: `video_duration`, `min_detection_interval`
- **Modo de envío**: `auto_send` (automático o manual)

#### Ejemplo de mensaje de configuración MQTT:

```json
{
  "operation_start_hour": 6,
  "operation_end_hour": 18,
  "send_frequency_minutes": 30,
  "broker": "192.168.1.100",
  "operation_resolution": [640, 480],
  "server_image_resolution": [320, 240],
  "confidence_threshold": 0.60,
  "nms_threshold": 0.4,
  "target_class": "butterfly",
  "web_server_url": "http://192.168.1.100:8080/upload",
  "video_duration": 15,
  "min_detection_interval": 3,
  "auto_send": true
}
```

### 9.2 Sistema de Buffer de Detecciones

El sistema ahora utiliza un buffer inteligente que:

- **Almacena todas las detecciones** durante el período de operación
- **Persiste datos en disco** para evitar pérdida de información
- **Programa envíos automáticos** según la frecuencia configurada
- **Maneja reintentos** para envíos fallidos
- **Limpia automáticamente** detecciones antiguas

### 9.3 Operación por Horarios

El sistema respeta los horarios configurados:

- **Solo detecta durante horario de operación** definido por `operation_start_hour` y `operation_end_hour`
- **Soporta horarios que cruzan medianoche** (ej: 18:00 a 6:00)
- **Pausa automáticamente** fuera del horario de operación

### 9.4 Envío por Lotes Programado

El nuevo comportamiento de envío funciona así:

1. **Acumula detecciones** durante el período especificado
2. **Envía en lotes** según la frecuencia configurada
3. **Procesa uno por uno** para asegurar integridad de datos
4. **Mantiene estadísticas** de envíos exitosos y fallidos

### 9.5 Gestión Mejorada de Configuración

La clase `ConfigManager` maneja:

- **Conexión automática** al broker MQTT para configuración
- **Actualización en tiempo real** de parámetros
- **Validación de configuración** recibida
- **Valores por defecto** robustos

## 10. Estructura de Archivos del Sistema Mejorado

```
butterfly_detection/
├── main_enhanced.py          # Sistema principal mejorado
├── config.py                 # Configuración base
├── config_manager.py         # Gestor de configuración remota
├── detection_buffer.py       # Buffer de detecciones
├── camera.py                 # Gestión de cámara
├── detector.py               # Detección de mariposas (mejorado)
├── processor.py              # Procesamiento de datos (mejorado)
├── communication.py          # Comunicación MQTT/HTTP (mejorado)
├── script_diagnostico.py     # Diagnóstico de cámara
├── buffer/                   # Directorio de buffer de detecciones
├── videos/                   # Videos grabados
├── images/                   # Imágenes anotadas
└── models/                   # Modelos de IA
```

## 11. Uso del Sistema Mejorado

### 11.1 Ejecución Básica

```bash
cd ~/butterfly_detection
python3 main_enhanced.py
```

### 11.2 Configuración del Broker MQTT

Para enviar configuración al sistema, publique un mensaje JSON en el topic `butterfly/config`:

```bash
mosquitto_pub -h 192.168.1.100 -t "butterfly/config" -m '{
  "operation_start_hour": 7,
  "operation_end_hour": 19,
  "send_frequency_minutes": 60,
  "confidence_threshold": 0.70
}'
```

### 11.3 Monitoreo del Sistema

El sistema genera logs detallados en `butterfly_detector.log` y puede monitorearse con:

```bash
tail -f butterfly_detector.log
```

Para ver estadísticas del buffer en tiempo real, el sistema registra periódicamente:
- Número total de detecciones
- Detecciones enviadas exitosamente
- Detecciones pendientes de envío
- Tiempo promedio entre detecciones

### 11.4 Servicio Systemd Actualizado

El servicio systemd ha sido actualizado para usar el nuevo script principal:

```bash
sudo systemctl daemon-reload
sudo systemctl restart butterfly-detector.service
sudo systemctl status butterfly-detector.service
```

## 12. Ventajas del Sistema Mejorado

1. **Configuración Remota**: Permite ajustar parámetros sin acceso físico al dispositivo
2. **Operación Programada**: Funciona solo durante las horas especificadas, ahorrando energía
3. **Buffer Inteligente**: Previene pérdida de datos y optimiza el uso de ancho de banda
4. **Envío Eficiente**: Agrupa detecciones para envío masivo en lugar de individual
5. **Recuperación Automática**: Reintenta envíos fallidos y mantiene persistencia en disco
6. **Escalabilidad**: Fácil de configurar múltiples dispositivos con diferentes parámetros
7. **Monitoreo Avanzado**: Estadísticas detalladas y logging comprehensivo

El sistema ahora es mucho más robusto y adecuado para implementaciones en producción, con capacidades de gestión remota y operación autónoma durante largos períodos.### 5.5 communication.py - Módulo de comunicación

```python
import paho.mqtt.client as mqtt
import requests
import json
import time
import threading
import os
import logging
from config import (
    MQTT_BROKER, MQTT_PORT, MQTT_TOPIC_DATA, MQTT_CLIENT_ID,
    MQTT_USERNAME, MQTT_PASSWORD, WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD
)

logger = logging.getLogger("Communication")

class CommunicationManager:
    def __init__(self):
        # Inicializar cliente MQTT
        self.mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID)
        self.mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
        self.mqtt_client.on_connect = self._on_mqtt_connect
        self.mqtt_client.on_disconnect = self._on_mqtt_disconnect
        
        # Cola para reenvíos
        self.retry_queue = []
        self.connected = False
        self.lock = threading.Lock()
        
        # Configuración dinámica
        self.web_server_url = None
        
        # Iniciar cliente MQTT
        self.start_mqtt_client()
        
    def update_web_server_url(self, url):
        """Actualiza la URL del servidor web"""
        self.web_server_url = url
        
    def start_mqtt_client(self):
        """Inicia la conexión MQTT en un hilo separado"""
        def connect_thread():
            while True:
                try:
                    self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
                    self.mqtt_client.loop_start()
                    break
                except Exception as e:
                    logger.error(f"Error connecting to MQTT broker: {e}")
                    time.sleep(10)  # Reintento cada 10 segundos
        
        threading.Thread(target=connect_thread, daemon=True).start()
        
    def _on_mqtt_connect(self, client, userdata, flags, rc):
        """Callback cuando se conecta al broker MQTT"""
        if rc == 0:
            logger.info("Connected to MQTT broker")
            with self.lock:
                self.connected = True
                # Enviar mensajes pendientes
                pending_messages = self.retry_queue.copy()
                self.retry_queue.clear()
            
            # Intentar reenviar mensajes pendientes
            for topic, payload in pending_messages:
                self.send_mqtt_message(topic, payload)
        else:
            logger.error(f"Failed to connect to MQTT broker with code: {rc}")
            
    def _on_mqtt_disconnect(self, client, userdata, rc):
        """Callback cuando se desconecta del broker MQTT"""
        logger.warning(f"Disconnected from MQTT broker with code: {rc}")
        with self.lock:
            self.connected = False
        
        # Intentar reconectar
        if rc != 0:
            threading.Thread(target=self.start_mqtt_client, daemon=True).start()
            
    def send_mqtt_message(self, topic, payload):
        """
        Envía un mensaje MQTT al broker
        
        Args:
            topic: Tema MQTT
            payload: Contenido del mensaje (dict o string)
            
        Returns:
            True si se envió correctamente, False en caso contrario
        """
        # Convertir a JSON si es necesario
        if isinstance(payload, dict):
            payload = json.dumps(payload)
            
        with self.lock:
            if not self.connected:
                self.retry_queue.append((topic, payload))
                logger.warning(f"MQTT not connected. Message queued for later delivery.")
                return False
        
        try:
            result = self.mqtt_client.publish(topic, payload, qos=1)
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                logger.error(f"Failed to publish MQTT message: {result.rc}")
                with self.lock:
                    self.retry_queue.append((topic, payload))
                return False
            logger.info(f"MQTT message sent successfully to {topic}")
            return True
        except Exception as e:
            logger.error(f"Error publishing MQTT message: {e}")
            with self.lock:
                self.retry_queue.append((topic, payload))
            return False
            
    def send_video_to_server(self, video_path, server_url=None):
        """
        Envía un archivo de video al servidor web
        
        Args:
            video_path: Ruta al archivo de video
            server_url: URL del servidor (opcional, usa la configurada si no se especifica)
            
        Returns:
            True si se envió correctamente, False en caso contrario
        """
        if not os.path.exists(video_path):
            logger.error(f"Video file not found: {video_path}")
            return False
        
        # Usar URL configurada o la proporcionada
        url = server_url or self.web_server_url
        if not url:
            logger.error("No web server URL configured")
            return False
            
        try:
            with open(video_path, 'rb') as video_file:
                files = {'video': video_file}
                response = requests.post(
                    url,
                    files=files,
                    auth=(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD),
                    timeout=60  # 60 segundos de timeout para subidas grandes
                )
                
                if response.status_code == 200:
                    logger.info(f"Video uploaded successfully: {video_path}")
                    return True
                else:
                    logger.error(f"Failed to upload video: {response.status_code} - {response.text}")
                    return False
        except Exception as e:
            logger.error(f"Error uploading video: {e}")
            return False
            
    def send_detection_batch(self, detections):
        """
        Envía un lote de detecciones al servidor
        
        Args:
            detections: Lista de detecciones a enviar
            
        Returns:
            List of (success, detection_timestamp) tuples
        """
        results = []
        
        for detection in detections:
            detection_data = detection["detection_data"]
            timestamp = detection["timestamp"]
            video_path = detection.get("video_path")
            
            # Enviar datos JSON por MQTT
            mqtt_success = self.send_mqtt_message(MQTT_TOPIC_DATA, detection_data)
            
            # Enviar video al servidor web si existe
            video_success = True
            if video_path and os.path.exists(video_path):
                video_success = self.send_video_to_server(video_path)
            
            # Consideramos exitoso si al menos el MQTT funciona
            overall_success = mqtt_success
            results.append((overall_success, timestamp))
            
            if overall_success:
                logger.info(f"Detection {timestamp} sent successfully")
            else:
                logger.error(f"Failed to send detection {timestamp}")
            
            # Pequeña pausa entre envíos para no saturar la red
            time.sleep(0.5)
        
        return results### 8.4 Solución para el error de Qt/XCB

Si encuentra el siguiente error:

```
qt.qpa.plugin: Could not load the Qt platform plugin "xcb" in "/home/orangepi/bia/lib/python3.10/site-packages/cv2/qt/plugins" even though it was found.
This application failed to start because no Qt platform plugin could be initialized. Reinstalling the application may fix this problem.
Available platform plugins are: xcb.
Aborted
```

Este error está relacionado con la interfaz gráfica Qt que utiliza OpenCV cuando intenta mostrar imágenes con `cv2.imshow()`. Esto ocurre porque el sistema está ejecutándose en un entorno sin interfaz gráfica (headless) o hay problemas con las bibliotecas X11/XCB.

Hay dos soluciones:

#### 8.4.1 Usar el script headless

Se ha incluido un script especial (`script_headless.py`) que ejecuta el sistema sin intentar mostrar ninguna interfaz gráfica:

```bash
cd ~/butterfly_detection
python3 script_headless.py
```

Este script:
- No utiliza funciones de visualización como `cv2.imshow()`
- Guarda las imágenes de detección en archivos para verificación
- Utiliza un sistema de logging más robusto

#### 8.4.2 Instalar las dependencias de Qt/XCB

Si prefiere utilizar la versión original con visualización, puede instalar las dependencias necesarias:

```bash
sudo apt update
sudo apt install -y libqt5gui5 libqt5core5a libqt5widgets5 libx11-xcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0
```

Sin embargo, para un sistema embebido de detección de mariposas, recomendamos usar la versión headless, ya que:
- Consume menos recursos
- Es más estable en entornos sin monitor
- Sigue guardando imágenes de las detecciones para su verificación

#### 8.4.3 Modificar el archivo principal

Alternativamente, puede modificar `main.py` para establecer la variable `HEADLESS_MODE = True` al inicio del archivo. Esto desactivará todas las llamadas a funciones de visualización.### 5.9 script_headless.py - Script para ejecutar sin interfaz gráfica

```python
#!/usr/bin/env python3
import time
import cv2
import signal
import sys
import os
from datetime import datetime
import logging

# Configurar logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler("butterfly_detector.log")
    ]
)
logger = logging.getLogger("ButterflyDetector")

# Importar módulos del proyecto
# Asegurarse de estar en el directorio correcto para las importaciones
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

try:
    from camera import Camera
    from detector import ButterflyDetector
    from processor import DetectionProcessor
    from communication import CommunicationManager
    from config import MIN_DETECTION_INTERVAL
except ImportError as e:
    logger.error(f"Error importando módulos: {e}")
    logger.error("Asegúrese de ejecutar este script desde el directorio del proyecto")
    sys.exit(1)

def handle_exit(signum, frame):
    """Manejador de señales para salida limpia"""
    logger.info("Cerrando aplicación...")
    if 'camera' in globals():
        camera.stop()
    sys.exit(0)

# Registrar manejadores de señales
signal.signal(signal.SIGINT, handle_exit)
signal.signal(signal.SIGTERM, handle_exit)

def main():
    logger.info("Iniciando Sistema de Detección de Mariposas (Modo Headless)...")
    
    try:
        # Inicializar componentes
        camera = Camera()
        detector = ButterflyDetector()
        processor = DetectionProcessor()
        comms = CommunicationManager()
        
        # Iniciar captura de cámara
        camera.start()
        logger.info("Cámara iniciada correctamente")
        
        last_detection_time = 0
        
        logger.info("Sistema ejecutándose - esperando mariposas...")
        
        while True:
            # Obtener frame de la cámara
            frame = camera.get_frame()
            
            if frame is None:
                time.sleep(0.1)
                continue
                
            # Ejecutar detección
            try:
                boxes, confidences, indices, class_ids = detector.detect(frame)
            except Exception as e:
                logger.error(f"Error en detección: {e}")
                time.sleep(1)
                continue
            
            # Si hay detecciones y ha pasado suficiente tiempo desde la última
            current_time = time.time()
            if len(indices) > 0 and (current_time - last_detection_time) > MIN_DETECTION_INTERVAL:
                logger.info(f"Detectadas {len(indices)} mariposas!")
                last_detection_time = current_time
                
                try:
                    # Iniciar grabación de video
                    video_path = camera.start_recording()
                    
                    # Crear JSON con datos de detección
                    detection_json, image_path = processor.create_detection_json(
                        frame, boxes, confidences, indices, video_path
                    )
                    
                    # Crear imagen con detecciones (solo para logging)
                    annotated_frame = detector.annotate_frame(frame, boxes, confidences, indices, class_ids)
                    
                    # Guardar imagen anotada para verificación
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    debug_img_path = f"debug_detection_{timestamp}.jpg"
                    cv2.imwrite(debug_img_path, annotated_frame)
                    logger.info(f"Imagen de depuración guardada en {debug_img_path}")
                    
                    # Enviar datos por MQTT (el video se enviará cuando termine la grabación)
                    comms.send_detection(detection_json, video_path)
                    
                    logger.info(f"Detección procesada - JSON enviado por MQTT, video grabándose en: {video_path}")
                except Exception as e:
                    logger.error(f"Error procesando detección: {e}")
                
            # Pequeña pausa para no saturar la CPU
            time.sleep(0.01)
            
    except KeyboardInterrupt:
        logger.info("Programa interrumpido por el usuario")
    except Exception as e:
        logger.error(f"Error inesperado: {e}")
    finally:
        if 'camera' in locals():
            camera.stop()
        logger.info("Sistema detenido")

if __name__ == "__main__":
    main()
```## 8. Solución de problemas con la cámara

Si experimenta problemas para que la cámara sea reconocida, este documento proporciona información para ayudarle a solucionar el problema. El error común que podría ver es:

```
Error capturing frame from camera
[ WARN:1@17.680] global cap_v4l.cpp:913 open VIDEOIO(V4L2:/dev/video0): can't open camera by index
[ERROR:1@17.686] global obsensor_uvc_stream_channel.cpp:158 getStreamChannelGroup Camera index out of range
```

### 8.1 Script de diagnóstico

El sistema incluye un script de diagnóstico (`script_diagnostico.py`) que puede ayudar a identificar y resolver problemas con la cámara. Este script:

1. Lista todos los dispositivos de video disponibles en el sistema
2. Prueba diferentes APIs de OpenCV para abrir la cámara
3. Proporciona recomendaciones basadas en los resultados

Para ejecutar el script de diagnóstico:

```bash
cd ~/butterfly_detection
python3 script_diagnostico.py
```

### 8.2 Verificación de la cámara

Si la cámara está siendo detectada como:
```
Bus 001 Device 002: ID 32e4:1298 16MP Camera Mamufacture 16MP USB Camera
```
pero sigue dando errores, pruebe las siguientes soluciones:

#### 8.2.1 Verificar permisos

Asegúrese de que el usuario tiene permisos para acceder al dispositivo de video:

```bash
sudo chmod 666 /dev/video0
```

#### 8.2.2 Instalar herramientas de V4L2

```bash
sudo apt-get install v4l-utils
```

Luego verifique los formatos soportados:
```bash
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

#### 8.2.3 Modificar la configuración

Basado en los resultados del script de diagnóstico, modifique `config.py` para usar la configuración que funcionó mejor:

1. Cambie `CAMERA_DEVICE` al dispositivo que funcionó (por ejemplo, `/dev/video0` o índice `0`)
2. Cambie `CAMERA_API` a la API que funcionó (V4L2, V4L, etc.)
3. Ajuste `CAMERA_RESOLUTION` a una resolución compatible con la cámara

#### 8.2.4 Probar la cámara independientemente

Puede probar si la cámara funciona correctamente con otras herramientas:

```bash
# Ver la imagen de la cámara
mplayer tv:// -tv driver=v4l2:device=/dev/video0

# O con VLC
cvlc v4l2:///dev/video0
```

#### 8.2.5 Verificar la conexión USB

- Intente conectar la cámara a diferentes puertos USB
- Si es posible, use un hub USB con alimentación externa
- Reinicie la Orange Pi con la cámara conectada

### 8.3 Soluciones alternativas

Si continúa teniendo problemas con la cámara USB, considere estas alternativas:

1. **Usar una cámara CSI** si su Orange Pi 2 W tiene un puerto CSI
2. **Probar con GStreamer** para capturar video:

```python
# En camera.py
def __init__(self):
    # Usar GStreamer como alternativa
    gst_str = 'v4l2src device=/dev/video0 ! video/x-raw, width=640, height=480 ! videoconvert ! appsink'
    self.camera = cv2.VideoCapture(gst_str, cv2.CAP_GSTREAMER)
```

3. **Usar software más básico de captura** como fswebcam:

```bash
sudo apt-get install fswebcam
```

Y modificar el código para usar fswebcam para capturar imágenes en lugar de OpenCV.### 5.8 script_diagnostico.py - Script para diagnosticar la cámara

```python
#!/usr/bin/env python3
import cv2
import time
import os
import subprocess

def listar_dispositivos_video():
    """Lista todos los dispositivos de video disponibles en el sistema"""
    print("\n=== DISPOSITIVOS DE VIDEO DISPONIBLES ===")
    
    # Listar dispositivos en /dev/
    print("\nDispositivos en /dev/:")
    video_devices = [d for d in os.listdir('/dev') if d.startswith('video')]
    if video_devices:
        for device in sorted(video_devices):
            print(f"/dev/{device}")
    else:
        print("No se encontraron dispositivos de video en /dev/")
    
    # Información de dispositivos USB
    print("\nDispositivos USB conectados:")
    try:
        usb_output = subprocess.check_output(['lsusb'], universal_newlines=True)
        for line in usb_output.splitlines():
            if 'Camera' in line or 'cam' in line.lower() or 'video' in line.lower():
                print(f"[CÁMARA] {line.strip()}")
            else:
                print(line.strip())
    except:
        print("No se pudo ejecutar lsusb")
    
    # Información detallada V4L2
    print("\nInformación detallada de V4L2:")
    try:
        for device in video_devices:
            device_path = f"/dev/{device}"
            print(f"\nInfo para {device_path}:")
            try:
                v4l2_output = subprocess.check_output(['v4l2-ctl', '--device', device_path, '--all'], universal_newlines=True)
                # Mostrar solo las primeras 20 líneas para no saturar la salida
                print("\n".join(v4l2_output.splitlines()[:20]))
                print("...")
            except:
                print(f"No se pudo obtener información de {device_path}")
    except:
        print("No se pudo obtener información de V4L2")

def probar_apis_camara():
    """Prueba diferentes APIs de OpenCV para abrir la cámara"""
    print("\n=== PROBANDO DIFERENTES APIS DE CÁMARA ===")
    
    # Lista de APIs a probar
    apis = [
        (cv2.CAP_ANY, "AUTO"),
        (cv2.CAP_V4L2, "V4L2"),
        (cv2.CAP_V4L, "V4L"),
        (cv2.CAP_GSTREAMER, "GStreamer"),
    ]
    
    # Dispositivos a probar
    dispositivos = [
        0,  # Índice numérico
        1,  # Índice alternativo
        "/dev/video0",  # Ruta directa
        "/dev/video1",  # Ruta alternativa
    ]
    
    # Probar cada combinación
    for api_id, api_name in apis:
        for dispositivo in dispositivos:
            print(f"\nProbando API {api_name} con dispositivo {dispositivo}")
            try:
                cap = cv2.VideoCapture(dispositivo, api_id)
                if cap.isOpened():
                    print(f"✓ Conexión exitosa")
                    
                    # Obtener propiedades
                    ancho = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                    alto = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                    fps = cap.get(cv2.CAP_PROP_FPS)
                    
                    print(f"  Resolución: {ancho}x{alto}")
                    print(f"  FPS: {fps}")
                    
                    # Intentar leer un frame
                    ret, frame = cap.read()
                    if ret:
                        print(f"  ✓ Frame leído correctamente: {frame.shape}")
                        # Guardar el frame como prueba
                        output_file = f"test_{api_name}_{dispositivo}.jpg".replace("/", "_")
                        cv2.imwrite(output_file, frame)
                        print(f"  ✓ Frame guardado en {output_file}")
                    else:
                        print(f"  ✗ No se pudo leer un frame")
                    
                    cap.release()
                else:
                    print(f"✗ No se pudo abrir la cámara")
            except Exception as e:
                print(f"✗ Error: {str(e)}")

def recomendar_configuracion():
    """Proporciona recomendaciones basadas en los resultados"""
    print("\n=== RECOMENDACIONES ===")
    print("""
Basado en los resultados, modifique config.py para usar:

1. El dispositivo que funcionó correctamente (por ejemplo, /dev/video0 o índice 0)
2. La API que funcionó (V4L2, V4L, etc.)
3. Una resolución compatible

Por ejemplo:
    CAMERA_DEVICE = "/dev/video0"  # O el que funcionó
    CAMERA_API = cv2.CAP_V4L2  # O la API que funcionó
    CAMERA_RESOLUTION = (640, 480)  # O una resolución compatible
    
Si ninguna configuración funcionó, puede intentar:
1. Verificar si la cámara requiere drivers adicionales
2. Reiniciar el sistema
3. Probar con un HUB USB con alimentación externa si usa USB
4. Verificar permisos de acceso al dispositivo de video
   sudo chmod 666 /dev/videoX
""")

if __name__ == "__main__":
    print("DIAGNÓSTICO DE CÁMARA PARA SISTEMA DE DETECCIÓN DE MARIPOSAS")
    print("==========================================================")
    
    # Obtener versión de OpenCV
    print(f"Versión de OpenCV: {cv2.__version__}")
    
    # Listar dispositivos de video disponibles
    listar_dispositivos_video()
    
    # Probar diferentes APIs
    probar_apis_camara()
    
    # Mostrar recomendaciones
    recomendar_configuracion()
```# Sistema de Detección de Mariposas con Orange Pi 2 W

Este documento describe la implementación de un sistema embebido para detectar mariposas usando una Orange Pi 2 W y una cámara, con funcionalidades de procesamiento de video, comunicación MQTT y transferencia de archivos a un servidor web.

## 1. Componentes del Hardware

- **Orange Pi 2 W**
- **Cámara USB** (o cámara CSI compatible con Orange Pi)
- **Fuente de alimentación** (5V/2A recomendado)
- **Tarjeta MicroSD** (16GB o más, clase 10)
- **Opcional: Carcasa o soporte** para instalación en exteriores

## 2. Configuración del Software Base

### 2.1 Sistema Operativo

Instalaremos Armbian, una distribución Linux optimizada para dispositivos ARM:

```bash
# Descargar imagen de Armbian para Orange Pi 2 W desde https://www.armbian.com/orange-pi-2/
# Flashear la imagen en la tarjeta MicroSD usando Etcher o dd

# Una vez iniciado el sistema:
sudo apt update
sudo apt upgrade -y
```

### 2.2 Instalación de dependencias

```bash
# Instalación de paquetes básicos
sudo apt install -y python3-pip python3-opencv python3-numpy python3-paho-mqtt

# Instalación de bibliotecas para deep learning y procesamiento de video
pip3 install tensorflow-lite # Versión ligera de TensorFlow para dispositivos embebidos
pip3 install pillow scikit-image imutils

# Para comunicación MQTT
pip3 install paho-mqtt

# Para envío de archivos a servidor web
pip3 install requests
```

## 3. Arquitectura del Software

El sistema estará compuesto por varios módulos:

1. **Módulo de Captura de Video**: Para obtener imágenes de la cámara.
2. **Módulo de Detección**: Para identificar mariposas en los fotogramas.
3. **Módulo de Procesamiento**: Para generar datos JSON y comprimir imágenes.
4. **Módulo de Comunicación**: Para enviar datos mediante MQTT y HTTP.
5. **Módulo Principal**: Para coordinar todos los componentes.

## 4. Implementación

### 4.1 Configuración del entorno

Crearemos una estructura de directorios para organizar el proyecto:

```bash
mkdir -p ~/butterfly_detection/{models,videos,images,logs}
cd ~/butterfly_detection
```

### 4.2 Descarga e implementación del modelo de detección

Para la detección de mariposas, utilizaremos un modelo pre-entrenado de MobileNet SSD o YOLOv4-tiny, y lo fine-tuneamos para detectar mariposas específicamente.

```bash
# Descargar un modelo pre-entrenado (ejemplo con YOLOv4-tiny)
wget https://github.com/AlexeyAB/darknet/releases/download/darknet_yolo_v4_pre/yolov4-tiny.weights -P models/
wget https://raw.githubusercontent.com/AlexeyAB/darknet/master/cfg/yolov4-tiny.cfg -P models/
wget https://raw.githubusercontent.com/AlexeyAB/darknet/master/data/coco.names -P models/

# Convertir el modelo a formato TFLite si es necesario
```

## 5. Código Principal

A continuación se presenta el código Python completo para implementar el sistema de detección de mariposas:

### 5.1 config.py - Archivo de configuración

```python
# Configuración del sistema

# Parámetros de la cámara
CAMERA_INDEX = 0  # 0 para la primera cámara USB
CAMERA_DEVICE = "/dev/video0"  # Dispositivo de cámara específico
CAMERA_API = cv2.CAP_V4L2  # Forzar uso de V4L2
CAMERA_RESOLUTION = (640, 480)
CAMERA_FPS = 15

# Parámetros de detección (valores por defecto, pueden ser actualizados via MQTT)
CONFIDENCE_THRESHOLD = 0.50  # Umbral de confianza para detección
NMS_THRESHOLD = 0.4  # Non-Maximum Suppression threshold
TARGET_CLASS = "butterfly"  # Clase objetivo para detección

# Parámetros MQTT
MQTT_BROKER = "192.168.1.100"  # Dirección del broker MQTT
MQTT_PORT = 1883
MQTT_TOPIC_DATA = "butterfly/detections"  # Topic para enviar detecciones
MQTT_TOPIC_CONFIG = "butterfly/config"  # Topic para recibir configuración
MQTT_CLIENT_ID = "orangepi2w_butterfly_detector"
MQTT_USERNAME = "mqtt_user"
MQTT_PASSWORD = "mqtt_password"

# Parámetros del servidor web
WEB_SERVER_URL = "http://192.168.1.100:8080/upload"
WEB_SERVER_USERNAME = "web_user"
WEB_SERVER_PASSWORD = "web_password"

# Parámetros de almacenamiento
VIDEO_DIR = "videos"
IMAGE_DIR = "images"
LOG_DIR = "logs"

# Configuración de operación (valores por defecto)
OPERATION_START_HOUR = 6  # Hora de inicio (24h format)
OPERATION_END_HOUR = 18   # Hora de fin (24h format)
SEND_FREQUENCY_MINUTES = 60  # Frecuencia de envío en minutos
OPERATION_RESOLUTION = (640, 480)  # Resolución de operación
SERVER_IMAGE_RESOLUTION = (320, 240)  # Resolución de imagen enviada al servidor
VIDEO_DURATION = 10  # Tiempo de grabación de video después de la detección (segundos)
MIN_DETECTION_INTERVAL = 5  # Intervalo mínimo entre detecciones (segundos)
AUTO_SEND = True  # Envío automático o esperar configuración
```
```

### 5.2 camera.py - Módulo de captura de video

```python
import cv2
import time
import threading
import os
from datetime import datetime
from config import CAMERA_INDEX, CAMERA_DEVICE, CAMERA_API, CAMERA_RESOLUTION, CAMERA_FPS, VIDEO_DIR, VIDEO_DURATION

class Camera:
    def __init__(self):
        # Intentar primero con el dispositivo específico y API V4L2
        print(f"Intentando abrir cámara en {CAMERA_DEVICE} con API V4L2...")
        self.camera = cv2.VideoCapture(CAMERA_DEVICE, CAMERA_API)
        
        # Si falla, intentar alternativas
        if not self.camera.isOpened():
            print(f"Fallo al abrir {CAMERA_DEVICE} con V4L2. Intentando índice {CAMERA_INDEX}...")
            self.camera = cv2.VideoCapture(CAMERA_INDEX)
            
            # Si sigue fallando, intentar otros dispositivos
            if not self.camera.isOpened():
                print("Buscando cámaras disponibles...")
                for i in range(10):  # Probar los primeros 10 dispositivos
                    test_camera = cv2.VideoCapture(i)
                    if test_camera.isOpened():
                        print(f"Cámara encontrada en índice {i}")
                        self.camera = test_camera
                        break
                    test_camera.release()
                
        # Verificar si finalmente tenemos una cámara abierta        
        if not self.camera.isOpened():
            print("ERROR: No se pudo abrir ninguna cámara")
            raise ValueError("No se pudo inicializar la cámara")
            
        print("Cámara inicializada correctamente")
        
        # Configurar propiedades de la cámara
        self.camera.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_RESOLUTION[0])
        self.camera.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_RESOLUTION[1])
        self.camera.set(cv2.CAP_PROP_FPS, CAMERA_FPS)
        
        # Leer un frame para verificar que la cámara funciona
        ret, test_frame = self.camera.read()
        if not ret or test_frame is None:
            print("ADVERTENCIA: Primera lectura de frame fallida")
        else:
            print(f"Cámara funcionando. Tamaño de frame: {test_frame.shape}")
        
        self.frame = None
        self.running = False
        self.lock = threading.Lock()
        self.recording = False
        self.video_writer = None
        self.video_path = None
        
    def start(self):
        if not self.running:
            self.running = True
            self.thread = threading.Thread(target=self._capture_loop)
            self.thread.daemon = True
            self.thread.start()
            
    def _capture_loop(self):
        consecutive_failures = 0
        while self.running:
            success, frame = self.camera.read()
            if success and frame is not None:
                with self.lock:
                    self.frame = frame
                    if self.recording and self.video_writer is not None:
                        self.video_writer.write(frame)
                consecutive_failures = 0  # Reset contador de fallos
            else:
                consecutive_failures += 1
                print(f"Error capturing frame from camera (fallo #{consecutive_failures})")
                if consecutive_failures > 5:
                    print("Reintentando conexión con la cámara...")
                    time.sleep(2)
                    self.camera.release()
                    try:
                        # Reintentar con el dispositivo específico
                        self.camera = cv2.VideoCapture(CAMERA_DEVICE, CAMERA_API)
                        if not self.camera.isOpened():
                            # Probar con índice simple
                            self.camera = cv2.VideoCapture(CAMERA_INDEX)
                    except Exception as e:
                        print(f"Error al reconectar cámara: {e}")
                    consecutive_failures = 0
                time.sleep(0.5)
                
    def get_frame(self):
        with self.lock:
            if self.frame is not None:
                return self.frame.copy()
            return None
            
    def start_recording(self):
        """Start recording video to a file"""
        if self.recording:
            return self.video_path
            
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        os.makedirs(VIDEO_DIR, exist_ok=True)
        self.video_path = os.path.join(VIDEO_DIR, f"butterfly_{timestamp}.mp4")
        
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        with self.lock:
            self.video_writer = cv2.VideoWriter(
                self.video_path, 
                fourcc, 
                CAMERA_FPS, 
                CAMERA_RESOLUTION
            )
            self.recording = True
            
        # Schedule the recording to stop after VIDEO_DURATION
        stop_thread = threading.Thread(target=self._delayed_stop_recording)
        stop_thread.daemon = True
        stop_thread.start()
        
        return self.video_path
        
    def _delayed_stop_recording(self):
        """Stop recording after VIDEO_DURATION seconds"""
        time.sleep(VIDEO_DURATION)
        self.stop_recording()
        
    def stop_recording(self):
        """Stop recording and close the video file"""
        with self.lock:
            if self.recording and self.video_writer is not None:
                self.recording = False
                self.video_writer.release()
                self.video_writer = None
                return self.video_path
        return None
        
    def stop(self):
        """Stop the camera capture thread"""
        self.running = False
        if hasattr(self, 'thread'):
            self.thread.join(timeout=1.0)
        self.stop_recording()
        self.camera.release()
```

### 5.3 detector.py - Módulo de detección de mariposas

```python
import cv2
import numpy as np
import os
import logging
from config import CONFIDENCE_THRESHOLD, NMS_THRESHOLD, TARGET_CLASS

logger = logging.getLogger("ButterflyDetector")

class ButterflyDetector:
    def __init__(self, model_path="models/yolov4-tiny.weights", config_path="models/yolov4-tiny.cfg", classes_path="models/coco.names"):
        # Cargar las clases
        with open(classes_path, 'r') as f:
            self.classes = [line.strip() for line in f.readlines()]
        
        # Cargar la red neuronal
        self.net = cv2.dnn.readNetFromDarknet(config_path, model_path)
        
        # Preferir backend OpenCV con aceleración hardware si está disponible
        self.net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
        self.net.setPreferableTarget(cv2.dnn.DNN_TARGET_OPENCL)
        
        # Obtener las capas de salida
        self.layer_names = self.net.getLayerNames()
        self.output_layers = [self.layer_names[i - 1] for i in self.net.getUnconnectedOutLayers()]
        
        # Parámetros configurables
        self.confidence_threshold = CONFIDENCE_THRESHOLD
        self.nms_threshold = NMS_THRESHOLD
        self.target_class = TARGET_CLASS
        
        # Identificar el índice de la clase "butterfly" o equivalente
        self.butterfly_indices = []
        self._update_target_classes()
        
        logger.info(f"Butterfly detection initialized with class indices: {self.butterfly_indices}")
    
    def _update_target_classes(self):
        """Actualiza las clases objetivo basado en target_class"""
        self.butterfly_indices = []
        target_lower = self.target_class.lower()
        
        for i, class_name in enumerate(self.classes):
            class_lower = class_name.lower()
            if (target_lower in class_lower or 
                "butterfly" in class_lower or 
                "mariposa" in class_lower):
                self.butterfly_indices.append(i)
        
        # Si no hay clase específica de mariposa, usaremos clases similares como fallback
        if not self.butterfly_indices:
            for i, class_name in enumerate(self.classes):
                class_lower = class_name.lower()
                if ("bird" in class_lower or 
                    "insect" in class_lower or
                    "moth" in class_lower):
                    self.butterfly_indices.append(i)
    
    def update_thresholds(self, confidence_threshold, nms_threshold, target_class=None):
        """
        Actualiza los umbrales de detección dinámicamente
        
        Args:
            confidence_threshold: Nuevo umbral de confianza
            nms_threshold: Nuevo umbral NMS
            target_class: Nueva clase objetivo (opcional)
        """
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        
        if target_class and target_class != self.target_class:
            self.target_class = target_class
            self._update_target_classes()
            logger.info(f"Target class updated to: {target_class}")
        
        logger.info(f"Detection thresholds updated - Confidence: {confidence_threshold}, NMS: {nms_threshold}")
    
    def detect(self, frame):
        """
        Detect butterflies in the given frame
        
        Args:
            frame: Input image frame
            
        Returns:
            boxes: List of bounding boxes (x, y, w, h)
            confidences: List of confidence scores
            indices: List of valid detection indices after NMS
            class_ids: List of class IDs for each detection
        """
        height, width, _ = frame.shape
        
        # Convertir imagen al formato de entrada de la red
        blob = cv2.dnn.blobFromImage(frame, 1/255.0, (416, 416), swapRB=True, crop=False)
        self.net.setInput(blob)
        
        # Obtener detecciones
        outputs = self.net.forward(self.output_layers)
        
        # Procesar las detecciones
        boxes = []
        confidences = []
        class_ids = []
        
        for output in outputs:
            for detection in output:
                scores = detection[5:]
                if len(self.butterfly_indices) > 0:
                    # Si tenemos clases específicas para mariposas, solo verificamos esas
                    max_confidence = 0
                    max_class_id = -1
                    for butterfly_idx in self.butterfly_indices:
                        if butterfly_idx < len(scores) and scores[butterfly_idx] > max_confidence:
                            max_confidence = scores[butterfly_idx]
                            max_class_id = butterfly_idx
                    
                    if max_confidence > self.confidence_threshold:
                        class_id = max_class_id
                        confidence = max_confidence
                    else:
                        continue
                else:
                    # Si no hay clases específicas, buscamos cualquier clase con alta confianza
                    class_id = np.argmax(scores)
                    confidence = scores[class_id]
                    if confidence < self.confidence_threshold:
                        continue
                
                # Calcular coordenadas del bounding box
                center_x = int(detection[0] * width)
                center_y = int(detection[1] * height)
                w = int(detection[2] * width)
                h = int(detection[3] * height)
                
                # Coordenadas de la esquina superior izquierda
                x = int(center_x - w / 2)
                y = int(center_y - h / 2)
                
                boxes.append([x, y, w, h])
                confidences.append(float(confidence))
                class_ids.append(class_id)
        
        # Aplicar Non-Maximum Suppression para eliminar detecciones duplicadas
        indices = cv2.dnn.NMSBoxes(boxes, confidences, self.confidence_threshold, self.nms_threshold)
        
        return boxes, confidences, indices, class_ids
        
    def annotate_frame(self, frame, boxes, confidences, indices, class_ids):
        """
        Annotate the frame with bounding boxes around detected butterflies
        
        Args:
            frame: Input image frame
            boxes, confidences, indices, class_ids: Detection results from detect()
            
        Returns:
            Annotated frame
        """
        annotated = frame.copy()
        
        if len(indices) > 0:
            for i in indices.flatten():
                x, y, w, h = boxes[i]
                
                # Asegurar que las coordenadas estén dentro de los límites de la imagen
                x = max(0, x)
                y = max(0, y)
                
                # Dibujar rectángulo alrededor del objeto
                cv2.rectangle(annotated, (x, y), (x + w, y + h), (0, 255, 0), 2)
                
                # Mostrar etiqueta con la clase y la confianza
                class_id = class_ids[i]
                if 0 <= class_id < len(self.classes):
                    class_name = self.classes[class_id]
                else:
                    class_name = "Unknown"
                    
                label = f"{class_name}: {confidences[i]:.2f}"
                cv2.putText(annotated, label, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        
        return annotated
```

### 5.4 processor.py - Módulo de procesamiento de datos

```python
import json
import cv2
import numpy as np
import base64
import time
import uuid
from datetime import datetime
from config import IMAGE_DIR
import os

class DetectionProcessor:
    def __init__(self):
        self.last_detection_id = 0
        os.makedirs(IMAGE_DIR, exist_ok=True)
    
    def create_detection_json(self, frame, boxes, confidences, indices, video_path, server_image_resolution=(320, 240)):
        """
        Create a JSON object with detection data
        
        Args:
            frame: Original frame with detections
            boxes: Bounding boxes
            confidences: Confidence scores
            indices: Valid detection indices
            video_path: Path to the recorded video
            server_image_resolution: Resolution for the image sent to server
            
        Returns:
            JSON string with detection data
            Path to the saved annotated image
        """
        height, width = frame.shape[:2]
        timestamp = datetime.now().isoformat()
        butterfly_id = str(uuid.uuid4())
        num_butterflies = len(indices) if indices is not None else 0
        
        # Create annotated image with bounding boxes
        annotated_frame = frame.copy()
        if num_butterflies > 0:
            for i in indices.flatten():
                x, y, w, h = boxes[i]
                confidence = confidences[i]
                
                # Draw bounding box
                cv2.rectangle(annotated_frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                
                # Draw confidence
                label = f"{confidence:.2f}"
                cv2.putText(annotated_frame, label, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        
        # Save annotated image at full resolution
        image_filename = f"butterfly_{butterfly_id}.jpg"
        image_path = os.path.join(IMAGE_DIR, image_filename)
        cv2.imwrite(image_path, annotated_frame)
        
        # Create a resized and compressed image for server transmission
        resized_frame = cv2.resize(annotated_frame, server_image_resolution)
        _, buffer = cv2.imencode('.jpg', resized_frame, [cv2.IMWRITE_JPEG_QUALITY, 75])
        jpg_as_text = base64.b64encode(buffer).decode('utf-8')
        
        # Extract bounding box information (adjust coordinates for resized image)
        scale_x = server_image_resolution[0] / width
        scale_y = server_image_resolution[1] / height
        
        butterfly_boxes = []
        if num_butterflies > 0:
            for i in indices.flatten():
                x, y, w, h = boxes[i]
                confidence = confidences[i]
                butterfly_boxes.append({
                    "x": int(x * scale_x),
                    "y": int(y * scale_y),
                    "width": int(w * scale_x),
                    "height": int(h * scale_y),
                    "confidence": float(confidence),
                    "original_x": x,
                    "original_y": y,
                    "original_width": w,
                    "original_height": h
                })
        
        # Create detection data JSON
        detection_data = {
            "timestamp": timestamp,
            "butterfly_id": butterfly_id,
            "num_butterflies": num_butterflies,
            "image": {
                "width": server_image_resolution[0],
                "height": server_image_resolution[1],
                "original_width": width,
                "original_height": height,
                "format": "jpg",
                "data": jpg_as_text
            },
            "bounding_boxes": butterfly_boxes,
            "video_path": video_path
        }
        
        return detection_data, image_path
```

### 5.5 communication.py - Módulo de comunicación

```python
import paho.mqtt.client as mqtt
import requests
import json
import time
import threading
import os
from config import (
    MQTT_BROKER, MQTT_PORT, MQTT_TOPIC, MQTT_CLIENT_ID,
    MQTT_USERNAME, MQTT_PASSWORD, WEB_SERVER_URL,
    WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD
)

class CommunicationManager:
    def __init__(self):
        # Inicializar cliente MQTT
        self.mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID)
        self.mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
        self.mqtt_client.on_connect = self._on_mqtt_connect
        self.mqtt_client.on_disconnect = self._on_mqtt_disconnect
        
        # Cola para reenvíos
        self.retry_queue = []
        self.connected = False
        self.lock = threading.Lock()
        
        # Iniciar cliente MQTT
        self.start_mqtt_client()
        
    def start_mqtt_client(self):
        """Inicia la conexión MQTT en un hilo separado"""
        def connect_thread():
            while True:
                try:
                    self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
                    self.mqtt_client.loop_start()
                    break
                except Exception as e:
                    print(f"Error connecting to MQTT broker: {e}")
                    time.sleep(10)  # Reintento cada 10 segundos
        
        threading.Thread(target=connect_thread, daemon=True).start()
        
    def _on_mqtt_connect(self, client, userdata, flags, rc):
        """Callback cuando se conecta al broker MQTT"""
        if rc == 0:
            print("Connected to MQTT broker")
            with self.lock:
                self.connected = True
                # Enviar mensajes pendientes
                pending_messages = self.retry_queue.copy()
                self.retry_queue.clear()
            
            # Intentar reenviar mensajes pendientes
            for topic, payload in pending_messages:
                self.send_mqtt_message(topic, payload)
        else:
            print(f"Failed to connect to MQTT broker with code: {rc}")
            
    def _on_mqtt_disconnect(self, client, userdata, rc):
        """Callback cuando se desconecta del broker MQTT"""
        print(f"Disconnected from MQTT broker with code: {rc}")
        with self.lock:
            self.connected = False
        
        # Intentar reconectar
        if rc != 0:
            threading.Thread(target=self.start_mqtt_client, daemon=True).start()
            
    def send_mqtt_message(self, topic, payload):
        """
        Envía un mensaje MQTT al broker
        
        Args:
            topic: Tema MQTT
            payload: Contenido del mensaje
            
        Returns:
            True si se envió correctamente, False en caso contrario
        """
        with self.lock:
            if not self.connected:
                self.retry_queue.append((topic, payload))
                print(f"MQTT not connected. Message queued for later delivery.")
                return False
        
        try:
            result = self.mqtt_client.publish(topic, payload, qos=1)
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                print(f"Failed to publish MQTT message: {result.rc}")
                with self.lock:
                    self.retry_queue.append((topic, payload))
                return False
            return True
        except Exception as e:
            print(f"Error publishing MQTT message: {e}")
            with self.lock:
                self.retry_queue.append((topic, payload))
            return False
            
    def send_video_to_server(self, video_path):
        """
        Envía un archivo de video al servidor web
        
        Args:
            video_path: Ruta al archivo de video
            
        Returns:
            True si se envió correctamente, False en caso contrario
        """
        if not os.path.exists(video_path):
            print(f"Video file not found: {video_path}")
            return False
            
        try:
            with open(video_path, 'rb') as video_file:
                files = {'video': video_file}
                response = requests.post(
                    WEB_SERVER_URL,
                    files=files,
                    auth=(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD),
                    timeout=60  # 60 segundos de timeout para subidas grandes
                )
                
                if response.status_code == 200:
                    print(f"Video uploaded successfully: {video_path}")
                    return True
                else:
                    print(f"Failed to upload video: {response.status_code} - {response.text}")
                    return False
        except Exception as e:
            print(f"Error uploading video: {e}")
            return False
            
    def send_detection(self, detection_json, video_path):
        """
        Envía datos de detección por MQTT y video al servidor web
        
        Args:
            detection_json: JSON con datos de detección
            video_path: Ruta al archivo de video
            
        Returns:
            True si ambos envíos fueron exitosos
        """
        mqtt_success = self.send_mqtt_message(MQTT_TOPIC, detection_json)
        
        # Iniciar subida de video en un hilo separado para no bloquear
        upload_thread = threading.Thread(
            target=self.send_video_to_server,
            args=(video_path,)
        )
        upload_thread.daemon = True
        upload_thread.start()
        
        return mqtt_success
```

### 5.6 main.py - Programa principal

```python
#!/usr/bin/env python3
import time
import cv2
import signal
import sys
import os
from datetime import datetime
from config import MIN_DETECTION_INTERVAL

from camera import Camera
from detector import ButterflyDetector
from processor import DetectionProcessor
from communication import CommunicationManager

# Variable global para modo headless (sin interfaz gráfica)
HEADLESS_MODE = True  # Cambiar a False si desea habilitar visualización

def handle_exit(signum, frame):
    """Manejador de señales para salida limpia"""
    print("Exiting application...")
    if 'camera' in globals():
        camera.stop()
    sys.exit(0)

# Registrar manejadores de señales
signal.signal(signal.SIGINT, handle_exit)
signal.signal(signal.SIGTERM, handle_exit)

def main():
    print("Starting Butterfly Detection System...")
    
    # Inicializar componentes
    camera = Camera()
    detector = ButterflyDetector()
    processor = DetectionProcessor()
    comms = CommunicationManager()
    
    # Iniciar captura de cámara
    camera.start()
    print("Camera started")
    
    last_detection_time = 0
    
    print("System running - waiting for butterflies...")
    
    try:
        while True:
            # Obtener frame de la cámara
            frame = camera.get_frame()
            
            if frame is None:
                time.sleep(0.1)
                continue
                
            # Ejecutar detección
            boxes, confidences, indices, class_ids = detector.detect(frame)
            
            # Si hay detecciones y ha pasado suficiente tiempo desde la última
            current_time = time.time()
            if len(indices) > 0 and (current_time - last_detection_time) > MIN_DETECTION_INTERVAL:
                print(f"Detected {len(indices)} butterflies!")
                last_detection_time = current_time
                
                # Iniciar grabación de video
                video_path = camera.start_recording()
                
                # Crear JSON con datos de detección
                detection_json, image_path = processor.create_detection_json(
                    frame, boxes, confidences, indices, video_path
                )
                
                # Crear imagen con detecciones (solo para logging, no para mostrar)
                annotated_frame = detector.annotate_frame(frame, boxes, confidences, indices, class_ids)
                
                # Guardar imagen anotada para verificación
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                debug_img_path = f"debug_detection_{timestamp}.jpg"
                cv2.imwrite(debug_img_path, annotated_frame)
                print(f"Debug image saved to {debug_img_path}")
                
                # Enviar datos por MQTT (el video se enviará cuando termine la grabación)
                comms.send_detection(detection_json, video_path)
                
                print(f"Detection processed - JSON sent via MQTT, video recording to: {video_path}")
                
            # Visualización (solo si no estamos en modo headless)
            if not HEADLESS_MODE:
                try:
                    cv2.imshow("Butterfly Detection", frame)
                    if cv2.waitKey(1) == ord('q'):
                        break
                except Exception as e:
                    print(f"Error en visualización: {e}")
                    print("Cambiando a modo headless")
                    HEADLESS_MODE = True
                
            # Pequeña pausa para no saturar la CPU
            time.sleep(0.01)
            
    except KeyboardInterrupt:
        print("Program interrupted by user")
    finally:
        camera.stop()
        if not HEADLESS_MODE:
            try:
                cv2.destroyAllWindows()
            except:
                pass
        print("System stopped")

if __name__ == "__main__":
    main()
```

### 5.7 Servicio Systemd para inicio automático

Crearemos un servicio systemd para que el sistema se inicie automáticamente cuando arranca la Orange Pi:

```bash
sudo nano /etc/systemd/system/butterfly-detector.service
```

Contenido del archivo:

```
[Unit]
Description=Butterfly Detection System
After=network.target

[Service]
User=root
WorkingDirectory=/home/orangepi/butterfly_detection
ExecStart=/usr/bin/python3 /home/orangepi/butterfly_detection/script_headless.py
Restart=always
RestartSec=10
StandardOutput=append:/var/log/butterfly-detector.log
StandardError=append:/var/log/butterfly-detector-error.log

[Install]
WantedBy=multi-target.target
```

Habilitar e iniciar el servicio:

```bash
sudo systemctl enable butterfly-detector.service
sudo systemctl start butterfly-detector.service
```

Para verificar el estado del servicio:

```bash
sudo systemctl status butterfly-detector.service
```

Para ver los registros en tiempo real:

```bash
sudo journalctl -u butterfly-detector.service -f
```

## 6. Estructura del JSON generado

El sistema genera un JSON con esta estructura cuando detecta mariposas:

```json
{
  "timestamp": "2025-05-20T15:30:45.123456",
  "butterfly_id": "550e8400-e29b-41d4-a716-446655440000",
  "num_butterflies": 2,
  "image": {
    "width": 640,
    "height": 480,
    "format": "jpg",
    "data": "base64_encoded_image_data..."
  },
  "bounding_boxes": [
    {
      "x": 120,
      "y": 80,
      "width": 60,
      "height": 40,
      "confidence": 0.86
    },
    {
      "x": 320,
      "y": 160,
      "width": 70,
      "height": 50,
      "confidence": 0.78
    }
  ],
  "video_path": "videos/butterfly_20250520_153045.mp4"
}
```

## 7. Consideraciones de despliegue

### 7.1 Optimización de rendimiento

- Ajustar la resolución de la cámara según sea necesario (balance entre rendimiento y precisión)
- Optimizar el modelo de detección para dispositivos embebidos
- Considerar el uso de aceleración hardware si está disponible

### 7.2 Manejo de almacenamiento

- Implementar rotación de archivos para evitar llenar el almacenamiento
- Eliminar videos antiguos después de un período de tiempo configurable

### 7.3 Configuración de red

- Asegurar que la Orange Pi tenga una conexión estable a la red
- Configurar correctamente el broker MQTT y el servidor web
- Implementar reintentos para envíos fallidos

### 7.4 Monitoreo del sistema

- Implementar registro de eventos y errores
- Considerar un sistema de monitoreo remoto para verificar el estado del dispositivo
