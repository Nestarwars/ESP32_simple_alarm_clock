#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"
#include <Fonts/FreeSans9pt7b.h>

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// buttons
#define show_touch 19  //D19
#define touch1 15
#define touch2 16
#define touch3 17
#define touch4 18

int show_touch_state = 0;
int previous = 0;

// Alarm variables
int alarm_hour = 7;
int alarm_minute = 0;
boolean alarm_enabled = false;
boolean alarm_ringing = false;
unsigned long alarm_flash_start = 0;
const int alarm_flash_duration = 10000; // flash for 10 seconds

// Button debounce
unsigned long last_button_press = 0;
const int debounce_delay = 300; // milliseconds

// Mode for button controls (0=normal, 1=setting alarm hour, 2=setting alarm minute)
int settings_mode = 0;
unsigned long settings_mode_timeout = 0;
const int settings_timeout = 10000; // exit settings mode after 10 seconds

// WiFi credentials
const char* ssid = "GalaxyBox";
const char* password = "7ewkfoig25nov1915rel1stein";

// Wifi icon
const unsigned char wifiicon[] PROGMEM  ={ // wifi icon
  0x00, 0xff, 0x00, 0x7e, 0x00, 0x18,0x00, 0x00
};

// Time config (India: UTC+5:30)
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// refresh every second
const int refresh_time = 1000;
const int fade_out_time = 3;
// adapt refresh with fade out
const int remaining_time = refresh_time - 255 * fade_out_time;

unsigned long startTime = millis();



void setup() {
  Serial.begin(115200);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  pinMode(show_touch, INPUT);//Setting input mode
  pinMode(touch1, INPUT);//Setting input mode
  pinMode(touch2, INPUT);//Setting input mode
  pinMode(touch3, INPUT);//Setting input mode
  pinMode(touch4, INPUT);//Setting input mode

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    delay(200);
    Serial.print(".");
    drawCentreString(ssid, 64, 20);
    drawCentreString("Connecting", 64, 0);
    display.display();
    delay(200);
    display.clearDisplay();
  }
  Serial.println("\nWiFi connected");

  display.clearDisplay();
  drawCentreString("Connected", 64, 0);
  display.display();
  delay(500);
  display.clearDisplay();
  display.display();

  // Initialize OLED
  display.clearDisplay();
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(255);
  // display.setFont(&FreeSans9pt7b);

  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(200);

  // WiFi.disconnect();
  // WiFi.mode(WIFI_OFF);
  // drawCentreString("Turning off Wifi", 64, 0);
  // display.display();
  // delay(500);
  // display.clearDisplay();
  
}

void loop() {

  show_touch_state = digitalRead(show_touch);//Read the value of the key

  unsigned long currentTime = millis();
  
  // Check alarm trigger
  checkAlarm();
  
  // Handle alarm ringing - flash screen
  if (alarm_ringing) {
    handleAlarmFlash();
  }
  
  if (show_touch_state == 1){
    
    if (currentTime - startTime > 1000){
      startTime = currentTime;
      if (settings_mode == 0) {
        printTime();
      } else {
        printSettingsScreen();
      }
    }

    if (previous==0){
      fadein(2000);
      previous = 1;
    }
  }

  else {
    if (previous==1){
      fadeout(2000);
      previous = 0;
    }
    display.clearDisplay();
    display.display();
  }

  // Button handling with debounce
  unsigned long now = millis();
  
  if (digitalRead(touch1) == 1 && (now - last_button_press > debounce_delay)){
    last_button_press = now;
    handleButton1();
  }

  if (digitalRead(touch2) == 1 && (now - last_button_press > debounce_delay)){
    last_button_press = now;
    handleButton2();
  }

  if (digitalRead(touch3) == 1 && (now - last_button_press > debounce_delay)){
    last_button_press = now;
    handleButton3();
  }

  if (digitalRead(touch4) == 1 && (now - last_button_press > debounce_delay)){
    last_button_press = now;
    handleButton4();
  }
  
  // Exit settings mode on timeout
  if (settings_mode > 0 && (now - settings_mode_timeout > settings_timeout)) {
    settings_mode = 0;
  }

}


void fadeout(int duration) {
  int delay_ms = duration / (16+2);

  for (int dim=160; dim>=0; dim-=10) {
    printTime();
    display.ssd1306_command(0x81);
    display.ssd1306_command(dim); //max 157
    delay(delay_ms);
  }
  
  
  for (int dim2=34; dim2>=0; dim2-=17) {
    printTime();
    display.ssd1306_command(0xD9);
    display.ssd1306_command(dim2);  //max 34
    delay(delay_ms);
  }
}

void fadein(int duration) {

  int delay_ms = duration / (16+2);

  for (int dim2=0; dim2<=34; dim2+=17) {
    printTime();
    display.ssd1306_command(0xD9);
    display.ssd1306_command(dim2);  //max 34
    delay(delay_ms);
  }

  for (int dim=0; dim<=160; dim+=10) {
    printTime();
    display.ssd1306_command(0x81);
    display.ssd1306_command(dim); //max 160
    delay(delay_ms);
  }
}

