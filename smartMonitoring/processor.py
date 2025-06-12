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
