#!/usr/bin/env python3
"""
main_enhanced.py - Sistema principal con configuración remota y buffer de detecciones
"""

import time
import cv2
import signal
import sys
import os
import threading
from datetime import datetime, timedelta
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
try:
    from camera import Camera
    from detector import ButterflyDetector
    from processor import DetectionProcessor
    from communication import CommunicationManager
    from config_manager import ConfigManager
    from detection_buffer import DetectionBuffer
    from config import *
except ImportError as e:
    logger.error(f"Error importando módulos: {e}")
    sys.exit(1)

class ButterflyDetectionSystem:
    def __init__(self):
        self.running = False
        self.start_time = datetime.now()
        
        # Inicializar componentes
        self.camera = None
        self.detector = None
        self.processor = None
        self.comms = None
        self.config_manager = None
        self.detection_buffer = None
        
        # Control de operación
        self.last_detection_time = 0
        self.last_send_time = None
        self.send_thread = None
        
        # Estado del sistema
        self.in_operation_hours = False
        self.config_received = False
        
    def initialize_components(self):
        """Inicializa todos los componentes del sistema"""
        try:
            # Inicializar gestor de configuración
            self.config_manager = ConfigManager(
                MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_CONFIG
            )
            
            # Inicializar buffer de detecciones
            self.detection_buffer = DetectionBuffer()
            
            # Inicializar procesador
            self.processor = DetectionProcessor()
            
            # Inicializar comunicación
            self.comms = CommunicationManager()
            
            logger.info("Core components initialized")
            
            # Esperar configuración inicial o usar valores por defecto
            self.wait_for_initial_config()
            
            # Inicializar cámara y detector con configuración actualizada
            config = self.config_manager.get_config()
            
            # Actualizar resolución de la cámara si es necesario
            global CAMERA_RESOLUTION
            CAMERA_RESOLUTION = tuple(config["operation_resolution"])
            
            self.camera = Camera()
            self.detector = ButterflyDetector()
            
            # Actualizar URL del servidor web
            self.comms.update_web_server_url(config["web_server_url"])
            
            logger.info("All components initialized successfully")
            
        except Exception as e:
            logger.error(f"Error initializing components: {e}")
            raise
    
    def wait_for_initial_config(self):
        """Espera la configuración inicial del servidor"""
        config = self.config_manager.get_config()
        
        if not config["auto_send"]:
            logger.info("Waiting for configuration from server...")
            
            # Esperar hasta 1 hora por configuración
            wait_start = time.time()
            wait_timeout = 3600  # 1 hora
            
            while time.time() - wait_start < wait_timeout:
                if self.config_manager.is_config_updated():
                    logger.info("Configuration received from server")
                    self.config_received = True
                    break
                time.sleep(10)  # Verificar cada 10 segundos
            
            if not self.config_received:
                logger.warning("No configuration received, using default values")
        else:
            logger.info("Using auto-send mode with default configuration")
            self.config_received = True
    
    def check_operation_hours(self):
        """Verifica si estamos en horario de operación"""
        is_operation_time = self.config_manager.is_operation_time()
        
        if is_operation_time != self.in_operation_hours:
            self.in_operation_hours = is_operation_time
            if is_operation_time:
                logger.info("Entering operation hours - starting detection")
                if self.camera:
                    self.camera.start()
            else:
                logger.info("Exiting operation hours - stopping detection")
                if self.camera:
                    # No detenemos completamente la cámara, solo pausamos la detección
                    pass
    
    def process_detection(self, frame, boxes, confidences, indices, class_ids):
        """Procesa una detección y la añade al buffer"""
        try:
            config = self.config_manager.get_config()
            
            # Iniciar grabación de video
            video_path = self.camera.start_recording()
            
            # Crear datos de detección
            detection_data, image_path = self.processor.create_detection_json(
                frame, boxes, confidences, indices, video_path,
                tuple(config["server_image_resolution"])
            )
            
            # Añadir al buffer
            self.detection_buffer.add_detection(detection_data, image_path, video_path)
            
            # Crear imagen anotada para verificación local
            annotated_frame = self.detector.annotate_frame(frame, boxes, confidences, indices, class_ids)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            debug_img_path = f"debug_detection_{timestamp}.jpg"
            cv2.imwrite(debug_img_path, annotated_frame)
            
            logger.info(f"Detection processed and added to buffer. Debug image: {debug_img_path}")
            
        except Exception as e:
            logger.error(f"Error processing detection: {e}")
    
    def should_send_now(self):
        """Determina si es momento de enviar las detecciones del buffer"""
        config = self.config_manager.get_config()
        
        # Si no hay envío automático, no enviar
        if not config["auto_send"]:
            return False
        
        # Calcular próximo tiempo de envío
        next_send_time = self.config_manager.get_next_send_time(self.last_send_time)
        
        return datetime.now() >= next_send_time
    
    def send_buffered_detections(self):
        """Envía todas las detecciones del buffer al servidor"""
        try:
            pending_detections = self.detection_buffer.get_pending_detections()
            
            if not pending_detections:
                logger.info("No pending detections to send")
                return
            
            logger.info(f"Sending {len(pending_detections)} pending detections...")
            
            # Enviar detecciones una por una
            results = self.comms.send_detection_batch(pending_detections)
            
            # Procesar resultados
            for success, timestamp in results:
                if success:
                    self.detection_buffer.mark_as_sent(timestamp)
                else:
                    self.detection_buffer.increment_retry_count(timestamp)
            
            # Actualizar tiempo de último envío
            self.last_send_time = datetime.now()
            
            # Mostrar estadísticas
            stats = self.detection_buffer.get_statistics()
            logger.info(f"Buffer stats: {stats}")
            
        except Exception as e:
            logger.error(f"Error sending buffered detections: {e}")
    
    def detection_loop(self):
        """Bucle principal de detección"""
        logger.info("Starting detection loop...")
        
        while self.running:
            try:
                # Verificar horario de operación
                self.check_operation_hours()
                
                # Procesar configuración actualizada si es necesario
                if self.config_manager.is_config_updated():
                    logger.info("Configuration updated, applying changes...")
                    config = self.config_manager.get_config()
                    
                    # Actualizar parámetros del detector
                    self.detector.update_thresholds(
                        config["confidence_threshold"],
                        config["nms_threshold"]
                    )
                    
                    # Actualizar URL del servidor
                    self.comms.update_web_server_url(config["web_server_url"])
                
                # Solo detectar si estamos en horario de operación
                if not self.in_operation_hours:
                    time.sleep(10)  # Verificar cada 10 segundos
                    continue
                
                # Obtener frame de la cámara
                frame = self.camera.get_frame()
                if frame is None:
                    time.sleep(0.1)
                    continue
                
                # Ejecutar detección
                config = self.config_manager.get_config()
                boxes, confidences, indices, class_ids = self.detector.detect(frame)
                
                # Procesar detecciones si las hay
                current_time = time.time()
                if (len(indices) > 0 and 
                    (current_time - self.last_detection_time) > config["min_detection_interval"]):
                    
                    logger.info(f"Detected {len(indices)} butterflies!")
                    self.last_detection_time = current_time
                    
                    # Procesar y almacenar en buffer
                    self.process_detection(frame, boxes, confidences, indices, class_ids)
                
                # Verificar si es momento de enviar datos
                if self.should_send_now():
                    threading.Thread(
                        target=self.send_buffered_detections,
                        daemon=True
                    ).start()
                
                # Limpieza periódica del buffer
                if datetime.now().minute == 0:  # Una vez por hora
                    self.detection_buffer.cleanup_old_detections()
                
                time.sleep(0.01)  # Pequeña pausa para no saturar la CPU
                
            except Exception as e:
                logger.error(f"Error in detection loop: {e}")
                time.sleep(1)
    
    def start(self):
        """Inicia el sistema de detección"""
        logger.info("Starting Butterfly Detection System with Enhanced Features...")
        
        try:
            # Inicializar componentes
            self.initialize_components()
            
            # Marcar como ejecutándose
            self.running = True
            
            # Iniciar bucle de detección
            self.detection_loop()
            
        except KeyboardInterrupt:
            logger.info("System interrupted by user")
        except Exception as e:
            logger.error(f"Unexpected error: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Detiene el sistema de detección"""
        logger.info("Stopping Butterfly Detection System...")
        
        self.running = False
        
        if self.camera:
            self.camera.stop()
        
        # Enviar detecciones pendientes antes de cerrar
        if self.detection_buffer and self.comms:
            try:
                self.send_buffered_detections()
            except:
                pass
        
        logger.info("System stopped")

def handle_exit(signum, frame):
    """Manejador de señales para salida limpia"""
    logger.info("Exiting application...")
    if 'detection_system' in globals():
        detection_system.stop()
    sys.exit(0)

# Registrar manejadores de señales
signal.signal(signal.SIGINT, handle_exit)
signal.signal(signal.SIGTERM, handle_exit)

def main():
    global detection_system
    detection_system = ButterflyDetectionSystem()
    detection_system.start()

if __name__ == "__main__":
    main()
