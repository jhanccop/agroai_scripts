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
