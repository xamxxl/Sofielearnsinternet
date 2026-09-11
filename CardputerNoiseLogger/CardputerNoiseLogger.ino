#include <M5Unified.h>
#include <SD.h>
#include <driver/i2s.h>
#include <time.h>

#define SAMPLE_RATE 16000
#define SAMPLE_BITS I2S_BITS_PER_SAMPLE_16BIT
#define CHANNELS 1
#define BUFFER_SIZE 1024
float noiseThresholdDb = 50.0;
#define LOG_INTERVAL_MS 1000

enum AppState {
  STATE_START_SCREEN,
  STATE_TIME_MENU,
  STATE_LOGGING
};

AppState currentState = STATE_START_SCREEN;
int timeMenuSelection = 0;
int timeValues[6] = {2024, 1, 1, 0, 0, 0};
const char* timeLabels[6] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
int timeMaxValues[6] = {2099, 12, 31, 23, 59, 59};
int timeMinValues[6] = {2024, 1, 1, 0, 0, 0};

File logFile;
bool sdCardReady = false;
bool loggingActive = false;
unsigned long lastLogTime = 0;
int16_t audioBuffer[BUFFER_SIZE];
float currentDb = 0.0;

char serialBuffer[32];
int serialBufferIndex = 0;

void setup() {
  M5.begin();
  
  Serial.begin(115200);
  
  M5.Display.setRotation(0);
  M5.Display.setBrightness(100);
  M5.Display.setTextSize(2);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setCursor(10, 10);
  M5.Display.print("Cardputer");
  M5.Display.setCursor(10, 40);
  M5.Display.print("Booting...");
  
  initSDCard();
  initMicrophone();
  showStartScreen();
}

void loop() {
  M5.update();
  handleSerialInput();
  
  switch (currentState) {
    case STATE_START_SCREEN:
      handleStartScreen();
      break;
    case STATE_TIME_MENU:
      handleTimeMenu();
      break;
    case STATE_LOGGING:
      handleLogging();
      break;
  }
  
  delay(10);
}

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBufferIndex > 0) {
        serialBuffer[serialBufferIndex] = '\0';
        processSerialCommand(serialBuffer);
        serialBufferIndex = 0;
      }
    } else if (serialBufferIndex < 31) {
      serialBuffer[serialBufferIndex++] = c;
    }
  }
}

void processSerialCommand(const char* cmd) {
  if (strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0) {
    if (currentState == STATE_START_SCREEN) {
      currentState = STATE_TIME_MENU;
      timeMenuSelection = 0;
      drawTimeMenu();
    } else if (currentState == STATE_TIME_MENU) {
      if (sdCardReady) {
        startLogging();
      } else {
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setTextSize(2);
        M5.Display.setCursor(10, 100);
        M5.Display.print("SD Card Required!");
        delay(2000);
        drawTimeMenu();
      }
    }
  } else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "P") == 0) {
    if (currentState == STATE_LOGGING) {
      stopLogging();
    }
  } else if (strcmp(cmd, "u") == 0 || strcmp(cmd, "U") == 0) {
    if (currentState == STATE_TIME_MENU) {
      timeMenuSelection = (timeMenuSelection - 1 + 6) % 6;
      drawTimeMenu();
    }
  } else if (strcmp(cmd, "d") == 0 || strcmp(cmd, "D") == 0) {
    if (currentState == STATE_TIME_MENU) {
      timeMenuSelection = (timeMenuSelection + 1) % 6;
      drawTimeMenu();
    }
  } else if (strcmp(cmd, "l") == 0 || strcmp(cmd, "L") == 0) {
    if (currentState == STATE_TIME_MENU) {
      timeValues[timeMenuSelection]--;
      if (timeValues[timeMenuSelection] < timeMinValues[timeMenuSelection]) {
        timeValues[timeMenuSelection] = timeMaxValues[timeMenuSelection];
      }
      if (timeMenuSelection == 1) {
        timeMaxValues[2] = getDaysInMonth(timeValues[0], timeValues[1]);
      }
      drawTimeMenu();
    } else if (currentState == STATE_LOGGING) {
      adjustThreshold(-1.0);
    }
  } else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "R") == 0) {
    if (currentState == STATE_TIME_MENU) {
      timeValues[timeMenuSelection]++;
      if (timeValues[timeMenuSelection] > timeMaxValues[timeMenuSelection]) {
        timeValues[timeMenuSelection] = timeMinValues[timeMenuSelection];
      }
      if (timeMenuSelection == 1) {
        timeMaxValues[2] = getDaysInMonth(timeValues[0], timeValues[1]);
      }
      drawTimeMenu();
    } else if (currentState == STATE_LOGGING) {
      adjustThreshold(1.0);
    }
  } else if (strcmp(cmd, "a") == 0 || strcmp(cmd, "A") == 0) {
    if (currentState == STATE_TIME_MENU) {
      setSystemTime();
      drawTimeMenu();
    }
  } else if (strcmp(cmd, "t") == 0 || strcmp(cmd, "T") == 0) {
    if (currentState == STATE_LOGGING) {
      adjustThresholdInteractive();
    }
  }
}

void showStartScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(20, 40);
  M5.Display.print("Cardputer");
  M5.Display.setCursor(20, 80);
  M5.Display.print("Noise Logger");
  
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setCursor(10, 140);
  M5.Display.print("Press 's' to continue...");
  
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(10, 160);
  if (sdCardReady) {
    M5.Display.print("SD Card: OK");
  } else {
    M5.Display.print("SD Card: NOT FOUND");
  }
  
  M5.Display.setCursor(10, 180);
  M5.Display.print("Mic: Ready");
}

void handleStartScreen() {
  // Handled via serial command 's'
}

void drawTimeMenu() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(10, 10);
  M5.Display.print("Set Current Time");
  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 40);
  M5.Display.print("u/d: Select field");
  M5.Display.setCursor(10, 55);
  M5.Display.print("l/r: Change value");
  M5.Display.setCursor(10, 70);
  M5.Display.print("a: Confirm time");
  M5.Display.setCursor(10, 85);
  M5.Display.print("s: Start logging");
  
  for (int i = 0; i < 6; i++) {
    int y = 110 + i * 20;
    if (i == timeMenuSelection) {
      M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    } else {
      M5.Display.setTextColor(TFT_WHITE);
    }
    M5.Display.setCursor(10, y);
    M5.Display.printf("%s: %02d", timeLabels[i], timeValues[i]);
  }
}

void handleTimeMenu() {
  // Handled via serial commands
}

void setSystemTime() {
  struct tm timeinfo = {0};
  timeinfo.tm_year = timeValues[0] - 1900;
  timeinfo.tm_mon = timeValues[1] - 1;
  timeinfo.tm_mday = timeValues[2];
  timeinfo.tm_hour = timeValues[3];
  timeinfo.tm_min = timeValues[4];
  timeinfo.tm_sec = timeValues[5];
  
  time_t t = mktime(&timeinfo);
  struct timeval tv = {t, 0};
  settimeofday(&tv, NULL);
  
  M5.Display.fillScreen(TFT_GREEN);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 100);
  M5.Display.print("Time Set!");
  delay(1000);
}

int getDaysInMonth(int year, int month) {
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
      return 29;
    }
    return 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  }
  return 31;
}

void initSDCard() {
  if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    sdCardReady = false;
    return;
  }
  sdCardReady = true;
}

