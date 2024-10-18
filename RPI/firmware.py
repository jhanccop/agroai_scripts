# pip install torch --no-cache-dir

import os
import time
import random
import json
from datetime import datetime
import base64
from paho.mqtt import client as mqtt_client

import serial
import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BOARD)

GPIO.setwarnings(False)

# SETTING PINS
PIN = 40
PINL = 22
PIN_RPI_STATUS = 18
PIN_RPI_MANAGER = 16
PIN_ESP32_MANAGER = 15

GPIO.setup(PIN, GPIO.IN, pull_up_down = GPIO.PUD_UP)
GPIO.setup(PINL, GPIO.OUT)
GPIO.setup(PIN_RPI_STATUS, GPIO.OUT)
GPIO.setup(PIN_RPI_MANAGER, GPIO.IN, pull_up_down = GPIO.PUD_UP)
GPIO.setup(PIN_ESP32_MANAGER, GPIO.OUT)

# SETTING GENERAL
status = True
continuous = True
timesleep = 60 # 60 minutes
refresh = 10 # seconds
saveImage = False
runningNN = False

#broker = 'broker.emqx.io' #'broker.hivemq.com' broker.hivemq.com
broker = "24.199.125.52"
port = 1883

topicPub = "jhpOandG/data"
topicSub = "jhpOandG/settings"
client_id = f'publish-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'

client_name = "CAMERA4"
mac = "30:AE:A4:CA:C5:E4"

def connectWifi():

    try:
        ssid = "WLAN_INVITADOS"
        pw = "9876564321"

        GPIO.output(PIN_ESP32_MANAGER, GPIO.HIGH)
        time.sleep(0.5)
        GPIO.output(PIN_ESP32_MANAGER, GPIO.LOW)
        time.sleep(2)

        ser = serial.Serial('/dev/ttyS0', 115200, timeout=2)
        ser.flush()
        payload = {"msg":"wifi"}
        msg = json.dumps(payload)
        print("msg esp32", payload)
        string = msg + "\n" #"\n" for line seperation
        string = string.encode('utf_8')
        ser.write(string) # sending over UART

        line = ""

        count = 0
        while True:
            line = ser.readline().decode('utf-8').rstrip()
            print("esp32 received: ",line)
            if len(line) >= 5:
                msgIn = json.loads(line)
                ssid = msgIn["ssid"]
                pw = msgIn["pw"]
                #return msgIn['mac'], msgIn['voltage'], msgIn['T'], msgIn['H']
                break
            if count >= 5:
                return "error"
                #return {"0",0,0,0}
                #break
            count = count + 1
        ser.close()

        
        os.system("sudo iw wlan0 scan | grep SSID")
        time.sleep(1)
        os.system(f'sudo nmcli dev wifi connect "{ssid}" password "{pw}"')

    except Exception as ex:
        print("wifi err", type(ex))

def blink(n):
    print("blink",n)
    for i in range(n):
        GPIO.output(PINL, GPIO.HIGH)
        time.sleep(0.4)
        GPIO.output(PINL, GPIO.LOW)
        time.sleep(0.2)
    time.sleep(2)

