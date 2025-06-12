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
