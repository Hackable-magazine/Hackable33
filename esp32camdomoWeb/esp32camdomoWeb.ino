#include <esp_camera.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <fb_gfx.h>
#include <esp_http_server.h>
#include <Wire.h>
#include <BME280I2C.h>
/*
//#include <Adafruit_BME280.h>  
  impossible erreur esp32-camera/sensor.h="typedef struct _sensor sensor_t" et Adafruit_Sensor.h="typedef struct sensor_t sensor_t"
  Adafruit qui préfixe ces fichier de "Adafruit_" partout pour se faire de la pub, mais pas ces structs utilisées dans ses libs !
*/

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// macro dessin image
#define COLOR_WHITE  0x00FFFFFF
#define COLOR_BLACK  0x00000000
#define COLOR_RED    0x000000FF
#define COLOR_GREEN  0x0000FF00
#define COLOR_BLUE   0x00FF0000
#define COLOR_YELLOW (COLOR_RED | COLOR_GREEN)
#define COLOR_CYAN   (COLOR_BLUE | COLOR_GREEN)
#define COLOR_PURPLE (COLOR_BLUE | COLOR_RED)
#define ESPX 100
#define ESPY 100
#define TAILLE 10

// mot de passe pour l'OTA
const char* otapass = "123456";

// structure pour stocker les infos
struct EEconf {
  char ssid[32];
  char password[64];
  char myhostname[32];
};
EEconf readconf;

httpd_handle_t cam_httpd = NULL;
httpd_handle_t live_httpd = NULL;

BME280I2C bme;
float temp, hum, pres;

unsigned long previousMillis = 0;

void confOTA() {
  // Port 8266 (défaut)
  ArduinoOTA.setPort(8266);

  // Hostname défaut : esp8266-[ChipID]
  ArduinoOTA.setHostname(readconf.myhostname);

  // mot de passe pour OTA
  ArduinoOTA.setPassword(otapass);

  // lancé au début de la MaJ
  ArduinoOTA.onStart([]() {
    Serial.println("/!\\ MaJ OTA");
  });

  // lancé en fin MaJ
  ArduinoOTA.onEnd([]() {
    Serial.print("\n/!\\ MaJ terminee ");
  });

  // lancé lors de la progression de la MaJ
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progression: %u%%\r", (progress / (total / 100)));
  });

  // En cas d'erreur
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("OTA_AUTH_ERROR");
    else if (error == OTA_BEGIN_ERROR) Serial.println("OTA_BEGIN_ERROR");
    else if (error == OTA_CONNECT_ERROR) Serial.println("OTA_CONNECT_ERROR");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA_RECEIVE_ERROR");
    else if (error == OTA_END_ERROR) Serial.println("OTA_END_ERROR");
    else Serial.println("Erreur inconnue");
  });

  // Activation fonctionnalité OTA
  ArduinoOTA.begin();
}

int configcam() {
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return 1;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1);//flip it back
  s->set_hmirror(s, 1); // mirror it back
  //drop down frame size for higher initial frame rate
  //s->set_framesize(s, FRAMESIZE_QQVGA);   // 160x120
  //s->set_framesize(s, FRAMESIZE_QQVGA2);  // 128x160
  //s->set_framesize(s, FRAMESIZE_QCIF);    // 176x144
  //s->set_framesize(s, FRAMESIZE_HQVGA);   // 240x176
  //s->set_framesize(s, FRAMESIZE_QVGA);    // 320x240
  //s->set_framesize(s, FRAMESIZE_CIF);     // 400x296
  //s->set_framesize(s, FRAMESIZE_VGA);     // 640x480
  s->set_framesize(s, FRAMESIZE_SVGA);    // 800x600
  //s->set_framesize(s, FRAMESIZE_XGA);     // 1024x768
  //s->set_framesize(s, FRAMESIZE_SXGA);    // 1280x1024
  //s->set_framesize(s, FRAMESIZE_UXGA);    // 1600x1200

  return 0;
}