void initMicrophone() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = GPIO_NUM_33,
    .ws_io_num = GPIO_NUM_34,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = GPIO_NUM_35
  };
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void startLogging() {
  struct tm timeinfo;
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  
  char filename[32];
  snprintf(filename, sizeof(filename), "/noise_%04d%02d%02d.csv", 
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  
  logFile = SD.open(filename, FILE_APPEND);
  if (!logFile) {
    logFile = SD.open(filename, FILE_WRITE);
    if (logFile) {
      logFile.println("Timestamp,dB");
    }
  }
  
  if (logFile) {
    loggingActive = true;
    currentState = STATE_LOGGING;
    lastLogTime = millis();
    showLoggingScreen();
  }
}

void showLoggingScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(10, 20);
  M5.Display.print("LOGGING ACTIVE");
  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 60);
  M5.Display.print("Threshold: ");
  M5.Display.print(noiseThresholdDb);
  M5.Display.print(" dB");
  
  M5.Display.setCursor(10, 80);
  M5.Display.print("p: Pause/Stop");
  M5.Display.setCursor(10, 95);
  M5.Display.print("t: Adjust Threshold");
  M5.Display.setCursor(10, 110);
  M5.Display.print("l/r: Change Threshold");
}

void handleLogging() {
  readMicrophone();
  calculateDb();
  
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(10, 130);
  M5.Display.printf("Current: %.1f dB", currentDb);
  
  if (currentDb > noiseThresholdDb) {
    M5.Display.fillRect(10, 160, 220, 40, TFT_RED);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(20, 170);
    M5.Display.print("NOISE DETECTED!");
    logNoiseEvent();
  } else {
    M5.Display.fillRect(10, 160, 220, 40, TFT_GREEN);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(20, 170);
    M5.Display.print("Quiet");
  }
}

void readMicrophone() {
  size_t bytesRead = 0;
  i2s_read(I2S_NUM_0, audioBuffer, BUFFER_SIZE * 2, &bytesRead, portMAX_DELAY);
}

void calculateDb() {
  long sum = 0;
  int samples = BUFFER_SIZE;
  
  for (int i = 0; i < samples; i++) {
    long val = audioBuffer[i];
    sum += val * val;
  }
  
  float rms = sqrt((float)sum / samples);
  currentDb = 20.0 * log10(rms / 32768.0) + 94.0;
  
  if (currentDb < 0) currentDb = 0;
  if (currentDb > 120) currentDb = 120;
}

void logNoiseEvent() {
  if (!logFile) return;
  
  unsigned long now = millis();
  if (now - lastLogTime < LOG_INTERVAL_MS) return;
  lastLogTime = now;
  
  time_t t = time(nullptr);
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  
  char timestamp[32];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  logFile.printf("%s,%.1f\n", timestamp, currentDb);
  logFile.flush();
}

void adjustThreshold(float delta) {
  noiseThresholdDb += delta;
  if (noiseThresholdDb < 30.0) noiseThresholdDb = 30.0;
  if (noiseThresholdDb > 100.0) noiseThresholdDb = 100.0;
  
  M5.Display.fillRect(10, 60, 220, 20, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(10, 60);
  M5.Display.print("Threshold: ");
  M5.Display.print(noiseThresholdDb, 1);
  M5.Display.print(" dB");
}

void adjustThresholdInteractive() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(10, 50);
  M5.Display.print("Adjust Threshold");
  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 100);
  M5.Display.print("l/r: Change");
  M5.Display.setCursor(10, 120);
  M5.Display.print("a: Confirm");
  
  bool adjusting = true;
  
  while (adjusting) {
    M5.update();
    handleSerialInput();
    
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'a' || c == 'A') {
        adjusting = false;
      } else if (c == 'l' || c == 'L') {
        adjustThreshold(-1.0);
      } else if (c == 'r' || c == 'R') {
        adjustThreshold(1.0);
      }
    }
    
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.fillRect(10, 160, 220, 50, TFT_BLACK);
    M5.Display.setCursor(50, 170);
    M5.Display.printf("%.1f dB", noiseThresholdDb);
    
    delay(50);
  }
  
  showLoggingScreen();
}

void stopLogging() {
  loggingActive = false;
  if (logFile) {
    logFile.close();
    logFile = File();
  }
  currentState = STATE_TIME_MENU;
  drawTimeMenu();
}