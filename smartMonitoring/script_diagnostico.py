#!/usr/bin/env python3
import cv2
import time
import os
import subprocess

def listar_dispositivos_video():
    """Lista todos los dispositivos de video disponibles en el sistema"""
    print("\n=== DISPOSITIVOS DE VIDEO DISPONIBLES ===")
    
    # Listar dispositivos en /dev/
    print("\nDispositivos en /dev/:")
    video_devices = [d for d in os.listdir('/dev') if d.startswith('video')]
    if video_devices:
        for device in sorted(video_devices):
            print(f"/dev/{device}")
    else:
        print("No se encontraron dispositivos de video en /dev/")
    
    # Información de dispositivos USB
    print("\nDispositivos USB conectados:")
    try:
        usb_output = subprocess.check_output(['lsusb'], universal_newlines=True)
        for line in usb_output.splitlines():
            if 'Camera' in line or 'cam' in line.lower() or 'video' in line.lower():
                print(f"[CÁMARA] {line.strip()}")
            else:
                print(line.strip())
    except:
        print("No se pudo ejecutar lsusb")
    
    # Información detallada V4L2
    print("\nInformación detallada de V4L2:")
    try:
        for device in video_devices:
            device_path = f"/dev/{device}"
            print(f"\nInfo para {device_path}:")
            try:
                v4l2_output = subprocess.check_output(['v4l2-ctl', '--device', device_path, '--all'], universal_newlines=True)
                # Mostrar solo las primeras 20 líneas para no saturar la salida
                print("\n".join(v4l2_output.splitlines()[:20]))
                print("...")
            except:
                print(f"No se pudo obtener información de {device_path}")
    except:
        print("No se pudo obtener información de V4L2")

def probar_apis_camara():
    """Prueba diferentes APIs de OpenCV para abrir la cámara"""
    print("\n=== PROBANDO DIFERENTES APIS DE CÁMARA ===")
    
    # Lista de APIs a probar
    apis = [
        (cv2.CAP_ANY, "AUTO"),
        (cv2.CAP_V4L2, "V4L2"),
        (cv2.CAP_V4L, "V4L"),
        (cv2.CAP_GSTREAMER, "GStreamer"),
    ]
    
    # Dispositivos a probar
    dispositivos = [
        0,  # Índice numérico
        1,  # Índice alternativo
        "/dev/video0",  # Ruta directa
        "/dev/video1",  # Ruta alternativa
    ]
    
    # Probar cada combinación
    for api_id, api_name in apis:
        for dispositivo in dispositivos:
            print(f"\nProbando API {api_name} con dispositivo {dispositivo}")
            try:
                cap = cv2.VideoCapture(dispositivo, api_id)
                if cap.isOpened():
                    print(f"✓ Conexión exitosa")
                    
                    # Obtener propiedades
                    ancho = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                    alto = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                    fps = cap.get(cv2.CAP_PROP_FPS)
                    
                    print(f"  Resolución: {ancho}x{alto}")
                    print(f"  FPS: {fps}")
                    
                    # Intentar leer un frame
                    ret, frame = cap.read()
                    if ret:
                        print(f"  ✓ Frame leído correctamente: {frame.shape}")
                        # Guardar el frame como prueba
                        output_file = f"test_{api_name}_{dispositivo}.jpg".replace("/", "_")
                        cv2.imwrite(output_file, frame)
                        print(f"  ✓ Frame guardado en {output_file}")
                    else:
                        print(f"  ✗ No se pudo leer un frame")
                    
                    cap.release()
                else:
                    print(f"✗ No se pudo abrir la cámara")
            except Exception as e:
                print(f"✗ Error: {str(e)}")

def recomendar_configuracion():
    """Proporciona recomendaciones basadas en los resultados"""
    print("\n=== RECOMENDACIONES ===")
    print("""
Basado en los resultados, modifique config.py para usar:

1. El dispositivo que funcionó correctamente (por ejemplo, /dev/video0 o índice 0)
2. La API que funcionó (V4L2, V4L, etc.)
3. Una resolución compatible

Por ejemplo:
    CAMERA_DEVICE = "/dev/video0"  # O el que funcionó
    CAMERA_API = cv2.CAP_V4L2  # O la API que funcionó
    CAMERA_RESOLUTION = (640, 480)  # O una resolución compatible
    
Si ninguna configuración funcionó, puede intentar:
1. Verificar si la cámara requiere drivers adicionales
2. Reiniciar el sistema
3. Probar con un HUB USB con alimentación externa si usa USB
4. Verificar permisos de acceso al dispositivo de video
   sudo chmod 666 /dev/videoX
""")

if __name__ == "__main__":
    print("DIAGNÓSTICO DE CÁMARA PARA SISTEMA DE DETECCIÓN DE MARIPOSAS")
    print("==========================================================")
    
    # Obtener versión de OpenCV
    print(f"Versión de OpenCV: {cv2.__version__}")
    
    # Listar dispositivos de video disponibles
    listar_dispositivos_video()
    
    # Probar diferentes APIs
    probar_apis_camara()
    
    # Mostrar recomendaciones
    recomendar_configuracion()
```# Sistema de Detección de Mariposas con Orange Pi 2 W

Este documento describe la implementación de un sistema embebido para detectar mariposas usando una Orange Pi 2 W y una cámara, con funcionalidades de procesamiento de video, comunicación MQTT y transferencia de archivos a un servidor web.

## 1. Componentes del Hardware

- **Orange Pi 2 W**
- **Cámara USB** (o cámara CSI compatible con Orange Pi)
- **Fuente de alimentación** (5V/2A recomendado)
- **Tarjeta MicroSD** (16GB o más, clase 10)
- **Opcional: Carcasa o soporte** para instalación en exteriores

## 2. Configuración del Software Base

### 2.1 Sistema Operativo

Instalaremos Armbian, una distribución Linux optimizada para dispositivos ARM:

```bash
# Descargar imagen de Armbian para Orange Pi 2 W desde https://www.armbian.com/orange-pi-2/
# Flashear la imagen en la tarjeta MicroSD usando Etcher o dd

# Una vez iniciado el sistema:
sudo apt update
sudo apt upgrade -y
