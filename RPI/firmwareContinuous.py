# pip install torch --no-cache-dir

import os
import time
import random
import json
from datetime import datetime
import base64
from paho.mqtt import client as mqtt_client
from math import ceil

import serial
import RPi.GPIO as GPIO

# ======================= 

import cv2
import time
import json
import os
import gc
import psutil
import signal
from datetime import datetime
import paho.mqtt.client as mqtt
from ultralytics import YOLO
import subprocess
import socket
from threading import Timer
from contextlib import contextmanager
import tempfile
import shutil


MAX_MEMORY_PERCENT = 80  # % máximo de RAM
MAX_DISK_PERCENT = 90    # % máximo de disco
MAX_PROCESS_TIME = 45    # segundos máximo por operación
MIN_FREE_SPACE_GB = 1    # GB mínimo libre en disco
MAX_TEMP_FILES = 4      # máximo archivos temporales

# Configuración de cámara optimizada
CAMERA_WIDTH = 1920      # Reducido de 4928
CAMERA_HEIGHT = 1080     # Reducido de 3264
CAMERA_FPS = 2           # Reducido de 5
FFMPEG_TIMEOUT = 15      # Timeout para ffmpeg

# Configuración inicial por defecto
DEFAULT_CONFIG = {
    "MQTT_BROKER" : "24.199.125.52",
    "MQTT_PORT" : 1883,
    "MQTT_TOPIC" : "jhpOandG/data/trapViewImage",
    "MQTT_KEEPALIVE" : 60,
    "IMAGE_FOLDER" : "detected_butterflies",
    "LOG_FILE" : "detecciones.json",
    "MODEL_PATH" : "best.pt",
    "CHECK_INTERVAL" : 10, # segundos
    "SHUTDOWN_AFTER" : 600,  # segundos (1 hora) 3000
    "CONFIDENCE_THRESHOLD": 0.45,
    "NMS_THRESHOLD": 0.4,
    "TARGET_CLASS": 0
}

# Inicialización
os.makedirs(DEFAULT_CONFIG["IMAGE_FOLDER"], exist_ok=True)
start_time = time.time()

# =====================

GPIO.setmode(GPIO.BOARD)

GPIO.setwarnings(False)

# SETTING PINS
PIN = 40
PINL = 22
PIN_RPI_STATUS = 18
PIN_RPI_MANAGER = 16
PIN_ESP32_MANAGER = 15

GPIO.setup(PIN, GPIO.IN, pull_up_down = GPIO.PUD_UP)
GPIO.setup(PINL, GPIO.OUT)
GPIO.setup(PIN_RPI_STATUS, GPIO.OUT)
GPIO.setup(PIN_RPI_MANAGER, GPIO.IN, pull_up_down = GPIO.PUD_UP)
GPIO.setup(PIN_ESP32_MANAGER, GPIO.OUT)

# SETTING GENERAL
#server = "24.199.125.52"
port = 60 # puertos
resolution = "0" # resolucion
saveImage = False
runningNN = False
chunkSize = 8192

#broker = "broker.emqx.io" #
#broker = 'broker.hivemq.com' #broker.hivemq.com
broker = "24.199.125.52"
port = 1883

topicPub = "jhpOandG/data/trapViewImage"
topicSub = "jhpOandG/settings"
client_id = f'publish-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'

client_name = "trapView"
mac = "30:AE:A4:CA:D2:04"

# ============================ FUNCTIONS =========================
class TimeoutException(Exception):
    pass


@contextmanager
def timeout_context(seconds):
    """Context manager para timeout de operaciones"""
    def timeout_handler(signum, frame):
        raise TimeoutException(f"Operación excedió {seconds} segundos")
    
    # Configurar señal de timeout
    old_handler = signal.signal(signal.SIGALRM, timeout_handler)
    signal.alarm(seconds)
    
    try:
        yield
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old_handler)