void drawCentreString(const char *buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, 0, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w / 2, y);
    display.print(buf);
}

void printTime() {
  
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Format 12-hour time
  char timeStr[8];
  int hour = timeinfo.tm_hour;
  sprintf(timeStr, "%02d:%02d", hour, timeinfo.tm_min);
  char secStr[3];
  sprintf(secStr, "%02d", timeinfo.tm_sec);

  // Format date in one line (e.g., "24 Apr 2025")
  char dateStr[12];
  strftime(dateStr, sizeof(dateStr), "%d %b %Y", &timeinfo);

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  drawCentreString(dateStr, 64, 0);

  display.setTextSize(4);
  drawCentreString(timeStr, 64, 15);

  display.setTextSize(2);
  drawCentreString(secStr, 64, 48);

  if (WiFi.status() == WL_CONNECTED) {
      display.drawBitmap(120,1,wifiicon,8,8,WHITE);
  }
    
  display.display();

}

void printTest(const char *str){
  display.clearDisplay();
  display.setTextSize(5);
  drawCentreString(str, 64, 15);
  display.display();
  delay(50);
}
// Alarm functionality
void checkAlarm() {
  if (!alarm_enabled || alarm_ringing) return;
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  // Check if current time matches alarm time
  if (timeinfo.tm_hour == alarm_hour && timeinfo.tm_min == alarm_minute) {
    alarm_ringing = true;
    alarm_flash_start = millis();
    Serial.println("ALARM RINGING!");
  }
}

void handleAlarmFlash() {
  unsigned long elapsed = millis() - alarm_flash_start;
  
  // Stop flashing after duration
  if (elapsed > alarm_flash_duration) {
    alarm_ringing = false;
    return;
  }
  
  // Flash the screen - toggle every 200ms
  if ((elapsed / 200) % 2 == 0) {
    display.ssd1306_command(0x81);
    display.ssd1306_command(255); // full brightness
  } else {
    display.ssd1306_command(0x81);
    display.ssd1306_command(0); // off
  }
}

// Button handlers
void handleButton1() {
  // Button 1: Enter/exit settings mode or increment hour
  if (settings_mode == 0) {
    // Enter settings mode
    settings_mode = 1;
    settings_mode_timeout = millis();
    Serial.println("Entering alarm time settings - Hour");
  } else if (settings_mode == 1) {
    // Increment hour
    alarm_hour = (alarm_hour + 1) % 24;
    settings_mode_timeout = millis();
    Serial.print("Hour: ");
    Serial.println(alarm_hour);
  } else if (settings_mode == 2) {
    // Move back to hour setting
    settings_mode = 1;
    settings_mode_timeout = millis();
  }
}

void handleButton2() {
  // Button 2: Move to minute setting or increment minute
  if (settings_mode == 1) {
    // Move to minute setting
    settings_mode = 2;
    settings_mode_timeout = millis();
    Serial.println("Setting alarm time - Minute");
  } else if (settings_mode == 2) {
    // Increment minute
    alarm_minute = (alarm_minute + 1) % 60;
    settings_mode_timeout = millis();
    Serial.print("Minute: ");
    Serial.println(alarm_minute);
  }
}

void handleButton3() {
  // Button 3: Toggle alarm enabled/disabled
  alarm_enabled = !alarm_enabled;
  // Stop flashing if alarm is disabled while ringing
  if (!alarm_enabled && alarm_ringing) {
    alarm_ringing = false;
    display.ssd1306_command(0x81);
    display.ssd1306_command(160); // restore normal brightness
  }
  Serial.print("Alarm: ");
  Serial.println(alarm_enabled ? "ENABLED" : "DISABLED");
}

void handleButton4() {
  // Button 4: Save settings and exit
  settings_mode = 0;
  Serial.println("Alarm settings saved");
  Serial.print("Alarm set to: ");
  Serial.print(alarm_hour);
  Serial.print(":");
  if (alarm_minute < 10) Serial.print("0");
  Serial.println(alarm_minute);
}

void printSettingsScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Draw header
  drawCentreString("SET ALARM", 64, 0);
  
  // Draw alarm time in large font
  display.setTextSize(3);
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", alarm_hour, alarm_minute);
  
  if (settings_mode == 1) {
    // Highlight hour
    drawCentreString(timeStr, 64, 20);
    display.setTextSize(1);
    drawCentreString("^ Hour", 64, 45);
  } else if (settings_mode == 2) {
    // Highlight minute
    drawCentreString(timeStr, 64, 20);
    display.setTextSize(1);
    drawCentreString("^ Minute", 64, 45);
  }
  
  // Draw instructions
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("1:Inc 2:Min 3:Tog 4:Save");
  
  display.display();
}