void printSysInfo() {
  // Info
  Serial.println("Info système:");
  Serial.print("  Révision CPU   : ");
  Serial.println(ESP.getChipRevision());
  Serial.print("  Fréq. CPU      : ");
  Serial.println(ESP.getCpuFreqMHz());
  Serial.print("  Flash totale   : ");
  Serial.println(ESP.getFlashChipSize());
  Serial.print("  Taille sketch  : ");
  Serial.println(ESP.getSketchSize());
  Serial.print("  Sketch libre   : ");
  Serial.println(ESP.getFreeSketchSpace());
  Serial.print("  RAM totale     : ");
  Serial.println(ESP.getHeapSize());
  Serial.print("  RAM libre      : ");
  Serial.println(ESP.getFreeHeap());
  if(psramFound()){
    Serial.print("  PSRAM totale   : ");
    Serial.println(ESP.getPsramSize());
    Serial.print("  PSRAM libre    : ");
    Serial.println(ESP.getFreePsram());
  }
  Serial.print("  Adresse MAC    : ");
  Serial.println(WiFi.macAddress());
  Serial.println("");
}

void printCamStatus() {
  sensor_t * s = esp_camera_sensor_get(); 
  Serial.println("Configuration caméra:");
  Serial.printf("  framesize      : %u\r\n", s->status.framesize);
  Serial.printf("  quality        : %u\r\n", s->status.quality);
  Serial.printf("  brightness     : %d\r\n", s->status.brightness);
  Serial.printf("  contrast       : %d\r\n", s->status.contrast);
  Serial.printf("  saturation     : %d\r\n", s->status.saturation);
  Serial.printf("  sharpness      : %d\r\n", s->status.sharpness);
  Serial.printf("  special_effect : %u\r\n", s->status.special_effect);
  Serial.printf("  wb_mode        : %u\r\n", s->status.wb_mode);
  Serial.printf("  awb            : %u\r\n", s->status.awb);
  Serial.printf("  awb_gain       : %u\r\n", s->status.awb_gain);
  Serial.printf("  aec            : %u\r\n", s->status.aec);
  Serial.printf("  aec2           : %u\r\n", s->status.aec2);
  Serial.printf("  ae_level       : %d\r\n", s->status.ae_level);
  Serial.printf("  aec_value      : %u\r\n", s->status.aec_value);
  Serial.printf("  agc            : %u\r\n", s->status.agc);
  Serial.printf("  agc_gain       : %u\r\n", s->status.agc_gain);
  Serial.printf("  gainceiling    : %u\r\n", s->status.gainceiling);
  Serial.printf("  bpc            : %u\r\n", s->status.bpc);
  Serial.printf("  wpc            : %u\r\n", s->status.wpc);
  Serial.printf("  raw_gma        : %u\r\n", s->status.raw_gma);
  Serial.printf("  lenc           : %u\r\n", s->status.lenc);
  Serial.printf("  vflip          : %u\r\n", s->status.vflip);
  Serial.printf("  hmirror        : %u\r\n", s->status.hmirror);
  Serial.printf("  dcw            : %u\r\n", s->status.dcw);
  Serial.printf("  colorbar       : %u\r\n", s->status.colorbar);
  Serial.println("");
}

