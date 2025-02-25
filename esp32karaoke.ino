#include <Arduino.h>
#include <LiquidCrystal.h>

// ---- Pin Configuration ----
#define MSP_RX_PIN 16  // ESP32 receives data from MSP432 (Tx2 on MSP432)
#define MSP_TX_PIN 17  // ESP32 transmits data to MSP432 (Rx2 on MSP432)

// LCD pins (RS, E, D4, D5, D6, D7)
static const int LCD_RS = 4;
static const int LCD_EN = 5;
static const int LCD_D4 = 18;
static const int LCD_D5 = 19;
static const int LCD_D6 = 21;
static const int LCD_D7 = 22;

static const int POT_PIN = 34;
static unsigned long lastVolumeCheck = 0;
static const unsigned long VOLUME_CHECK_INTERVAL = 250;  // Check every 250ms
static int lastVolume = -1;                              // Track last sent volume

#define LCD_COLUMNS 16
#define LCD_ROWS 2

// ---- Global Variables ----
unsigned int isPlaying = 0;
unsigned int songIndex = 0;
unsigned int isReset = 0;

// ---- Objects ----
HardwareSerial SerialMSP(1);  // UART for MSP432
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  // Start serial at 115200 for debugging & receiving commands (USB)
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // Initialize LCD for 16 columns & 2 rows
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.clear();

  // Print a welcome message
  lcd.setCursor(0, 0);
  lcd.print("Ready To Play!");
  delay(1000);

  // Initialize UART1 for MSP432 Communication
  SerialMSP.begin(115200, SERIAL_8N1, MSP_RX_PIN, MSP_TX_PIN);

  Serial.println("Setup complete");
}

// -----------------------------------------------------------------------------
// HELPER: Handle L: Commands for Lyric Lines
// -----------------------------------------------------------------------------
void handleLyricCommand(const String& data) {
  // Example input: "L:HELLO|WORLD"
  if (!data.startsWith("L:")) return;

  // Remove "L:" prefix
  String payload = data.substring(2);
  payload.trim();

  // Split at '|'
  int sepPos = payload.indexOf('|');
  String line1 = (sepPos != -1) ? payload.substring(0, sepPos) : payload;
  String line2 = (sepPos != -1) ? payload.substring(sepPos + 1) : "";

  // Safely limit to 16 chars each
  if (line1.length() > 16) line1 = line1.substring(0, 16);
  if (line2.length() > 16) line2 = line2.substring(0, 16);

  // Clear the LCD and print
  lcd.clear();
  delay(5);  // Small delay to ensure command is processed
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastVolumeCheck >= VOLUME_CHECK_INTERVAL) {
    lastVolumeCheck = currentMillis;

    int potValue = analogRead(POT_PIN);
    int volume = map(potValue, 0, 4095, 0, 100);

    if (volume != lastVolume) {
      Serial.printf("V:%d\n", volume);
      lastVolume = volume;
    }
  }

  // 1) Check for lyrics from Python script via USB Serial
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() > 0 && data.startsWith("L:")) {
      handleLyricCommand(data);
    }
  }

  // 2) Check for commands from MSP432 via UART
  if (SerialMSP.available()) {
    String mspData = SerialMSP.readStringUntil('\n');
    mspData.trim();
    if (mspData.length() > 0) {
      Serial.println(mspData);  // Debug

      // Example: "P:1 S:2 R:0"
      unsigned int tempIsPlaying, tempSongIndex, tempIsReset;
      if (sscanf(mspData.c_str(), "P:%u S:%u R:%u",
                 &tempIsPlaying, &tempSongIndex, &tempIsReset)
          == 3) {

        // Only update if values changed
        if (isPlaying != tempIsPlaying || songIndex != tempSongIndex || isReset != tempIsReset) {

          isPlaying = tempIsPlaying;
          songIndex = tempSongIndex;
          isReset = tempIsReset;

          // Clear display on reset or new song
          if (isReset || tempSongIndex != songIndex) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Get Ready");
            lcd.setCursor(0, 1);
            lcd.print("To Sing!");
          }
        }
      }
    }
  }

  // Very short delay to avoid busy loop
  delay(2);
}