class SystemMonitor:
    """Monitor de recursos del sistema"""
    
    @staticmethod
    def check_memory():
        """Verifica uso de memoria"""
        memory = psutil.virtual_memory()
        if memory.percent > MAX_MEMORY_PERCENT:
            print(f"⚠️ Memoria alta: {memory.percent:.1f}%")
            return False
        return True
    
    @staticmethod
    def check_disk_space():
        """Verifica espacio en disco"""
        disk = psutil.disk_usage('.')
        free_gb = disk.free / (1024**3)
        if free_gb < MIN_FREE_SPACE_GB:
            print(f"⚠️ Poco espacio en disco: {free_gb:.1f}GB")
            return False
        return True
    
    @staticmethod
    def cleanup_temp_files():
        """Limpia archivos temporales antiguos"""
        try:
            temp_pattern = "temp_capture"
            files_removed = 0
            for file in os.listdir('.'):
                if temp_pattern in file:
                    try:
                        os.remove(file)
                        files_removed += 1
                    except:
                        pass
            if files_removed > 0:
                print(f"🧹 Limpiados {files_removed} archivos temporales")
        except Exception as e:
            print(f"⚠️ Error limpiando archivos: {str(e)}")
    
    @staticmethod
    def force_gc():
        """Fuerza recolección de basura"""
        gc.collect()

class MQTTManager:
    def __init__(self):
        self.client = mqtt.Client()
        self.connected = False
        self.setup_callbacks()
        
    def setup_callbacks(self):
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("✅ Conexión MQTT establecida con éxito")
            self.connected = True
        else:
            print(f"❌ Error de conexión MQTT. Código: {rc}")
            self.connected = False
            
    def on_disconnect(self, client, userdata, rc):
        print(f"⚠️ Desconectado del broker MQTT. Código: {rc}")
        self.connected = False
        
    def connect(self):
        try:
            print(f"🔗 Conectando a MQTT broker ({DEFAULT_CONFIG['MQTT_BROKER']}:{DEFAULT_CONFIG['MQTT_PORT']})...")
            self.client.connect(DEFAULT_CONFIG["MQTT_BROKER"], DEFAULT_CONFIG["MQTT_PORT"], DEFAULT_CONFIG["MQTT_KEEPALIVE"])
            self.client.loop_start()
            time.sleep(2)  # Dar tiempo para conectar
        except socket.gaierror:
            print("❌ Error: No se puede resolver el nombre del broker")
        except Exception as e:
            print(f"❌ Error inesperado al conectar: {str(e)}")
            
    def reconnect(self):
        """Reconexión simplificada"""
        try:
            if not self.connected:
                self.client.reconnect()
                time.sleep(2)
        except Exception as e:
            print(f"❌ Error en reconexión: {str(e)}")
            
    def publish(self, topic, message):
        if not self.connected:
            self.reconnect()
            
        if self.connected:
            try:
                info = self.client.publish(topic, message)
                if info.rc == mqtt.MQTT_ERR_SUCCESS:
                    print(f"📤 Mensaje publicado en {topic}")
                    return True
                else:
                    print(f"❌ Error al publicar. Código: {info.rc}")
                    return False
            except Exception as e:
                print(f"❌ Error al publicar mensaje: {str(e)}")
                self.connected = False
                return False
        return False
            
    def disconnect(self):
        try:
            self.client.loop_stop()
            if self.connected:
                self.client.disconnect()
                print("🔌 Desconectado del broker MQTT")
        except:
            pass