void ajoutOSD(fb_data_t *rgbfb) {
  char infostr[255];       // buffer texte OSB
  /*
  for(int x=ESPX; x<rgbfb.width-1; x+=ESPX) {
    // trace ligne verticale
    fb_gfx_fillRect(&rgbfb, x-1, 0, 2, rgbfb.height-1, COLOR_GREEN);
  }
  for(int y=ESPY; y<rgbfb.height-1; y+=ESPY) {
    // trace ligne horizontale
    fb_gfx_fillRect(&rgbfb, 0, y-1, rgbfb.width-1, 2, COLOR_GREEN);
  }
  */

  // Gaffe ! Aucune vérif de fb_gfx*, écriture nimp en mém
  for(int x=ESPX; x<rgbfb->width-1; x+=ESPX) {
    for(int y=ESPY; y<rgbfb->height-1; y+=ESPY) {
      fb_gfx_fillRect(rgbfb, x-1, y-TAILLE, 2, TAILLE*2, COLOR_GREEN);
      fb_gfx_fillRect(rgbfb, x-TAILLE, y-1, TAILLE*2, 2, COLOR_GREEN);
    }
  }

  // Ajout texte (23 = hauteur car   14 = largeur car.)
  snprintf(infostr, 255, " %s.local (%d.%d.%d.%d)  RSSI: %d dBm ",
    readconf.myhostname, 
    WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3],
    WiFi.RSSI()); 
  fb_gfx_fillRect(rgbfb, 10, (rgbfb->height)-10-24, 14*strlen(infostr), 24, COLOR_BLACK);
  fb_gfx_print(rgbfb, 10, (rgbfb->height)-10-24, COLOR_WHITE, infostr);

  memset(infostr, 0, sizeof(infostr));
  snprintf(infostr, 255, " Temp: %.2fC  Hum: %.0f%%  Pres: %.2f hPa ", temp, hum, pres/100);
  fb_gfx_fillRect(rgbfb, 10, 10, 14*strlen(infostr), 24, COLOR_BLACK);
  fb_gfx_print(rgbfb, 10, 10, COLOR_WHITE, infostr);
}

static esp_err_t jpg_handler(httpd_req_t *req){
  camera_fb_t *fb = NULL;  // framebuffer cam
  esp_err_t res = ESP_OK;  // réponse/return
  uint8_t *rgb;            // data RGB
  size_t rgb_len;          // taille data RGB
  fb_data_t rgbfb;         // framebuffer RGB
  uint8_t *jpg;            // data JPG
  size_t jpg_len;          // taille data JPG

  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Erreur capture caméra !");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
//  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  /********* RGB *********/
  rgb_len = (fb->width)*(fb->height)*3;
  if((rgb = (uint8_t*)ps_malloc(rgb_len)) == NULL) {
    Serial.println("Erreur ps_malloc !");
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb)) {
    Serial.println("Erreur fmt2rgb888 !");
    free(rgb);
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;    
  }

  rgbfb.width = fb->width;
  rgbfb.height = fb->height;
  rgbfb.data = rgb;
  rgbfb.bytes_per_pixel = 3;
  rgbfb.format = FB_BGR888;

  /********* Dessin *********/
  ajoutOSD(&rgbfb);
  
  /********* RGB->JPG *********/
  if(!fmt2jpg(rgb, rgb_len, rgbfb.width, rgbfb.height, PIXFORMAT_RGB888, 80, &jpg, &jpg_len)) {
    Serial.println("Erreur fmt2jpg !");
    free(rgb);
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  res = httpd_resp_send(req, (const char *)jpg, jpg_len);
  Serial.printf("Réponse HTTP, %zu\r\n", jpg_len);

  free(rgb);
  free(jpg);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t mjpg_handler(httpd_req_t *req){
  camera_fb_t *fb = NULL;  // framebuffer cam
  esp_err_t res = ESP_OK;  // réponse/return
  uint8_t *rgb;            // data RGB
  size_t rgb_len;          // taille data RGB
  fb_data_t rgbfb;         // framebuffer RGB
  uint8_t *jpg;            // data JPG
  size_t jpg_len;          // taille data JPG
  char *head_buf[64];      // buffer header HTTP
  size_t headerlen;        // taille JPEG dans header http

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=hackableboundary");
  if(res != ESP_OK) return res;
  res = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  if(res != ESP_OK) return res;

  while(true) {
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Erreur capture caméra !");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    /********* RGB *********/
    rgb_len = (fb->width)*(fb->height)*3;
    if((rgb = (uint8_t*)ps_malloc(rgb_len)) == NULL) {
      Serial.println("Erreur ps_malloc !");
      esp_camera_fb_return(fb);
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb)) {
      Serial.println("Erreur fmt2rgb888 !");
      free(rgb);
      esp_camera_fb_return(fb);
      httpd_resp_send_500(req);
      return ESP_FAIL;    
    }

    rgbfb.width = fb->width;
    rgbfb.height = fb->height;
    rgbfb.data = rgb;
    rgbfb.bytes_per_pixel = 3;
    rgbfb.format = FB_BGR888;

    /********* Dessin *********/
    ajoutOSD(&rgbfb);
  
    /********* RGB->JPG *********/
    if(!fmt2jpg(rgb, rgb_len, rgbfb.width, rgbfb.height, PIXFORMAT_RGB888, 80, &jpg, &jpg_len)) {
      Serial.println("Erreur fmt2jpg !");
      free(rgb);
      esp_camera_fb_return(fb);
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    headerlen = snprintf((char *)head_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_len);
    res = httpd_resp_send_chunk(req, (const char *)head_buf, headerlen);
    if(res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char *)jpg, jpg_len);
    if(res == ESP_OK)
      res = httpd_resp_send_chunk(req, "\r\n--hackableboundary\r\n", 22);
      
    if(res != ESP_OK)
      break;

    free(rgb);
    rgb = NULL;
    free(jpg);
    jpg = NULL;
    esp_camera_fb_return(fb);
    fb = NULL;
  }
  
  return res;
}

