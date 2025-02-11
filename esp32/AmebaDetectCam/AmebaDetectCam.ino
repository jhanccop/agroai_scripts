/*

 Example guide:
 https://www.amebaiot.com/en/amebapro2-arduino-neuralnework-object-detection/

 NN Model Selection
 Select Neural Network(NN) task and models using .modelSelect(nntask, objdetmodel, facedetmodel, facerecogmodel).
 Replace with NA_MODEL if they are not necessary for your selected NN Task.

 NN task
 =======
 OBJECT_DETECTION/ FACE_DETECTION/ FACE_RECOGNITION

 Models
 =======
 YOLOv3 model         DEFAULT_YOLOV3TINY   / CUSTOMIZED_YOLOV3TINY
 YOLOv4 model         DEFAULT_YOLOV4TINY   / CUSTOMIZED_YOLOV4TINY
 YOLOv7 model         DEFAULT_YOLOV7TINY   / CUSTOMIZED_YOLOV7TINY
 SCRFD model          DEFAULT_SCRFD        / CUSTOMIZED_SCRFD
 MobileFaceNet model  DEFAULT_MOBILEFACENET/ CUSTOMIZED_MOBILEFACENET
 No model             NA_MODEL
 */

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"

#include "MP4Recording.h"

#define CHANNEL 0
#define CHANNELNN 3

// Lower resolution for NN processing
#define NNWIDTH 720
#define NNHEIGHT 460

VideoSetting config(VIDEO_FHD, 15, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH,NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
//RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "ConectaLineal2500";  // your network SSID (name)
char pass[] = "19CERA@DER27@14";    // your network password
int status = WL_IDLE_STATUS;

MP4Recording mp4;

IPAddress ip;
//int rtsp_portnum;

/* ================ CONFIGURACIONES INICIALES ==================== */
const int nDetect = 10;
int nCount = 0;
boolean flag = true;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);

  // conexion a red wifi
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(2000);
  }
  ip = WiFi.localIP();

  // Configure camera video channels with video format information
  // Adjust the bitrate based on your WiFi network quality
  //config.setBitrate(2 * 1024 * 1024);  // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
  Camera.configVideoChannel(CHANNEL, config);
  Camera.configVideoChannel(CHANNELNN, configNN);
  Camera.videoInit();

  // Configure RTSP with corresponding video format information
  //rtsp.configVideo(config);
  //rtsp.begin();
  //rtsp_portnum = rtsp.getPort();

  // Configure object detection with corresponding video format information
  // Select Neural Network(NN) task and models
  ObjDet.configVideo(configNN);
  ObjDet.setResultCallback(ODPostProcess);
  ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, NA_MODEL, NA_MODEL);
  ObjDet.begin();

  // Configure StreamIO object to stream data from video channel to RTSP
  //videoStreamer.registerInput(Camera.getStream(CHANNEL));
  //videoStreamer.registerOutput(rtsp);
  //if (videoStreamer.begin() != 0) {
  //    Serial.println("StreamIO link start failed");
  //}

  // Start data stream from video channel
  Camera.channelBegin(CHANNEL);

  // Configure StreamIO object to stream data from RGB video channel to object detection
  videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
  videoStreamerNN.setStackSize();
  videoStreamerNN.setTaskPriority();
  videoStreamerNN.registerOutput(ObjDet);
  if (videoStreamerNN.begin() != 0) {
    Serial.println("StreamIO link start failed");
  }

  // Start video channel for NN
  Camera.channelBegin(CHANNELNN);

  // Start OSD drawing on RTSP video channel
  OSD.configVideo(CHANNEL, config);
  OSD.begin();
}

void loop() {
  Serial.println(millis());
  delay(1000);
}

// User callback function for post processing of object detection results
void ODPostProcess(std::vector<ObjectDetectionResult> results) {
  uint16_t im_h = config.height();
  uint16_t im_w = config.width();

  printf("Total number of objects detected = %d - %d\r\n", ObjDet.getResultCount(),nCount);
  OSD.createBitmap(CHANNEL);

  if (ObjDet.getResultCount() > 0) {

    for (int i = 0; i < ObjDet.getResultCount(); i++) {
      int obj_type = results[i].type();
      if (itemList[obj_type].filter) {  // check if item should be ignored

        ObjectDetectionResult item = results[i];
        // Result coordinates are floats ranging from 0.00 to 1.00
        // Multiply with RTSP resolution to get coordinates in pixels
        int xmin = (int)(item.xMin() * im_w);
        int xmax = (int)(item.xMax() * im_w);
        int ymin = (int)(item.yMin() * im_h);
        int ymax = (int)(item.yMax() * im_h);

        // Draw boundary box
        printf("Item %d %s:\t%d %d %d %d\n\r", i, itemList[obj_type].objectName, xmin, xmax, ymin, ymax);
        OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE);

        // Print identification text
        char text_str[20];
        snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());
        OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);
      }
    }

    if(nCount >= nDetect && flag){
      flag = false;
      previousMillis = millis();
      Serial.println("recording captured..... *************************** ");

      mp4.configVideo(config);
      mp4.setRecordingDuration(15);
      mp4.setRecordingFileCount(1);
      mp4.setRecordingFileName("TestRecordingVideoOnly");
      mp4.setRecordingDataType(STORAGE_VIDEO);

      videoStreamer.registerInput(Camera.getStream(CHANNEL));
      videoStreamer.registerOutput(mp4);
      if (videoStreamer.begin() != 0) {
          Serial.println("StreamIO link start failed");
      }

      //Camera.channelBegin(CHANNEL);
      // Start recording MP4 data to SD card
      mp4.begin();

      nCount = 0;

      //Serial.println("captured.....");
      //delay(16000);

    }

    nCount++;

  }else{
    nCount = 0;
  }
  
  OSD.update(CHANNEL);

  if(millis() - previousMillis > 20000 && !flag){
    Serial.println("--------------------FINISHED ALLLLLLLLLLLLLLLL");
    OSD.end();
    videoStreamer.end();
    ObjDet.end();
  }
  
}
