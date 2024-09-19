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

broker = 'broker.hivemq.com' #'broker.emqx.io'
port = 1883

topicPub = "jhpOandG/data"
topicSub = "jhpOandG/settings"
client_id = f'publish-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'

client_name = "CAMERA1"
mac = "D4:8A:FC:A5:7A:58"

def connectWifi():

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

def blink(n):
    print("blink",n)
    for i in range(n):
        GPIO.output(PINL, GPIO.HIGH)
        time.sleep(0.4)
        GPIO.output(PINL, GPIO.LOW)
        time.sleep(0.2)
    time.sleep(2)

def takeVideo(name):
    os.system(f"libcamera-vid --level 4.2 --framerate 2 --width 1280 --height 720 -o {name}.h264 -t 90000 --autofocus-mode auto -n")
    
    #os.system(f"libcamera-still -o {name}.jpg")
    #os.system(f"ffmpeg -i {name}.h264 {name}.mp4")

    print("Image captured")
    #os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
    os.system(f"sudo sshpass -p 'c0l053N5353:20m' scp  -o 'StrictHostKeyChecking=no' {name}.h264 root@24.199.125.52:/root/imgs/{client_name}")
    os.system(f"mv {name}.h264 imgs")

def takePicture(name):
    os.system(f"libcamera-still -o {name}.jpg --width 1280 --height 720 -n -t 1")
    os.system(f"mv {name}.jpg imgs")

def runCNN():
    payload = {"0":3, "4":1}
    return True, payload

def connect_mqtt():
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

def connectESP(msgOut,response):
    GPIO.output(PIN_ESP32_MANAGER, GPIO.HIGH)
    time.sleep(0.5)
    GPIO.output(PIN_ESP32_MANAGER, GPIO.LOW)
    time.sleep(2)

    ser = serial.Serial('/dev/ttyS0', 115200, timeout=2)
    ser.flush()
    #payload = {"msg":msgOut,"timesleep":timesleep}
    payload = {"msg":msgOut}
    msg = json.dumps(payload)
    print("msg esp32", payload)
    string = msg + "\n" #"\n" for line seperation
    string = string.encode('utf_8')
    ser.write(string) # sending over UART

    line = ""

    count = 0
    while response:
        line = ser.readline().decode('utf-8').rstrip()
        print("esp32 received: ",line)
        if len(line) >= 5:
            msgIn = json.loads(line)
            return msgIn
            #return msgIn['mac'], msgIn['voltage'], msgIn['T'], msgIn['H']
            #break
        if count >= 2:
            return json.loads({})
            #return {"0",0,0,0}
            #break
        count = count + 1
    ser.close()
    return "0",0,0,0

def shutDown(client):
    client.loop_stop()
    connectESP("sleep",False)

    print("apagando")

    if GPIO.input(PIN):
        blink(10)
        os.system('sudo shutdown -h now')

def subscribe(client: mqtt_client):
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

                if not continuous:
                    shutDown(client)

        except Exception as e:
            print('Arrival msg error..... ', e)

            blink(10)

    client.subscribe(topicSub)
    client.on_message = on_message

def initial_publish(client):
    # send initial message to server
    settings = {
        "type":"camVidSet",
        "name" : client_name,
        "mac":mac
        }
    settings = json.dumps(settings)
    result = client.publish(topicPub, settings)

def publishMsg(client,file_name,payloadNN):

    with open(f"imgs/{file_name}.jpg", "rb") as f:
        encoded_image = base64.b64encode(f.read())
    # prepare data

    payloadESP32 = connectESP("info",True)
    payload = {}
    payload["type"] = "camVid"
    payload["mac"] = payloadESP32.get("mac",0)
    payload["name"] = client_name
    payload["H"] = payloadESP32.get("H",0)
    payload["T"] = payloadESP32.get("T",0)
    payload["B"] =payloadESP32.get("B",0)
    payload["img"] = f"{file_name}.jpg"
    payload["cnn"] = payloadNN
    payload["image"] = encoded_image.decode('utf-8')

    global timesleep, status, continuous, refresh
    refresh = payloadESP32.get("refresh",60)
    continuous = payloadESP32.get("continuous",True)
    status = payloadESP32.get("status",True)
    #print(payload)
    msg = json.dumps(payload)
    
    # send message
    result = client.publish(topicPub, msg)
    time.sleep(1)
    blink(4)
    #client.loop_stop()

def run():

    # 1. WRITE RPI STATUS: (LOW) IS ACTIVATED (HIGH) ISN'T ACTIVATE 
    GPIO.output(PIN_RPI_STATUS, False) # 

    client = connect_mqtt()
    client.loop_start()
    subscribe(client)

    # 2. SEND INITIAL MESSAGE - SETTINGS
    time.sleep(5)
    #initial_publish(client)

    payloadESP32 = connectESP("info",True)

    continuous = payloadESP32.get("continuous",True)
    refresh = payloadESP32.get("refresh",60)

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
        ojectDetect, payloadNN = runCNN()

        # 7 send data
        with open(f"imgs/{file_name}.jpg", "rb") as f:
            encoded_image = base64.b64encode(f.read())

        # prepare data
        payload = {}
        payload["type"] = "camVid"
        payload["mac"] = payloadESP32.get("mac",0)
        payload["name"] = client_name
        payload["H"] = payloadESP32.get("H",0)
        payload["T"] = payloadESP32.get("T",0)
        payload["B"] =payloadESP32.get("B",0)
        payload["img"] = f"{file_name}.jpg"
        payload["cnn"] = payloadNN
        payload["image"] = encoded_image.decode('utf-8')

        msg = json.dumps(payload)

        result = client.publish(topicPub, msg)
        time.sleep(1)
        blink(4)

        #publishMsg(client, file_name, payload)
        #connectESP("completed",False)
        shutDown(client) # SHUT DOWN RPI
        return 0


    counter = 0
    while continuous:

        # 5. TAKE PICTURE
        blink(2)
        time.sleep(2)

        now = datetime.now()
        dt_string = now.strftime("%d-%m-%Y_%H-%M-%S")
        print("Date Time =", dt_string)
        file_name = client_name + "_" + dt_string
        takePicture(file_name) # get image from camera

        # 6. RUN CNN
        ojectDetect, payload = runCNN()

        if ojectDetect and counter == 2:

            counter = 0
            # 7 WAKE UP ESP32                    
            publishMsg(client, file_name, payload)
            connectESP("completed",False)
        blink(3)
        counter += 1
        print(refresh)
        time.sleep(refresh)

    # 3. READ RPI MANAGER (LOW) IS ACTIVATED (HIGH) ISN'T ACTIVATE 
    GPIO.input(PIN_RPI_MANAGER)
    
if __name__ == '__main__':
    connectWifi()
    time.sleep(2)
    os.system("sudo ntpdate pool.ntp.org")
    GPIO.output(PINL, GPIO.HIGH)
    time.sleep(5)
    GPIO.output(PINL, GPIO.LOW)
    run()

