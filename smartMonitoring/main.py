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
                
                # Mostrar imagen con detecciones
                annotated_frame = detector.annotate_frame(frame, boxes, confidences, indices, class_ids)
                cv2.imshow("Butterfly Detection", annotated_frame)
                
                # Enviar datos por MQTT (el video se enviará cuando termine la grabación)
                comms.send_detection(detection_json, video_path)
                
                print(f"Detection processed - JSON sent via MQTT, video recording to: {video_path}")
                
            # Visualización (eliminar en producción o hacer configurable)
            cv2.imshow("Butterfly Detection", frame)
            if cv2.waitKey(1) == ord('q'):
                break
                
            # Pequeña pausa para no saturar la CPU
            time.sleep(0.01)
            
    except KeyboardInterrupt:
        print("Program interrupted by user")
    finally:
        camera.stop()
        cv2.destroyAllWindows()
        print("System stopped")

if __name__ == "__main__":
    main()