class CameraManager:
    """Manejo robusto de cámara"""
    
    def __init__(self):
        self.available_devices = self.find_cameras()
        self.current_device = None
        
    def find_cameras(self):
        """Encuentra cámaras disponibles"""
        devices = []
        for i in range(4):  # Buscar hasta 4 dispositivos
            device = f"/dev/video{i}"
            if os.path.exists(device):
                devices.append(device)
        print(f"📷 Cámaras encontradas: {devices}")
        return devices
    
    def capture_image_safe(self):
        """Captura imagen con timeout y manejo de errores"""
        if not self.available_devices:
            print("❌ No hay cámaras disponibles")
            return None
            
        timestamp = int(time.time())
        #temp_image = f"temp_capture_{timestamp}.jpg"
        temp_image = "image.jpg"
        
        for device in self.available_devices:
            try:
                print(f"📸 Intentando capturar con {device}")

                #os.system(f"rpicam-jpeg --output {name}.jpg --timeout 2000 --autofocus-range macro --autofocus-speed fast --autofocus-window 0.25,0.25,0.5,0.5 --hdr off --denoise auto --roi 0.27,0.25,0.45,1 --width 2400 --height 3000")
                
                command = [
                    "rpicam-jpeg",
                    "--output", f"{temp_image}",
                    "--timeout", "2000",
                    "--autofocus-range", "macro",
                    "--autofocus-speed", "fast",
                    "--autofocus-window", "0.25,0.25,0.5,0.5",
                    "--hdr", "off",
                    "--denoise", "auto",
                    "--roi", "0.27,0.25,0.45,1",
                    "--width", "1080",
                    "--height", "720"
                ]

                #command = [
                #    "ffmpeg", "-y",
                #    "-f", "v4l2",
                #    "-video_size", f"{CAMERA_WIDTH}x{CAMERA_HEIGHT}",
                #    "-framerate", str(CAMERA_FPS),
                #    "-input_format", "mjpeg",
                #    "-i", device,
                #    "-frames:v", "1",
                #    "-ss", "8",  # Reducido de 5 segundos
                #    "-loglevel", "error",  # Reduce logs de ffmpeg
                #    temp_image
                #]
                
                # Ejecutar con timeout
                with timeout_context(FFMPEG_TIMEOUT):
                    process = subprocess.run(
                        command,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                        text=True,
                        timeout=FFMPEG_TIMEOUT
                    )
                
                if process.returncode == 0 and os.path.exists(temp_image):
                    # Verificar que la imagen sea válida
                    if os.path.getsize(temp_image) > 1000:  # Al menos 1KB
                        self.current_device = device
                        return temp_image
                    else:
                        os.remove(temp_image)
                        
            except TimeoutException:
                print(f"⏱️ Timeout en captura con {device}")
                self.kill_ffmpeg_processes()
            except subprocess.TimeoutExpired:
                print(f"⏱️ FFmpeg timeout con {device}")
                self.kill_ffmpeg_processes()
            except Exception as e:
                print(f"❌ Error con {device}: {str(e)[:100]}")
                
            # Limpiar archivos fallidos
            if os.path.exists(temp_image):
                try:
                    os.remove(temp_image)
                except:
                    pass
                    
        return None
    
    @staticmethod
    def kill_ffmpeg_processes():
        """Mata procesos ffmpeg colgados"""
        try:
            subprocess.run(["pkill", "-f", "ffmpeg"], 
                         stdout=subprocess.DEVNULL, 
                         stderr=subprocess.DEVNULL)
        except:
            pass

class YOLOManager:
    """Manejo optimizado de YOLO"""
    
    def __init__(self):
        self.model = None
        self.load_model()
    
    def load_model(self):
        """Carga modelo YOLO con manejo de errores"""
        try:
            if os.path.exists(DEFAULT_CONFIG["MODEL_PATH"]):
                print(f"📦 Cargando modelo personalizado: {DEFAULT_CONFIG['MODEL_PATH']}")
                self.model = YOLO(DEFAULT_CONFIG["MODEL_PATH"])
                self.model.fuse()
            else:
                print("📦 Cargando modelo YOLOv8n por defecto")
                self.model = YOLO('yolov8n.pt')
            print("✅ Modelo YOLO cargado correctamente")
        except Exception as e:
            print(f"❌ Error cargando modelo YOLO: {str(e)}")
            self.model = None
    
    
    def detect_safe(self, image_path):
        """Detección con manejo correcto de tipos de datos"""
        if not self.model:
            print("❌ Modelo YOLO no disponible")
            return []
            
        try:
            with timeout_context(MAX_PROCESS_TIME):
                results = self.model(
                    image_path,
                    verbose=True
                )
                
                detections = []
                target_class = DEFAULT_CONFIG["TARGET_CLASS"]
                conf_threshold = DEFAULT_CONFIG["CONFIDENCE_THRESHOLD"]
                                
                for result in results:
                    if result.boxes is not None:
                        
                        for box in result.boxes:
                            # CONVERSIÓN CORRECTA DE TIPOS
                            cls_value = int(box.cls.item())  # Convertir tensor a int
                            conf_value = float(box.conf.item())  # Convertir tensor a float
                            
                            # COMPARACIÓN CORRECTA
                            if cls_value == target_class and conf_value >= conf_threshold:
                                x1, y1, x2, y2 = map(int, box.xyxy[0].tolist())
                                
                                detection = {
                                    "score": conf_value,
                                    "box": [x1, y1, x2, y2],
                                    "cls": cls_value
                                }
                                detections.append(detection)
                            else:
                                if cls_value != target_class:
                                    print(f"❌ Clase incorrecta: {cls_value} != {target_class}")
                                if conf_value < conf_threshold:
                                    print(f"❌ Confianza baja: {conf_value:.3f} < {conf_threshold}")
                    else:
                        print("❌ No hay boxes en el resultado")
                
                # Limpiar memoria después de detección  
                del results
                SystemMonitor.force_gc()
                
                print(f"🎯 TOTAL DETECCIONES VÁLIDAS: {len(detections)}")
                return detections
                
        except TimeoutException:
            print("⏱️ Timeout en detección YOLO")
            return []
        except Exception as e:
            print(f"❌ Error en detección: {str(e)}")
            import traceback
            traceback.print_exc()
            return []

