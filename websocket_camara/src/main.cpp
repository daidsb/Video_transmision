#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

const int ledPin = 2;            // Pin LED 
unsigned long startTime = 0; // Tiempo en el que empezó a intentar conectarse
const unsigned long wifiTimeout = 30000; // Tiempo límite para conectar (30 segundos)

// Your WiFi credentials.
const char* ssid = "LAPTOP_LENOVO";
const char* pass = "87654321"; //"147258369";

//Conexion Websocket
const char* ws_server = "192.168.137.1"; 
const int ws_port = 8765; 

WebSocketsClient webSocket;

bool isConnected = false;

//CAMERA_MODEL_ESP32S3_EYE
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13


void iniCamara() {
//Inicializar camara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_CIF;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.jpeg_quality = 30;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_DRAM;
  
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM encontrada y verificada");
  }


  // camera init
  esp_err_t cam_err = esp_camera_init(&config);
  Serial.println("Configurando...");
  if (cam_err != ESP_OK) {
    Serial.println("Error...");
    Serial.printf("MENSAJE: Camera init failed with error 0x%x", cam_err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_HVGA);
  Serial.println("Camara listo");
}


void connectToWiFi() {
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  startTime = millis(); //Momento en que intenta conectarse WiFi
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if ( (millis() - startTime) < wifiTimeout){  
      Serial.print(".");
      attempt++;
    }
    else{
      Serial.println("Tiempo de conexión agotado. Reiniciando...");
      ESP.restart();  
    }
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  digitalWrite(ledPin, HIGH);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
    {
			Serial.println("Desconectado del servidor WebSocket");
      //webSocket.begin(ws_server, ws_port, "/");
			break;
    }
		case WStype_CONNECTED:
    {
      Serial.println("Conectado al servidor WebSocket");            
      break;
    }
		case WStype_TEXT:
			Serial.printf("Mensaje recibido: %s\n", payload);
			break;
		case WStype_BIN:
			Serial.printf("Mensaje binario recibido, longitud: %u\n", length);
			break;
    case WStype_ERROR:
        Serial.println("Error en WebSocket");
        break;			
    case WStype_PING:
        Serial.println("Ping recibido\n");
        break;
    case WStype_PONG:
        Serial.println("Pong recibido\n");
        break;
	}
}


void setup() {

  pinMode(2, OUTPUT);
  delay(100);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  delay(500);

  iniCamara();
  delay(1000);

  connectToWiFi();
  delay(2000);

//Conectar Websocket  
  webSocket.begin(ws_server, ws_port, "/", "ESP32");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2); 
  delay(2000);

}

void loop() {

  webSocket.loop();

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
 
  if (fb) {

    if (webSocket.isConnected()) {
        //Serial.print("Mensaje");
        webSocket.sendBIN(fb->buf, fb->len);
        //Serial.println(" enviado");
      }
      else{
        //Serial.println("no enviado");
      }
    
    //Serial.print(".");
    esp_camera_fb_return(fb);

  }
  else{
    //Serial.println("Error al capturar imagen");
    esp_camera_fb_return(fb);
  }

  delay(50);

}