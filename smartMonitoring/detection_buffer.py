#!/usr/bin/env python3
"""
detection_buffer.py - Buffer para almacenar detecciones antes del envío
"""

import json
import os
import time
import threading
from datetime import datetime
from collections import deque
import logging

logger = logging.getLogger("DetectionBuffer")

class DetectionBuffer:
    def __init__(self, buffer_dir="buffer"):
        self.buffer_dir = buffer_dir
        self.detections = deque()
        self.lock = threading.Lock()
        
        # Crear directorio de buffer si no existe
        os.makedirs(buffer_dir, exist_ok=True)
        
        # Cargar detecciones pendientes del disco
        self._load_pending_detections()
    
    def add_detection(self, detection_data, image_path, video_path):
        """
        Añade una nueva detección al buffer
        
        Args:
            detection_data: Datos de la detección en formato dict
            image_path: Ruta a la imagen anotada
            video_path: Ruta al video grabado
        """
        timestamp = datetime.now().isoformat()
        
        detection_entry = {
            "timestamp": timestamp,
            "detection_data": detection_data,
            "image_path": image_path,
            "video_path": video_path,
            "sent": False,
            "retry_count": 0
        }
        
        with self.lock:
            self.detections.append(detection_entry)
            # Guardar en disco inmediatamente
            self._save_detection_to_disk(detection_entry)
        
        logger.info(f"Detection added to buffer at {timestamp}")
    
    def get_pending_detections(self):
        """
        Obtiene todas las detecciones pendientes de envío
        
        Returns:
            Lista de detecciones no enviadas
        """
        with self.lock:
            return [d for d in self.detections if not d["sent"]]
    
    def mark_as_sent(self, detection_timestamp):
        """
        Marca una detección como enviada exitosamente
        
        Args:
            detection_timestamp: Timestamp de la detección a marcar
        """
        with self.lock:
            for detection in self.detections:
                if detection["timestamp"] == detection_timestamp:
                    detection["sent"] = True
                    detection["sent_at"] = datetime.now().isoformat()
                    self._update_detection_on_disk(detection)
                    logger.info(f"Detection {detection_timestamp} marked as sent")
                    break
    
    def increment_retry_count(self, detection_timestamp):
        """
        Incrementa el contador de reintentos para una detección
        
        Args:
            detection_timestamp: Timestamp de la detección
        """
        with self.lock:
            for detection in self.detections:
                if detection["timestamp"] == detection_timestamp:
                    detection["retry_count"] += 1
                    self._update_detection_on_disk(detection)
                    logger.warning(f"Retry count for detection {detection_timestamp}: {detection['retry_count']}")
                    break
    
    def get_buffer_size(self):
        """Obtiene el número de detecciones en el buffer"""
        with self.lock:
            return len(self.detections)
    
    def get_pending_count(self):
        """Obtiene el número de detecciones pendientes de envío"""
        with self.lock:
            return len([d for d in self.detections if not d["sent"]])
    
    def cleanup_old_detections(self, max_age_hours=24):
        """
        Elimina detecciones antiguas del buffer
        
        Args:
            max_age_hours: Edad máxima en horas para mantener detecciones
        """
        cutoff_time = datetime.now().timestamp() - (max_age_hours * 3600)
        
        with self.lock:
            initial_count = len(self.detections)
            
            # Filtrar detecciones antiguas
            self.detections = deque([
                d for d in self.detections 
                if datetime.fromisoformat(d["timestamp"]).timestamp() > cutoff_time
            ])
            
            cleaned_count = initial_count - len(self.detections)
            if cleaned_count > 0:
                logger.info(f"Cleaned {cleaned_count} old detections from buffer")
    
    def _save_detection_to_disk(self, detection):
        """Guarda una detección en un archivo en disco"""
        filename = f"detection_{detection['timestamp'].replace(':', '-')}.json"
        filepath = os.path.join(self.buffer_dir, filename)
        
        try:
            with open(filepath, 'w') as f:
                json.dump(detection, f, indent=2)
        except Exception as e:
            logger.error(f"Error saving detection to disk: {e}")
    
    def _update_detection_on_disk(self, detection):
        """Actualiza una detección en disco"""
        filename = f"detection_{detection['timestamp'].replace(':', '-')}.json"
        filepath = os.path.join(self.buffer_dir, filename)
        
        try:
            with open(filepath, 'w') as f:
                json.dump(detection, f, indent=2)
        except Exception as e:
            logger.error(f"Error updating detection on disk: {e}")
    
    def _load_pending_detections(self):
        """Carga detecciones pendientes desde el disco al iniciar"""
        try:
            detection_files = [f for f in os.listdir(self.buffer_dir) if f.startswith('detection_') and f.endswith('.json')]
            
            for filename in detection_files:
                filepath = os.path.join(self.buffer_dir, filename)
                try:
                    with open(filepath, 'r') as f:
                        detection = json.load(f)
                        self.detections.append(detection)
                except Exception as e:
                    logger.error(f"Error loading detection from {filename}: {e}")
            
            logger.info(f"Loaded {len(self.detections)} detections from disk")
            
        except FileNotFoundError:
            logger.info("No previous detections found on disk")
        except Exception as e:
            logger.error(f"Error loading detections from disk: {e}")
    
    def get_statistics(self):
        """
        Obtiene estadísticas del buffer
        
        Returns:
            Dict con estadísticas del buffer
        """
        with self.lock:
            total = len(self.detections)
            sent = len([d for d in self.detections if d["sent"]])
            pending = total - sent
            
            # Calcular tiempo promedio entre detecciones
            if total > 1:
                timestamps = [datetime.fromisoformat(d["timestamp"]) for d in self.detections]
                timestamps.sort()
                intervals = [(timestamps[i] - timestamps[i-1]).total_seconds() for i in range(1, len(timestamps))]
                avg_interval = sum(intervals) / len(intervals) if intervals else 0
            else:
                avg_interval = 0
            
            return {
                "total_detections": total,
                "sent_detections": sent,
                "pending_detections": pending,
                "average_interval_seconds": avg_interval,
                "oldest_detection": timestamps[0].isoformat() if timestamps else None,
                "newest_detection": timestamps[-1].isoformat() if timestamps else None
            }
