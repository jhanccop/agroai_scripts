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