def takeVideo(name):
    try:
        os.system(f"libcamera-vid --level 4.2 --framerate 2 --width 1280 --height 720 -o {name}.h264 -t 90000 --autofocus-mode auto -n")
        
        #os.system(f"libcamera-still -o {name}.jpg")
        #os.system(f"ffmpeg -i {name}.h264 {name}.mp4")

        print("Image captured")
        #os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
        os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp  -o 'StrictHostKeyChecking=no' {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
        os.system(f"mv {name}.h264 imgs")

    except Exception as ex:
        print("takeVideo err", type(ex))

def takePicture(name):
    try:
        os.system(f"libcamera-still -o {name}.jpg --rotation 180 --width 1280 --height 720 -n -t 1")
        os.system(f"mv {name}.jpg imgs")
    except Exception as ex:
        print("takePicture err", type(ex))

def runCNN():
    
    try:
        payload = {"0":3, "4":1}
        return True, payload
    except Exception as ex:
        print("takePicture err", type(ex))

def connect_mqtt():
    try:
        def on_connect(client, userdata, flags, rc, props=None):
            if rc == 0:
                print("Connected to MQTT Broker!")
            else:
                print("Failed to connect, return code %d\n", rc)

        client = mqtt_client.Client(client_id=client_id, callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2)
        # client.username_pw_set(username, password)
        client.on_connect = on_connect
        client.connect(broker, port)
        return client

    except Exception as ex:
        print("connect_mqtt err", type(ex))

def connectESP(msgOut,response):
    try:
        GPIO.output(PIN_ESP32_MANAGER, GPIO.HIGH)
        time.sleep(0.5)
        GPIO.output(PIN_ESP32_MANAGER, GPIO.LOW)
        time.sleep(1)

        ser = serial.Serial('/dev/ttyS0', 115200, timeout=2)
        ser.flush()
        #payload = {"msg":msgOut,"timesleep":timesleep}
        payload = {"msg":msgOut}
        msg = json.dumps(payload)
        print("msg esp32", payload)
        string = msg + "\n" #"\n" for line seperation
        string = string.encode('utf_8')
        ser.write(string) # sending over UART
        ser.flush()
        line = ""

        count = 0
        while response:
            line = ser.readline().decode('utf-8').rstrip()
            print("esp32 received: ",line)
            if len(line) >= 5:
                msgIn = json.loads(line)
                ser.close()
                return msgIn
                #return msgIn['mac'], msgIn['voltage'], msgIn['T'], msgIn['H']
                #break
            if count >= 5:
                ser.close()
                return json.loads({})
                #return {"0",0,0,0}
                #break
            count = count + 1

    except Exception as ex:
        print("connectESP err", type(ex))
    
def shutDown():
    try:
        print("apagando")

        if GPIO.input(PIN):
            connectESP("sleep",False)
            print("apagado...")
            blink(10)
            os.system('sudo shutdown -h now')

    except Exception as ex:
        print("shutDown err", type(ex))

def subscribe(client: mqtt_client):
    try:
        def on_message(client, userdata, msg):
            print("settings recieved")

            try:
                topic_in = str(msg.topic)
                data_in = str(msg.payload.decode("utf-8"))

                print(data_in)
                m_mqtt = json.loads(data_in)
                name = m_mqtt.get("name",False)

                # 3. READ SETTINGS
                if name == client_name:
                    global timesleep, status, continuous, refresh
                    timesleep = int(m_mqtt.get("timesleep",60))
                    status = m_mqtt.get("status",True)
                    continuous = m_mqtt.get("continuous",True)
                    refresh = m_mqtt.get("refresh",10)
                    print("new timesleep ",continuous, type(continuous), type(True))

            except Exception as e:
                print('Arrival msg error..... ', e)

                blink(10)

        client.subscribe(topicSub)
        client.on_message = on_message

    except Exception as ex:
        print("subscribe err", type(ex))

def publishMsg(file_name,payloadNN):

    try:
        client = connect_mqtt()
        client.loop_start()
        #subscribe(client)

        with open(f"imgs/{file_name}.jpg", "rb") as f:
            encoded_image = base64.b64encode(f.read())

        # prepare data
        payloadESP32 = connectESP("data",True)

        payload = {}
        payload["type"] = "camVid"
        payload["mac"] = payloadESP32.get("mac",0)
        payload["name"] = client_name
        payload["T"] = payloadESP32.get("T",0)
        payload["H"] = payloadESP32.get("H",0)
        payload["B"] =payloadESP32.get("B",0)
        payload["P"] =payloadESP32.get("P",0)
        payload["R"] = payloadESP32.get("R",0)
        payload["V"] = payloadESP32.get("V",0)
        payload["D"] = payloadESP32.get("D",0)
        
        payload["img"] = f"{file_name}.jpg"
        payload["cnn"] = payloadNN
        payload["image"] = encoded_image.decode('utf-8')


        msg = json.dumps(payload)
        
        # send message
        result = client.publish(topicPub, msg)
        time.sleep(1)
        blink(3)
        client.loop_stop()

        #shutDown()

    except Exception as ex:
        print("publishMsg err", type(ex))
    
def run():
    try:

        # 1. WRITE RPI STATUS: (LOW) IS ACTIVATED (HIGH) ISN'T ACTIVATE 
        GPIO.output(PIN_RPI_STATUS, False) # 

        payloadESP32 = connectESP("settings",True)

        global continuous, refresh, saveImage, runningNN
        continuous = payloadESP32.get("continuous",True)
        refresh = payloadESP32.get("refresh",60)
        saveImage = payloadESP32.get("saveImage",False)
        runningNN = payloadESP32.get("runningNN",False)

        if not continuous:

            print("NO CONTINUE***")
            # 5. TAKE PICTURE
            blink(1)
            time.sleep(2)

            now = datetime.now()
            dt_string = now.strftime("%d-%m-%Y_%H-%M-%S")
            print("Date Time =", dt_string)
            file_name = client_name + "_" + dt_string
            takePicture(file_name) # get image from camera

            # 6. RUN CNN
            #ojectDetect, payloadNN = runCNN()

            # 7 send data
            publishMsg(file_name,0)

            #connectESPImage("image",file_name)
            blink(4)

            if not saveImage:
                print("Drop image")
                os.system(f"rm imgs/{file_name}.jpg")
            shutDown() # SHUT DOWN RPI
            return 0

        else:
            from ultralytics import YOLO
            model = YOLO('best.pt')
            time.sleep(5)
            blink(1)
            model = YOLO('best.pt')
            
            counter = 0

            while continuous:

                if counter >= 50:
                    payloadESP32 = connectESP("settings",True)

                    #global continuous, refresh, runningNN
                    continuous = payloadESP32.get("continuous",True)
                    refresh = payloadESP32.get("refresh",60)
                    runningNN = payloadESP32.get("runningNN",False)
                    saveImage = payloadESP32.get("saveImage",False)
                    counter = 0

                # 5. TAKE PICTURE
                blink(2)

                now = datetime.now()
                dt_string = now.strftime("%d-%m-%Y_%H-%M-%S")
                print("Date Time =", dt_string)
                file_name = client_name + "_" + dt_string
                takePicture(file_name) # get image from camera

                results = model([f"imgs/{file_name}.jpg"])
                nDetected = results[0].boxes.shape[0]

                if nDetected > 0:
                    results[0].save(filename=f"imgs/{file_name}.jpg")

                    publishMsg(file_name, nDetected)
                    connectESP("completed",False)

                blink(3)
                counter += 1
                print(refresh)
                time.sleep(refresh)
                counter = counter + 1

        # 3. READ RPI MANAGER (LOW) IS ACTIVATED (HIGH) ISN'T ACTIVATE 
        GPIO.input(PIN_RPI_MANAGER)

    except Exception as ex:
        print("run err", type(ex))
    finally:
        shutDown()
    
if __name__ == '__main__':
    connectWifi()
    time.sleep(2)
    os.system("sudo ntpdate pool.ntp.org")
    GPIO.output(PINL, GPIO.HIGH)
    time.sleep(3)
    GPIO.output(PINL, GPIO.LOW)
    run()
