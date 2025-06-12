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