void confServeurWeb() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  
  httpd_uri_t capture_uri = {
    .uri       = "/jpeg",
    .method    = HTTP_GET,
    .handler   = jpg_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/mjpeg",
    .method    = HTTP_GET,
    .handler   = mjpg_handler,
    .user_ctx  = NULL
  };

  Serial.printf("Démarrage serveur HTTP JPG port %d\r\n", config.server_port);
  if (httpd_start(&cam_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(cam_httpd, &capture_uri);
    Serial.println("Serveur HTTP JPG ok.");
  } else {
    Serial.println("Erreur démarrage serveur HTTP JPG");
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.printf("Démarrage serveur HTTP MJPEG port %d\r\n", config.server_port);
  if (httpd_start(&live_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(live_httpd, &stream_uri);
    Serial.println("Serveur HTTP MJPEG ok.");
  } else {
    Serial.println("Erreur démarrage serveur HTTP MJPEG");
  }
}

void setup() {
  // compte tentative wifi
  int count = 0;

  // moniteur série
  Serial.begin(115200);
  // Démarrage
  Serial.println("\r\nBoot...");

  // Lecture configuration Wifi
  EEPROM.begin(sizeof(readconf));
  EEPROM.get(0, readconf);

  Wire.begin(12, 13);  // sda, scl
  if (!bme.begin()){
    Serial.println("Erreur BME280");
  } else {
    if(bme.chipModel() == BME280::ChipModel_BME280)
      temp = bme.temp();
      hum = bme.hum();
      pres = bme.pres();
      Serial.print("Modèle BME280   ");
      Serial.print(temp); Serial.print(" ");
      Serial.print(hum); Serial.print(" ");
      Serial.print(pres); Serial.print(" ");
      Serial.println("");
  }

  // printSysInfo();

  // Connexion au Wifi
  Serial.print(F("Connexion Wifi AP..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(readconf.ssid, readconf.password);
  while(WiFi.status() != WL_CONNECTED && count<=16) {
    delay(500);
    Serial.print(".");
    count++;
  }
  if(count>16) {
    Serial.println("Erreur connexion Wifi ! Reboot...");
    ESP.restart();
  }
  
  Serial.println(F("\r\nWiFi connecté"));
  Serial.print(F("Mon adresse IP: "));
  Serial.println(WiFi.localIP());

  // configuration OTA
  confOTA();

  if(configcam()) {
    Serial.println(F("\r\nErreur configuration cam !"));
    return;
  } else {
    Serial.println(F("\r\nConfiguration cam ok."));
  }

  //printCamStatus();

  confServeurWeb();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 30000) {
    temp = bme.temp();
    hum = bme.hum();
    pres = bme.pres();
    previousMillis = currentMillis;
  }

  // gestion OTA
  ArduinoOTA.handle();
}
