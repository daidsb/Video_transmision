#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

// WiFi
const char *ssid = "ELIZABETH-1";
const char *pass = "CSEBMC23"; //"147258369";

// Conexion Websocket
const char *ws_server = "34.176.62.15"; //"34.176.62.15"
const int ws_port = 8765;
const char *ws_path = "ESP32";

// Variables globales
WebSocketsClient webSocket;
uint8_t contadorFrames = 0;

// Manejador de tareas
TaskHandle_t task1Handle = NULL;
// TaskHandle_t task2Handle = NULL;
SemaphoreHandle_t xMutex;

// Task
void enviarDatosTask(void *parameter);

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Funciones

void iniCamara()
{
  // Inicializar camara
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
  config.frame_size = FRAMESIZE_HD;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.jpeg_quality = 30;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_DRAM;

  if (psramFound())
  {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM encontrada y verificada");
  }

  // camera init
  esp_err_t cam_err = esp_camera_init(&config);
  Serial.println("Configurando...");
  if (cam_err != ESP_OK)
  {
    Serial.println("Error...");
    Serial.printf("MENSAJE: Camera init failed with error 0x%x", cam_err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  Serial.println("Camara listo");
}

void conectarWifi(void)
{

  // Si no hay conexión WiFi, intentar conectar
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf("\nConectando a %s ", ssid);
    WiFi.begin(ssid, pass);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < 15000)
    {
      Serial.print(".");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("\nNo se pudo conectar al WiFi. Reiniciando...");
      ESP.restart();
    }

    Serial.println("\nConexión WiFi establecida!");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
  {
    Serial.println("Desconectado del servidor WebSocket");
    // digitalWrite(2, LOW);
    break;
  }
  case WStype_CONNECTED:
  {
    Serial.println("Conectado al servidor WebSocket");
    // digitalWrite(2, HIGH);
    break;
  }
  case WStype_TEXT:
  {
    Serial.printf("Mensaje recibido: %s\n", payload);
    break;
  }
  case WStype_BIN:
  {
    Serial.printf("Mensaje binario recibido, longitud: %u\n", length);
    break;
  }
  case WStype_ERROR:
  {
    Serial.println("Error en WebSocket");
    break;
  }
  case WStype_PING:
  {
    Serial.println("Ping recibido\n");
    break;
  }
  case WStype_PONG:
  {
    Serial.println("Pong recibido\n");
    break;
  }
  }
}

void conectarWebSocket()
{
  webSocket.begin(ws_server, ws_port, "/", ws_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void enviarSensado()
{

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["type"] = "sensores";
  jsonDoc["temperatura"] = temperatureRead();
  jsonDoc["freeHeap"] = ESP.getFreeHeap();
  jsonDoc["totalHeap"] = ESP.getHeapSize();
  jsonDoc["rssi"] = WiFi.RSSI();

  char jsonBuffer[512];
  serializeJson(jsonDoc, jsonBuffer);
  webSocket.sendTXT(jsonBuffer);
}

void setup()
{

  Serial.begin(115200);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  // pinMode(2, OUTPUT);
  // vTaskDelay(100 / portTICK_PERIOD_MS);
  // digitalWrite(2, LOW);
  // vTaskDelay(100 / portTICK_PERIOD_MS);
  conectarWifi();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  iniCamara();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  conectarWebSocket();
  Serial.println("Configurando WebSocket");
  vTaskDelay(100 / portTICK_PERIOD_MS);

  // crear mutex
  xMutex = xSemaphoreCreateMutex();
  // Crear las tareas
  xTaskCreate(enviarDatosTask, "enviarDatos", 8000, NULL, 1, &task1Handle);
  // xTaskCreate(enviarDatosTask,"enviarDatosTask",10000,NULL,2,&task2Handle);
}

void loop()
{

  webSocket.loop();

  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
  {

    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();

    if (fb)
    {
      // Serial.println(fb->len);
      if (webSocket.isConnected())
      {

        webSocket.sendBIN(fb->buf, fb->len);
        /*
                contadorFrames++;

                if(contadorFrames >= 12){
                  contadorFrames = 0;
                  enviarSensado();

                }
        */
      }
      esp_camera_fb_return(fb);
    }
    else
    {
      esp_camera_fb_return(fb);
    }

    xSemaphoreGive(xMutex);
  }
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void enviarDatosTask(void *parameter)
{

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Desconectado, Intentando reconectar...");
    conectarWifi();
  }

  for (;;)
  {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
    {
      if (webSocket.isConnected())
      {

        contadorFrames++;

        if (contadorFrames >= 15)
        {
          contadorFrames = 0;
          enviarSensado();
        }
      }
      xSemaphoreGive(xMutex);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}