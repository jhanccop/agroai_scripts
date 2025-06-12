#!/usr/bin/env python3
"""
config_manager.py - Gestor de configuración remota via MQTT
"""

import json
import threading
import time
import logging
from datetime import datetime, timedelta
import paho.mqtt.client as mqtt

logger = logging.getLogger("ConfigManager")

class ConfigManager:
    def __init__(self, mqtt_broker, mqtt_port, mqtt_username, mqtt_password, config_topic):
        self.mqtt_broker = mqtt_broker
        self.mqtt_port = mqtt_port
        self.mqtt_username = mqtt_username
        self.mqtt_password = mqtt_password
        self.config_topic = config_topic
        
        # Configuración actual (valores por defecto)
        self.config = {
            "operation_start_hour": 6,
            "operation_end_hour": 18,
            "send_frequency_minutes": 60,
            "broker": mqtt_broker,
            "operation_resolution": [640, 480],
            "server_image_resolution": [320, 240],
            "confidence_threshold": 0.50,
            "nms_threshold": 0.4,
            "target_class": "butterfly",
            "web_server_url": "http://192.168.1.100:8080/upload",
            "video_duration": 10,
            "min_detection_interval": 5,
            "auto_send": True
        }
        
        self.config_updated = False
        self.config_lock = threading.Lock()
        
        # Cliente MQTT para recibir configuración
        self.mqtt_client = mqtt.Client(client_id=f"config_manager_{int(time.time())}")
        self.mqtt_client.username_pw_set(mqtt_username, mqtt_password)
        self.mqtt_client.on_connect = self._on_connect
        self.mqtt_client.on_message = self._on_message
        
        # Inicializar conexión MQTT
        self._start_mqtt_client()
    
    def _start_mqtt_client(self):
        """Inicia la conexión MQTT en un hilo separado"""
        def connect_thread():
            while True:
                try:
                    self.mqtt_client.connect(self.mqtt_broker, self.mqtt_port, 60)
                    self.mqtt_client.loop_start()
                    break
                except Exception as e:
                    logger.error(f"Error connecting to MQTT broker for config: {e}")
                    time.sleep(10)
        
        threading.Thread(target=connect_thread, daemon=True).start()
    
    def _on_connect(self, client, userdata, flags, rc):
        """Callback cuando se conecta al broker MQTT"""
        if rc == 0:
            logger.info("Connected to MQTT broker for configuration")
            client.subscribe(self.config_topic)
            logger.info(f"Subscribed to config topic: {self.config_topic}")
        else:
            logger.error(f"Failed to connect to MQTT broker for config with code: {rc}")
    
    def _on_message(self, client, userdata, msg):
        """Callback cuando se recibe un mensaje de configuración"""
        try:
            config_data = json.loads(msg.payload.decode())
            logger.info(f"Configuration received: {config_data}")
            
            with self.config_lock:
                # Actualizar configuración con los valores recibidos
                for key, value in config_data.items():
                    if key in self.config:
                        self.config[key] = value
                        logger.info(f"Config updated: {key} = {value}")
                
                self.config_updated = True
            
            logger.info("Configuration updated successfully")
            
        except json.JSONDecodeError as e:
            logger.error(f"Error parsing configuration JSON: {e}")
        except Exception as e:
            logger.error(f"Error processing configuration: {e}")
    
    def get_config(self):
        """Obtiene la configuración actual"""
        with self.config_lock:
            return self.config.copy()
    
    def is_config_updated(self):
        """Verifica si la configuración ha sido actualizada"""
        with self.config_lock:
            updated = self.config_updated
            self.config_updated = False  # Reset flag
            return updated
    
    def is_operation_time(self):
        """Verifica si estamos en horario de operación"""
        now = datetime.now()
        current_hour = now.hour
        
        with self.config_lock:
            start_hour = self.config["operation_start_hour"]
            end_hour = self.config["operation_end_hour"]
        
        if start_hour <= end_hour:
            # Horario normal (ej: 6:00 a 18:00)
            return start_hour <= current_hour < end_hour
        else:
            # Horario que cruza medianoche (ej: 18:00 a 6:00)
            return current_hour >= start_hour or current_hour < end_hour
    
    def get_next_send_time(self, last_send_time):
        """Calcula el próximo tiempo de envío basado en la frecuencia configurada"""
        with self.config_lock:
            frequency_minutes = self.config["send_frequency_minutes"]
        
        if last_send_time is None:
            return datetime.now()
        
        return last_send_time + timedelta(minutes=frequency_minutes)
    
    def should_auto_send(self):
        """Determina si debe realizar envío automático"""
        with self.config_lock:
            return self.config["auto_send"]
