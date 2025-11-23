#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_AHTX0.h>

// ===== Screen pins =====
#define TFT_CS   9
#define TFT_DC   10
#define TFT_RST  11
#define TFT_MOSI 12
#define TFT_SCLK 13
#define TFT_BL   8

#define I2C_SDA 47
#define I2C_SCL 38

// ===== LED pins =====
int led[4] = { 18, 17, 16, 15 };
int ledActive = -1;

// ===== Colors =====
#define COLOR_BG        0x0000
#define COLOR_TEXT      0xFFFF
#define COLOR_RING_TEMP 0x07E0
#define COLOR_RING_HUM  0x5D9B
uint16_t season_color[] = {
  0x04BB,
  0x0666,
  0xF800,
  0xFD60
};

// ===== Objects =====
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_AHTX0 aht;

// ===== WiFi / NTP =====
const char* ssid = "YourSSID";
const char* password = "YourWPA";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int  daylightOffset_sec = 3600;

// ===== Screen layout =====
const int SCREEN_W = 240;
const int SCREEN_H = 240;
const int CX = SCREEN_W/2;
const int CY = SCREEN_H/2;
const int OUTER_R = 110;
const int SMALL_R = 22;

// ===== Fonts =====
#include <Fonts/digital_740pt7b.h>
#include <Fonts/digital_711pt7b.h>

int last_minute = -1;
float lastTemp=0, lastHum=0;

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  aht.begin();

  // --- TFT ---
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(COLOR_BG);

  // --- LEDs ---
  for (int l = 0; l < 4; l++)
  {
    pinMode(led[l], OUTPUT);
  }

  // --- WiFi ---
  WiFi.begin(ssid, password);
  int ledRandom = random(0,4);
  while (WiFi.status() != WL_CONNECTED) {
    if (digitalRead(led[ledRandom]) == LOW)
      digitalWrite(led[ledRandom], HIGH);
    else
      digitalWrite(led[ledRandom], LOW);

    delay(250);
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;

  if (ti.tm_min != last_minute) {
    readSensors();
    drawFullFace();
    last_minute = ti.tm_min;

    if (ti.tm_min == 0 || ti.tm_min == 30)
    {
      ledsBlinking();
    }
  }  
}

// ====== SENSOR READING ======
void readSensors() {
  sensors_event_t hum, temp;
  aht.getEvent(&hum, &temp);
  lastTemp = temp.temperature;
  lastHum = hum.relative_humidity;
}

// ====== DISPLAY FUNCTIONS ======
void drawFullFace() {
  tft.fillScreen(COLOR_BG);
  drawOuterRing();
  
  drawMiniCircle(CX - 40, CY - 50, SMALL_R, lastTemp, -10, 50, "°C", COLOR_RING_TEMP);
  drawMiniCircle(CX + 40, CY - 50, SMALL_R, lastHum, 0, 100, "%", COLOR_RING_HUM);

  drawCentralTime();
  drawDateLine();
}

void drawOuterRing() {
  const int OUTER_R = 110;
  const int CX = 120, CY = 120;
  const int THICKNESS = 6;
  const int HALF = THICKNESS / 2;

  struct tm ti;
  if (!getLocalTime(&ti)) return;

  for (int i = 0; i < 360; i += 3) {
    float a = (i - 90) * PI / 180.0;
    int x1 = CX + (OUTER_R - HALF) * cos(a);
    int y1 = CY + (OUTER_R - HALF) * sin(a);
    int x2 = CX + (OUTER_R + HALF) * cos(a);
    int y2 = CY + (OUTER_R + HALF) * sin(a);
    tft.drawLine(x1, y1, x2, y2, tft.color565(40, 40, 40));
  }
 
  int season = getSeason();

  for (int m = 0; m < ti.tm_min; m++) {
    float a = (m * 6 - 90) * PI / 180.0;

    int x1 = CX + (OUTER_R - HALF) * cos(a);
    int y1 = CY + (OUTER_R - HALF) * sin(a);
    int x2 = CX + (OUTER_R + HALF) * cos(a);
    int y2 = CY + (OUTER_R + HALF) * sin(a);
    int xm = (x1 + x2) / 2;
    int ym = (y1 + y2) / 2;

    tft.fillCircle(x1, y1, HALF + 1, season_color[season]);
    tft.fillCircle(x2, y2, HALF + 1, season_color[season]);
    tft.fillCircle(xm, ym, HALF + 1, season_color[season]);
  }
}

void drawMiniCircle(int cx, int cy, int r, float value, float vmin, float vmax, const char* unit, uint16_t color) {
  int newR = r + 6;
  tft.fillCircle(cx, cy, newR, 0x1082);
  tft.drawCircle(cx, cy, newR, color);

  char buf[16];
  if (strcmp(unit, "°C") == 0) snprintf(buf, sizeof(buf), "%.0f'C", value);
  else if (strcmp(unit, "%") == 0) snprintf(buf, sizeof(buf), "%.0f%%", value);

  tft.setFont(&digital_711pt7b);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(COLOR_TEXT, 0x1082);
  tft.setCursor(cx - w / 2, cy + h / 2.25);
  tft.print(buf);
}

void drawCentralTime() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);

  tft.setFont(&digital_740pt7b);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CX - w/2, CY + 42);
  tft.print(buf);
}

void drawDateLine() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
  const char *wdays[] = {"DIM","LUN","MAR","MER","JEU","VEN","SAM"};
  char dstr[32];
  snprintf(dstr, sizeof(dstr), "%s  %02d/%02d/%04d", wdays[ti.tm_wday], ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);

  for (int l = 0; l < 4; l++)
  {
    digitalWrite(led[l], LOW);
  }

  digitalWrite(led[getSeason()], HIGH);

  tft.setFont(&digital_711pt7b);
  int16_t x1,y1; uint16_t w,h;
  tft.getTextBounds(dstr, 0,0,&x1,&y1,&w,&h);
  tft.setTextColor(season_color[getSeason()], COLOR_BG);
  tft.setCursor(CX - w/2, CY + 70);
  tft.print(dstr);
}

int getSeason()
{
  struct tm ti;
  if (!getLocalTime(&ti)) return - 1;

  if ((ti.tm_mday >= 21 && ti.tm_mon + 1 == 12 ) || ti.tm_mon + 1 == 1 || ti.tm_mon + 1 == 2 || (ti.tm_mday < 20 && ti.tm_mon + 1 == 3))
    return 0;
  else if ((ti.tm_mday >= 20 && ti.tm_mon + 1 == 3 ) || ti.tm_mon + 1 == 4 || ti.tm_mon + 1 == 5 || (ti.tm_mday < 21 && ti.tm_mon + 1 == 6))
    return 1;
  else if ((ti.tm_mday >= 21 && ti.tm_mon + 1 == 6 ) || ti.tm_mon + 1 == 7 || ti.tm_mon + 1 == 8 || (ti.tm_mday < 23 && ti.tm_mon + 1 == 9))
    return 2;
  else
    return 3;
}

void ledsBlinking()
{
  int time = 80;

  for (int i = 0; i < 4; i++)
  {
    time = random(30, 100);

    digitalWrite(led[ledActive], LOW);
    delay(time / 1.5);
    digitalWrite(led[ledActive], HIGH);
    delay(time);
  }
}
