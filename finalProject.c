#include "msp.h"

#include <stdint.h>
#include <stdbool.h>
#include "csHFXT.h"
#include "csLFXT.h"
#include "stepperMotor.h"
#include "lcd.h"

// RGB port and bit masks
#define RGB_PORT        P2              // Port 2
#define RGB_RED_PIN     0b00000001      // P2.0
#define RGB_GREEN_PIN   0b00000010      // P2.1
#define RGB_BLUE_PIN    0b00000100      // P2.2
#define RGB_ALL_PINS    (RGB_RED_PIN | RGB_GREEN_PIN | RGB_BLUE_PIN)

// Switch port and masks
#define SwitchPort      P3              // Port 3
#define SwitchNext         0b00000100      // P3.2
#define SwitchSelect      0b00001000      // P3.3
#define SwitchToggle         0b00100000      // P3.5
#define SwitchReset         0b01000000      // P3.6

#define LED_FLASHING_PERIOD 200         // milliseconds
#define SYSTEM_CLOCK_FREQUENCY 3000     // kHz
#define SINGLE_LOOP_CYCLES  88
#define CLK_FREQUENCY       48000000    // MCLK using 48MHz HFXT

// Delay for debouncing
#define DEBOUNCE_DELAY_CYCLES 5000

// LED colors and switch states
typedef enum _LEDcolors {
    RED, GREEN, BLUE, PURPLE, NONE
} LEDcolors;

typedef enum _SwitchState {
    NotPressed, Pressed
} SwitchState;

typedef enum MenuState {
    START_SCREEN, SELECT_SCREEN, PLAYING_SCREEN
} ScreenState;

ScreenState currentState = START_SCREEN;
int currentSong = 0;
int isPlaying = 0;

const char *songList[] = {"Happy", "Sad", "Excited", "Chill"};
#define SONG_COUNT (sizeof(songList) / sizeof(songList[0]))

// Function prototypes
void InitializeRGBLEDs(void);
void InitializeSwitches(void);
SwitchState CheckSwitchNext(void);
SwitchState CheckSwitchSelect(void);
SwitchState CheckSwitchToggle(void);
SwitchState CheckSwitchReset(void);
void debounce(void);
void SetLED(LEDcolors color);

// Global variables
LEDcolors CurrentLED = NONE;

int main(void)
{
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;  // Stop watchdog timer

    configHFXT();
    InitializeRGBLEDs();
    InitializeSwitches();
    initStepperMotor();
    enableStepperMotor();
    configLCD(CLK_FREQUENCY);
    initLCD();

    while (1) {
        handleButtonPress();
    }

    return 0;
}

void handleButtonPress() {
    static ScreenState lastState = (ScreenState)(-1);

    if (CheckSwitchReset() == Pressed) {
        currentState = START_SCREEN;
        currentSong = 0;
        isPlaying = 0;
        while (CheckSwitchReset() == Pressed) {}
        return;
    }

    if (currentState != lastState) {
        lastState = currentState;
        switch (currentState) {
        case START_SCREEN:
            lcdClearDisplay();
            lcdSetCursor(0, 0);
            lcdPrintString(" Karaoke Machine");
            lcdSetCursor(1, 0);
            lcdPrintString("  Press \"Next\"");
            break;
        case SELECT_SCREEN: {
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%d. %s", currentSong + 1, songList[currentSong]);
            lcdClearDisplay();
            lcdSetCursor(0, 0);
            lcdPrintString(buffer);
            break;
        }
        case PLAYING_SCREEN: {
            char buffer[20];
            lcdClearDisplay();
            lcdSetCursor(0, 0);
            snprintf(buffer, sizeof(buffer), "%s %s", isPlaying ? "> " : "||", songList[currentSong]);
            lcdPrintString(buffer);
            break;
        }
        }
    }

    switch (currentState) {
    case START_SCREEN:
        if (CheckSwitchSelect() == Pressed || CheckSwitchNext() == Pressed) {
            currentState = SELECT_SCREEN;
            while (CheckSwitchSelect() == Pressed || CheckSwitchNext() == Pressed) {}
        }
        break;
    case SELECT_SCREEN:
        if (CheckSwitchSelect() == Pressed) {
            currentState = PLAYING_SCREEN;
            while (CheckSwitchSelect() == Pressed) {}
        } else if (CheckSwitchNext() == Pressed) {
            currentSong = (currentSong + 1) % SONG_COUNT;
            lastState = (ScreenState)(-1);
            while (CheckSwitchNext() == Pressed) {}
        }
        break;
    case PLAYING_SCREEN:
        if (CheckSwitchToggle() == Pressed) {
            isPlaying = !isPlaying;
            lastState = (ScreenState)(-1);
            while (CheckSwitchToggle() == Pressed) {}
        }
        break;
    }
}


// Set the LED color
void SetLED(LEDcolors color)
{
    // Turn off all LEDs first
    RGB_PORT->OUT &= ~RGB_ALL_PINS;

    // Turn on the selected color
    switch (color) {
        case RED:
            RGB_PORT->OUT |= RGB_RED_PIN;
            break;
        case GREEN:
            RGB_PORT->OUT |= RGB_GREEN_PIN;
            break;
        case BLUE:
            RGB_PORT->OUT |= RGB_BLUE_PIN;
            break;
        case PURPLE:
            RGB_PORT->OUT |= (RGB_RED_PIN | RGB_BLUE_PIN);  // Red + Blue = Purple
            break;
        case NONE:
        default:
            // All LEDs are off
            break;
    }
}

// Debounce function to avoid switch bouncing issues
void debounce(void)
{
    int delay;
    for (delay = 0; delay < 5000; delay++);
}

// Check if Switch 1 is pressed
SwitchState CheckSwitchNext(void)
{
    if (!(SwitchPort->IN & SwitchNext)) {
        debounce();
        return (!(SwitchPort->IN & SwitchNext)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 2 is pressed
SwitchState CheckSwitchSelect(void)
{
    if (!(SwitchPort->IN & SwitchSelect)) {
        debounce();
        return (!(SwitchPort->IN & SwitchSelect)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 3 is pressed
SwitchState CheckSwitchToggle(void)
{
    if (!(SwitchPort->IN & SwitchToggle)) {
        debounce();
        return (!(SwitchPort->IN & SwitchToggle)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 4 is pressed
SwitchState CheckSwitchReset(void)
{
    if (!(SwitchPort->IN & SwitchReset)) {
        debounce();
        return (!(SwitchPort->IN & SwitchReset)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Initialize RGB LEDs
void InitializeRGBLEDs(void)
{
    // Set RGB pins as output
    RGB_PORT->DIR |= RGB_ALL_PINS;

    // Turn off all LEDs initially
    RGB_PORT->OUT &= ~RGB_ALL_PINS;
}

// Initialize switches
void InitializeSwitches(void)
{
    // Set switch pins as input and enable pull-up resistors
    SwitchPort->DIR &= ~(SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);  // Set as input
    SwitchPort->REN |= (SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);   // Enable pull resistors
    SwitchPort->OUT |= (SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);   // Set pull-up mode
}
