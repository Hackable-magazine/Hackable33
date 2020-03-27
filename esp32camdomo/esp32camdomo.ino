#include <esp_camera.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <SD_MMC.h>
#include <fb_gfx.h>

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

#define WHITELED 4

#define COLOR_WHITE  0x00FFFFFF
#define COLOR_BLACK  0x00000000
#define COLOR_RED    0x000000FF
#define COLOR_GREEN  0x0000FF00
#define COLOR_BLUE   0x00FF0000
#define COLOR_YELLOW (COLOR_RED | COLOR_GREEN)
#define COLOR_CYAN   (COLOR_BLUE | COLOR_GREEN)
#define COLOR_PURPLE (COLOR_BLUE | COLOR_RED)

// mot de passe pour l'OTA
const char* otapass = "123456";

// structure pour stocker les infos
struct EEconf {
  char ssid[32];
  char password[64];
  char myhostname[32];
};
EEconf readconf;

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

/*
    PIXFORMAT_RGB565,    // 2BPP/RGB565
    PIXFORMAT_YUV422,    // 2BPP/YUV422
    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    PIXFORMAT_RGB888,    // 3BPP/RGB888
    PIXFORMAT_RAW,       // RAW
    PIXFORMAT_RGB444,    // 3BP2P/RGB444
    PIXFORMAT_RGB555,    // 3BP2P/RGB555
 */

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
  Serial.print("  Carte SD       : ");
  if(SD_MMC.cardType() != CARD_NONE) {
    Serial.printf("%llu Mo", SD_MMC.cardSize()/(1024*1024));
  } else {
    Serial.println("non présente");
  }
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

size_t camcapture() {
  camera_fb_t *fb = NULL;
  size_t ret;

  Serial.print("PSRAM libre : "); Serial.println(ESP.getFreePsram());

  for(int i=0; i<10; i++) {
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Erreur capture caméra !");
    } else {
      esp_camera_fb_return(fb);
      fb = NULL;
    }
    delay(300);
  }

  fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Erreur capture caméra !");
    return 0;
  }
  ret = fb->len;

  /********* BMP *********/
  /*
  size_t bmp_len;
  uint8_t *bmp;
  if(!fmt2bmp(fb->buf, fb->len, fb->width, fb->height, fb->format, &bmp, &bmp_len)) {
    Serial.println("Erreur fmt2bmp !");
    esp_camera_fb_return(fb);
    return 0;    
  }

  Serial.println("");
  Serial.println("Info BMP");
  Serial.print("  Taille      : ");
  Serial.println(bmp_len);
  Serial.print("  Header type : ");
  if(bmp[0] == 'B' && bmp[1] == 'M')
    Serial.println("BMP");
  else
    Serial.println("???");
  uint32_t bmp_offset = (bmp[13]<<24) | (bmp[12]<<16) | (bmp[11]<<8) | (bmp[10]);
  Serial.printf("  Offset Data : 0x%08X = %u\r\n", bmp_offset, bmp_offset);
  if(bmp_offset == 40+10+4) {
    Serial.println("  Type header : BITMAPINFOHEADER");
    int bmp_width = (bmp[21]<<24) | (bmp[20]<<16) | (bmp[19]<<8) | (bmp[18]);
    Serial.printf("  Width       : %d\r\n", bmp_width);
    int bmp_height = (bmp[25]<<24) | (bmp[24]<<16) | (bmp[23]<<8) | (bmp[22]);
    Serial.printf("  Height      : %d\r\n", bmp_height);
    uint32_t bmp_bpp = (bmp[29]<<8) | (bmp[28]);
    Serial.printf("  BPP         : %u\r\n", bmp_bpp);
    uint32_t bmp_comp = (bmp[31]<<8) | (bmp[30]);
    Serial.printf("  Compression : %u (%s)\r\n", bmp_comp, !bmp_comp?"BI_RGB":"???");
  }
  free(bmp);
  */

  /********* RGB *********/
  Serial.println("");
  uint8_t *rgb;
  size_t rgb_len = (fb->width)*(fb->height)*3;
  Serial.print("PSRAM libre : "); Serial.println(ESP.getFreePsram());
  if((rgb = (uint8_t*)ps_malloc(rgb_len)) == NULL) {
    Serial.println("Erreur ps_malloc !");
    esp_camera_fb_return(fb);
    return 0;
  }
  Serial.print("PSRAM libre : "); Serial.println(ESP.getFreePsram());
  if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb)) {
    Serial.println("Erreur fmt2rgb888 !");
    free(rgb);
    esp_camera_fb_return(fb);
    return 0;    
  }

  fb_data_t rgbfb;
  rgbfb.width = fb->width;
  rgbfb.height = fb->height;
  rgbfb.data = rgb;
  rgbfb.bytes_per_pixel = 3;
  rgbfb.format = FB_BGR888;

  fb_gfx_print(&rgbfb, (rgbfb.width - (strlen("COUCOU") * 14)) / 2, 10, COLOR_RED, "COUCOU");
  fb_gfx_fillRect(&rgbfb, rgbfb.width/2, rgbfb.height/2, 160, 10, COLOR_GREEN);
  fb_gfx_fillRect(&rgbfb, 40, 400, 14*12, 21, COLOR_WHITE);
  fb_gfx_printf(&rgbfb, 40, 400, COLOR_BLACK, "Val: %u+%u=%u", 40, 2, 40+2);


  /********* RGB->JPG *********/
  size_t jpg_len;
  uint8_t *jpg;
  if(!fmt2jpg(rgb, rgb_len, fb->width, fb->height, PIXFORMAT_RGB888, 80, &jpg, &jpg_len)) {
    Serial.println("Erreur fmt2jpg !");
    free(rgb);
    esp_camera_fb_return(fb);
    return 0;
  }

  File file1 = SD_MMC.open("/avant.jpg", FILE_WRITE);
  if(!file1){
    Serial.println("Erreur ouverture file1");
  } else {
    Serial.print("JPEG orig : ");
    Serial.println(fb->len);
    file1.write(fb->buf, fb->len);
    file1.close();
  }
  
  File file2 = SD_MMC.open("/apres.jpg", FILE_WRITE);
  if(!file2){
    Serial.println("Erreur ouverture file2");
  } else {
    Serial.print("JPEG nouv : ");
    Serial.println(jpg_len);
    file2.write(jpg, jpg_len);
    file2.close();
  }
  
  Serial.print("PSRAM libre : "); Serial.println(ESP.getFreePsram());
  free(rgb);
  free(jpg);

  Serial.print("PSRAM libre : "); Serial.println(ESP.getFreePsram());
  
  esp_camera_fb_return(fb);
  return ret;
}

void setup() {
  // compte tentative wifi
  int count = 0;

  // pinMode(WHITELED, OUTPUT);
  
  // moniteur série
  Serial.begin(115200);
  // Démarrage
  Serial.println("\r\nBoot...");

  // Lecture configuration Wifi
  EEPROM.begin(sizeof(readconf));
  EEPROM.get(0, readconf);

  // init SD
  if(!SD_MMC.begin()){
    Serial.println("Erreur SD !");
    return;
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

  if(!camcapture()) {
    Serial.println("Erreur camcapture() !");
  }

  SD_MMC.end();
}

void loop() {
  // gestion OTA
  ArduinoOTA.handle();
}
