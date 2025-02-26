#include <Arduino.h>
#include <LiquidCrystal.h>

//PIN DEFINITIONS
#define MSP_RX_PIN 16  // ESP32 receives data from MSP432 (Tx2 on MSP432)
#define MSP_TX_PIN 17  // ESP32 transmits data to MSP432 (Rx2 on MSP432)

static const int LCD_RS = 4;
static const int LCD_EN = 5;
static const int LCD_D4 = 18;
static const int LCD_D5 = 19;
static const int LCD_D6 = 21;
static const int LCD_D7 = 22;

static const int POT_PIN = 34;
static unsigned long lastVolumeCheck = 0;
static const unsigned long VOLUME_CHECK_INTERVAL = 250;
static int lastVolume = -1;

#define LCD_COLUMNS 16
#define LCD_ROWS 2

// Variables
unsigned int isPlaying = 0;
unsigned int songIndex = 0;
unsigned int isReset = 0;

//Initializing UART and LCD
HardwareSerial SerialMSP(1);  // UART for MSP432
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

//SETUP FUNCTION
void setup() {
  Serial.begin(115200); //ESP32 to PC UART
  while (!Serial) { delay(10); }

  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.clear();

  // Displays welcome screen
  lcd.setCursor(0, 0);
  lcd.print("Ready To Play!");
  delay(1000);

  //MSP432 to ESP32 UART
  SerialMSP.begin(115200, SERIAL_8N1, MSP_RX_PIN, MSP_TX_PIN);

  Serial.println("Setup complete");
}

//Extracts PSR command data
void handleLyricCommand(const String& data) {
  if (!data.startsWith("L:")) return;

  // Removes "L:" prefix
  String payload = data.substring(2);
  payload.trim();

  // Splits at '|'
  int sepPos = payload.indexOf('|');
  String line1 = (sepPos != -1) ? payload.substring(0, sepPos) : payload;
  String line2 = (sepPos != -1) ? payload.substring(sepPos + 1) : "";

  // Limits each output to 16 characters
  if (line1.length() > 16) line1 = line1.substring(0, 16);
  if (line2.length() > 16) line2 = line2.substring(0, 16);

  // Clears the LCD and prints
  lcd.clear();
  delay(5);
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

//Main Loop
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastVolumeCheck >= VOLUME_CHECK_INTERVAL) { 
    lastVolumeCheck = currentMillis;

    int potValue = analogRead(POT_PIN); // Gets new volume data from potentiometer
    int volume = map(potValue, 0, 4095, 0, 100);

    if (volume != lastVolume) {
      Serial.printf("V:%d\n", volume); //Sends volume data to PC via UART
      lastVolume = volume;
    }
  }

  // Checks for lyrics from Python script
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() > 0 && data.startsWith("L:")) {
      handleLyricCommand(data);
    }
  }

  // Checks for commands from MSP432
  if (SerialMSP.available()) {
    String mspData = SerialMSP.readStringUntil('\n');
    mspData.trim();
    if (mspData.length() > 0) {
      Serial.println(mspData);

      unsigned int tempIsPlaying, tempSongIndex, tempIsReset;
      if (sscanf(mspData.c_str(), "P:%u S:%u R:%u",
                 &tempIsPlaying, &tempSongIndex, &tempIsReset)
          == 3) {

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

  delay(2);
}