# Inicializar componentes
mqtt_manager = MQTTManager()
camera_manager = CameraManager()
yolo_manager = YOLOManager()

def draw_boxes_safe(image_path, detections):
    """Dibuja bounding boxes - VERSIÓN CORREGIDA para diccionarios"""
    try:
        image = cv2.imread(image_path)
        if image is None:
            print(f"❌ No se pudo leer imagen: {image_path}")
            return None
        
        print(f"🎨 Dibujando {len(detections)} detecciones")
        
        for i, detection in enumerate(detections):
            #print(f"🎨 Procesando detección {i}: {detection}")
            
            # NUEVO: Manejar formato de diccionario
            if isinstance(detection, dict):
                # Formato: {"score": 0.85, "box": [x1, y1, x2, y2], "class": 0}
                x1, y1, x2, y2 = detection["box"]
                confidence = detection["score"]
                class_id = detection["cls"]
                
                # Obtener nombre de clase
                if yolo_manager.model and hasattr(yolo_manager.model, 'names'):
                    class_name = yolo_manager.model.names.get(class_id, f"Class_{class_id}")
                else:
                    class_name = f"Class_{class_id}"
                    
            else:
                # Formato original de YOLO box object (por compatibilidad)
                x1, y1, x2, y2 = map(int, detection.xyxy[0].tolist())
                confidence = float(detection.conf)
                class_id = int(detection.cls)
                
                if yolo_manager.model:
                    class_name = yolo_manager.model.names[class_id]
                else:
                    class_name = f"Class_{class_id}"
            
            # Dibujar rectángulo
            cv2.rectangle(image, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            # Añadir etiqueta
            label = f"{class_name} {confidence:.2f}"
            
            # Calcular tamaño del texto
            font = cv2.FONT_HERSHEY_SIMPLEX
            font_scale = 0.6
            font_thickness = 2
            (text_width, text_height), baseline = cv2.getTextSize(label, font, font_scale, font_thickness)
            
            # Dibujar fondo para el texto
            cv2.rectangle(image, (x1, y1 - text_height - 10), (x1 + text_width, y1), (0, 255, 0), -1)
            
            # Dibujar texto
            cv2.putText(image, label, (x1, y1 - 5), font, font_scale, (0, 0, 0), font_thickness)
            
            #print(f"✅ Box dibujado: {label} en ({x1},{y1})-({x2},{y2})")
        
        return image
        
    except Exception as e:
        print(f"❌ Error dibujando boxes: {str(e)}")
        import traceback
        traceback.print_exc()
        return None

def save_detection_safe(original_image_path, detections):
    """Guarda detección con validaciones"""
    try:
        if not SystemMonitor.check_disk_space():
            print("⚠️ Poco espacio en disco, saltando guardado")
            return None
            
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        original_filename = f"{DEFAULT_CONFIG['IMAGE_FOLDER']}/original_{timestamp}.jpg"
        processed_filename = f"{DEFAULT_CONFIG['IMAGE_FOLDER']}/processed_{timestamp}.jpg"
        
        # Mover imagen original
        shutil.move(original_image_path, original_filename)
        
        # Procesar imagen
        annotated_image = draw_boxes_safe(original_filename, detections)
        if annotated_image is not None:
            cv2.imwrite(processed_filename, annotated_image)
            del annotated_image  # Liberar memoria
        else:
            processed_filename = None
        
        # Log entry
        log_entry = {
            "timestamp": datetime.now().isoformat(),
            "count": len(detections),
            "species": [int(det["cls"]) for det in detections],
            "confidence": [float(det["score"]) for det in detections],
            "original_image": original_filename,
            "processed_image": processed_filename
        }
        
        # Guardar log
        with open(DEFAULT_CONFIG["LOG_FILE"], "a") as f:
            json.dump(log_entry, f)
            f.write("\n")
        
        return log_entry
        
    except Exception as e:
        print(f"❌ Error guardando detección: {str(e)}")
        return None

def process_frame_safe():
    """Procesa frame con todos los checks de seguridad"""
    try:
        # Verificar recursos del sistema
        if not SystemMonitor.check_memory():
            print("⚠️ Memoria insuficiente, saltando procesamiento")
            SystemMonitor.force_gc()
            return
            
        # Limpiar archivos temporales
        SystemMonitor.cleanup_temp_files()
        
        # Capturar imagen
        print("📸 Capturando imagen...")
        image_path = camera_manager.capture_image_safe()
        if not image_path:
            print("❌ No se pudo capturar imagen")
            return
            
        print("✅ Imagen capturada correctamente")
        
        # Detectar objetos
        print("🔍 Procesando detección...")
        detections = yolo_manager.detect_safe(image_path)
        
        if detections:
            print(f"🦋 Detecciones encontradas: {len(detections)}")

            # Guardar resultados
            log_entry = save_detection_safe(image_path, detections)
            if log_entry:
                # Enviar MQTT
                """mqtt_msg = {
                                                                    "timestamp": log_entry["timestamp"],
                                                                    "count": log_entry["count"],
                                                                    "confidence": log_entry["confidence"],
                                                                    "device": camera_manager.current_device
                                                                }"""

                #print(log_entry)

                publishMsgCont(log_entry["processed_image"],log_entry["count"])

                #mqtt_manager.publish(DEFAULT_CONFIG["MQTT_TOPIC"], json.dumps(mqtt_msg))
        else:
            print("🔍 No se detectaron mariposas")
            # Eliminar imagen temporal
            #if os.path.exists(image_path):
            #    os.remove(image_path)
                
    except Exception as e:
        print(f"❌ Error crítico en process_frame: {str(e)}")
        SystemMonitor.force_gc()

def check_shutdown():
    """Verifica condiciones de apagado"""
    elapsed = time.time() - start_time
    
    # Check por tiempo
    if elapsed >= DEFAULT_CONFIG["SHUTDOWN_AFTER"]:
        print("⏰ Tiempo completado. Apagando...")
        shutdown_system("Tiempo completado")
        return True
    
    # Check por recursos críticos
    if not SystemMonitor.check_memory() or not SystemMonitor.check_disk_space():
        print("🚨 Recursos críticos. Apagando...")
        shutdown_system("Recursos críticos")
        return True
        
    return False

def shutdown_system(reason):
    """Apagado limpio del sistema"""
    try:
        #mqtt_manager.publish(DEFAULT_CONFIG["MQTT_TOPIC"], json.dumps({
        #    "status": "shutdown",
        #    "reason": reason,
        #    "uptime": time.time() - start_time
        #}))
        time.sleep(2)  # Dar tiempo para enviar mensaje
    except:
        pass
    
    mqtt_manager.disconnect()
    camera_manager.kill_ffmpeg_processes()
    SystemMonitor.cleanup_temp_files()
    print("✅ Apagado limpio completado")


# ================================= end functions =========================
def connectWifi():
    try:
        ssid = "MIFI_8D3D"
        pw = "1234567890"

        GPIO.output(PIN_ESP32_MANAGER, GPIO.HIGH)
        time.sleep(0.5)
        GPIO.output(PIN_ESP32_MANAGER, GPIO.LOW)
        time.sleep(2)

        ser = serial.Serial('/dev/ttyS0', 115200, timeout=2)
        ser.flush()
        payload = {"msg":"wifi"}
        msg = json.dumps(payload)
        print("msg esp32", payload)
        string = msg + "\n" #"\n" for line seperation
        string = string.encode('utf_8')
        ser.write(string) # sending over UART

        line = ""

        count = 0
        while True:
            line = ser.readline().decode('utf-8').rstrip()
            print("esp32 received: ",line)
            if len(line) >= 5:
                msgIn = json.loads(line)
                ssid = msgIn["ssid"]
                pw = msgIn["pw"]
                #return msgIn['mac'], msgIn['voltage'], msgIn['T'], msgIn['H']
                break
            if count >= 5:
                return "error"
                #return {"0",0,0,0}
                #break
            count = count + 1
        ser.close()

        
        os.system("sudo iw wlan0 scan | grep SSID")
        time.sleep(1)
        os.system(f'sudo nmcli dev wifi connect "{ssid}" password "{pw}"')

    except Exception as ex:
        print("wifi err", type(ex))

def blink(n):
    print("blink",n)
    for i in range(n):
        GPIO.output(PINL, GPIO.HIGH)
        time.sleep(0.4)
        GPIO.output(PINL, GPIO.LOW)
        time.sleep(0.2)
    time.sleep(2)

def takeVideo(name):
    try:
        os.system(f"libcamera-vid --level 4.2 --framerate 2 --width 1280 --height 720 -o {name}.h264 -t 90000 --autofocus-mode auto -n")
        
        #os.system(f"libcamera-still -o {name}.jpg")
        #os.system(f"ffmpeg -i {name}.h264 {name}.mp4")

        print("Image captured")
        #os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
        os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp  -o 'StrictHostKeyChecking=no' {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
        os.system(f"mv {name}.h264 imgs")

    except Exception as ex:
        print("takeVideo err", type(ex))

def takePicture(name, resolution):
    try:
        if resolution == "0":
            os.system(f"rpicam-jpeg --output {name}.jpg --timeout 2000 --autofocus-range macro --autofocus-speed fast --autofocus-window 0.25,0.25,0.5,0.5 --hdr off --denoise auto --roi 0.27,0.25,0.45,1 --width 1200 --height 1500")
        else:
            os.system(f"rpicam-jpeg --output {name}.jpg --timeout 2000 --autofocus-range macro --autofocus-speed fast --autofocus-window 0.25,0.25,0.5,0.5 --hdr off --denoise auto --roi 0.27,0.25,0.45,1 --width 2400 --height 3000")
        os.system(f"mv {name}.jpg imgs")
    except Exception as ex:
        print("takePicture err", type(ex))

def runCNN():
    try:
        payload = {"0":3, "4":1}
        return True, payload
    except Exception as ex:
        print("takePicture err", type(ex))

def connect_mqtt():
    try:
        def on_connect(client, userdata, flags, rc, props=None):
            if rc == 0:
                print("Connected to MQTT Broker!")
            else:
                print("Failed to connect, return code %d\n", rc)

        client = mqtt_client.Client(client_id=client_id, callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2)
        # client.username_pw_set(username, password)
        client.on_connect = on_connect
        client.connect(broker, port)
        return client

    except Exception as ex:
        print("connect_mqtt err", type(ex))

def connectESP(msgOut,response):
    try:
        GPIO.output(PIN_ESP32_MANAGER, GPIO.HIGH)
        time.sleep(0.5)
        GPIO.output(PIN_ESP32_MANAGER, GPIO.LOW)
        time.sleep(1)

        ser = serial.Serial('/dev/ttyS0', 115200, timeout=2)
        ser.flush()
        #payload = {"msg":msgOut,"timesleep":timesleep}
        payload = {"msg":msgOut}
        msg = json.dumps(payload)
        print("msg esp32", payload)
        string = msg + "\n" #"\n" for line seperation
        string = string.encode('utf_8')
        ser.write(string) # sending over UART
        ser.flush()
        line = ""

        count = 0
        while response:
            line = ser.readline().decode('utf-8').rstrip()
            print("esp32 received: ",line)
            if len(line) >= 5:
                msgIn = json.loads(line)
                ser.close()
                return msgIn
                #return msgIn['mac'], msgIn['voltage'], msgIn['T'], msgIn['H']
                #break
            if count >= 5:
                ser.close()
                return json.loads({})
                #return {"0",0,0,0}
                #break
            count = count + 1

    except Exception as ex:
        print("connectESP err", type(ex))
    
def shutDown():
    try:
        if GPIO.input(PIN):
            #connectESP("sleep",False)
            print("apagado...")
            blink(10)
            os.system('sudo shutdown -h now')

    except Exception as ex:
        print("shutDown err", type(ex))

def subscribe(client: mqtt_client):
    try:
        def on_message(client, userdata, msg):
            print("settings recieved")

            try:
                topic_in = str(msg.topic)
                data_in = str(msg.payload.decode("utf-8"))

                print(data_in)
                m_mqtt = json.loads(data_in)
                name = m_mqtt.get("name",False)

                # 3. READ SETTINGS
                if name == client_name:
                    global timesleep, status, continuous, refresh
                    timesleep = int(m_mqtt.get("timesleep",60))
                    status = m_mqtt.get("status",True)
                    continuous = m_mqtt.get("continuous",True)
                    refresh = m_mqtt.get("refresh",10)
                    print("new timesleep ",continuous, type(continuous), type(True))

            except Exception as e:
                print('Arrival msg error..... ', e)

                blink(10)

        client.subscribe(topicSub)
        client.on_message = on_message

    except Exception as ex:
        print("subscribe err", type(ex))

def publishMsgCont(file_name,payloadNN):

    try:
        # prepare data
        payloadESP32 = connectESP("data",True)


        client = connect_mqtt()
        client.loop_start()
        #subscribe(client)

        with open(file_name, "rb") as f:
            encoded_image = base64.b64encode(f.read())

        payload = {}
        payload["DeviceMacAddress"] = payloadESP32.get("mac",0)
        #payload["name"] = client_name
        payload["Temperature"] = payloadESP32.get("T",0)
        payload["Humidity"] = payloadESP32.get("H",0)
        payload["VoltageBattery"] =payloadESP32.get("B",0)
        
        payload["nDetected"] = payloadNN
        payload["img64"] = encoded_image.decode('utf-8')

        msg = json.dumps(payload)

        msg = msg.encode('utf-8')

        total_chunks = ceil(len(msg) / chunkSize)

        for i in range(total_chunks):
            start_pos = i * chunkSize
            end_pos = start_pos + chunkSize
            chunk = msg[start_pos:end_pos]

            topic = """{0}/{1}/{2}/{3}""".format(topicPub,payloadESP32.get("mac",0),total_chunks,i)

            client.publish(
                topic,
                chunk,
                qos = 2
            )
            time.sleep(0.1)
        
        # send message
        #result = client.publish(topicPub, msg)
        #time.sleep(1)
        blink(3)
        client.loop_stop()

        shutDown()

    except Exception as ex:
        print("publishMsg err", type(ex))

def publishMsg(file_name,payloadNN):

    try:
        # prepare data
        payloadESP32 = connectESP("data",True)


        client = connect_mqtt()
        client.loop_start()
        #subscribe(client)

        with open(f"imgs/{file_name}.jpg", "rb") as f:
            encoded_image = base64.b64encode(f.read())

        payload = {}
        payload["DeviceMacAddress"] = payloadESP32.get("mac",0)
        #payload["name"] = client_name
        payload["Temperature"] = payloadESP32.get("T",0)
        payload["Humidity"] = payloadESP32.get("H",0)
        payload["VoltageBattery"] =payloadESP32.get("B",0)
        
        payload["nDetected"] = payloadNN
        payload["img64"] = encoded_image.decode('utf-8')

        msg = json.dumps(payload)

        msg = msg.encode('utf-8')

        total_chunks = ceil(len(msg) / chunkSize)

        for i in range(total_chunks):
            start_pos = i * chunkSize
            end_pos = start_pos + chunkSize
            chunk = msg[start_pos:end_pos]

            topic = """{0}/{1}/{2}/{3}""".format(topicPub,payloadESP32.get("mac",0),total_chunks,i)

            client.publish(
                topic,
                chunk,
                qos = 2
            )
            time.sleep(0.1)
        
        # send message
        #result = client.publish(topicPub, msg)
        #time.sleep(1)
        blink(3)
        client.loop_stop()

        shutDown()

    except Exception as ex:
        print("publishMsg err", type(ex))

def continueMode():
    print("\n=== Sistema de Detección de Mariposas (Versión Estable) ===")
    print(f"🔧 Configuración:")
    print(f" - Resolución: {CAMERA_WIDTH}x{CAMERA_HEIGHT}")
    print(f" - Intervalo: {DEFAULT_CONFIG['CHECK_INTERVAL']}s")
    print(f" - Timeout operaciones: {MAX_PROCESS_TIME}s")
    print(f" - Memoria máxima: {MAX_MEMORY_PERCENT}%")
    
    # Conectar MQTT
    mqtt_manager.connect()
    
    # Verificar componentes
    if not yolo_manager.model:
        print("❌ No se pudo cargar modelo YOLO")
        return
        
    if not camera_manager.available_devices:
        print("❌ No hay cámaras disponibles")
        return
    
    cycle_count = 0
    try:
        while True:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            cycle_count += 1
            print(f"\n--- Ciclo {cycle_count} {timestamp} ---")
            
            # Procesar frame
            process_frame_safe()
            
            # Verificar apagado
            if check_shutdown():
                break
                
            # Pausa entre ciclos
            print(f"😴 Esperando {DEFAULT_CONFIG['CHECK_INTERVAL']}s...")
            GPIO.output(PINL, GPIO.HIGH)
            time.sleep(DEFAULT_CONFIG["CHECK_INTERVAL"])
            GPIO.output(PINL, GPIO.LOW)
            
            # Limpieza periódica (cada 10 ciclos)
            if cycle_count % 5 == 0:
                print("🧹 Limpieza periódica...")
                SystemMonitor.cleanup_temp_files()
                SystemMonitor.force_gc()
                
    except KeyboardInterrupt:
        print("\n🛑 Detenido por usuario")
        shutdown_system("Detenido manualmente")
    except Exception as e:
        print(f"❌ Error crítico: {str(e)}")
        shutdown_system(f"Error crítico: {str(e)}")
    finally:
        print("🔚 Finalizando sistema...")
        shutDown()

def run():
    try:

        payloadESP32 = connectESP("settings",True)

        global broker, port, resolution, saveImage, runningNN

        saveImage = payloadESP32.get("saveImage",False)
        timeSleep = payloadESP32.get("timeSleep",60)
        runningNN = payloadESP32.get("runningNN",False)
        isContinue = payloadESP32.get("isContinue",False)
        sensibility = payloadESP32.get("sensibility",0.5)
        DEFAULT_CONFIG["CONFIDENCE_THRESHOLD"] = sensibility
        DEFAULT_CONFIG["SHUTDOWN_AFTER"] = int(timeSleep * 60 * 0.9)

        resolution = payloadESP32.get("resolution","0")
        broker = payloadESP32.get("server","24.199.125.52")
        #port = payloadESP32.get("port","80")
        
        print(broker, port, resolution, saveImage, runningNN)

        blink(1)

        if isContinue: #runningNN
            print("continue mode")
            blink(4)
            continueMode()
        else:
            print("NO continue mode")
            now = datetime.now()
            dt_string = now.strftime("%d-%m-%Y_%H-%M-%S")

            print("Date Time: ", dt_string)
            file_name = client_name + "_" + dt_string
            takePicture(file_name, resolution) # get image from camera}

            nDetected = 0
                
            publishMsg(file_name,nDetected)

            blink(4)

            if not saveImage:
                print("Drop image")
                os.system(f"rm imgs/{file_name}.jpg")

            shutDown()

    except Exception as ex:
        print("run err", type(ex))
    finally:
        shutDown()
    
if __name__ == '__main__':
    connectWifi()
    time.sleep(2)
    os.system("sudo ntpdate pool.ntp.org")
    GPIO.output(PINL, GPIO.HIGH)
    time.sleep(2)
    GPIO.output(PINL, GPIO.LOW)
    run()

