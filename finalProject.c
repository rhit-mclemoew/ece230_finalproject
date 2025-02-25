#include "msp.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "csHFXT.h"
#include "csLFXT.h"
#include "stepperMotor.h"
#include "lcd.h"
#include "uart.h"
#include "timer32.h"

#define PLAYBACK_LED_PORT    P2    // Using Port 2
#define PLAYBACK_LED_PIN     BIT3  // LED connected to P2.3


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

// Pressed States
typedef enum _SwitchState {
    NotPressed, Pressed
} SwitchState;

//Initial variables
ScreenState currentState = START_SCREEN;
uint8_t currentSong = 0;
uint8_t isPlaying = 0;
uint8_t isReset = 0;

//song list
const char *songList[] = {
    "Again-Fetty Wap",
    "Friends in Low Places-Garth Brooks",
    "Happy-Pharrell",
    "Suspicious Minds-Elvis Presley",
    "One More Time-Daft Punk",
    "Stronger-Kanye West",
    "Billie Jean-Michael Jackson",
    "Tennessee Whiskey-Chris Stapleton",
    "Chop Suey-System of a Down",
    "One Last Breath-Creed",
    "A Thousand Miles-Vanessa Carlton",
    "Blue Jean Baby-Zach Bryan",
    "Somebody That I Used To Know-Gotye",
    "Yellow-Coldplay",
    "Hey There Delilah-Plain White T's",
    "Grenade-Bruno Mars",
    "Starboy-The Weeknd",
    "Like a Rolling Stone-Bob Dylan",
    "Payphone-Maroon 5",
    "Boulevard of Broken Dreams-Green Day",
    "Circles-Post Malone",
    "Stressed Out-Twenty One Pilots",
    "Bitter Sweet Symphony-The Verve",
    "Runaway-Kanye West",
    "Ghost Riders in the Sky-Johnny Cash",
    "My Way-Frank Sinatra",
    "We Are The Champions-Queen"
};

//song count
#define SONG_COUNT (sizeof(songList) / sizeof(songList[0]))

// Function prototypes
void InitializeSwitches(void);
SwitchState CheckSwitchNext(void);
SwitchState CheckSwitchSelect(void);
SwitchState CheckSwitchToggle(void);
SwitchState CheckSwitchReset(void);
void debounce(void);
void handleButtonPress(void);
extern void play_serial_audio_stereo(void);
void InitializePlaybackLED(void);

volatile uint8_t updateLCD = 0;

// Global variables
LEDcolors CurrentLED = NONE;

//main function
int main(void)
{
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;  // Stop watchdog timer

    //initializing everything
    configHFXT();
    Timer32_Init();
    InitializePlaybackLED();
    InitializeSwitches();
    initStepperMotor();
    enableStepperMotor();
    configLCD(CLK_FREQUENCY);
    initLCD();
    initUART();

    //while loop
    while (1) {
        handleButtonPress();
        if (updateLCD) {
            updateLCD = 0;  // Reset flag
            lcdDisplayTitleArtist(songList[currentSong]); //calls the updating scrolling lcd text function
        }
        __no_operation();
    }

}

//main state machine, handles button presses and other important features
void handleButtonPress() {
    static ScreenState lastState = (ScreenState)(-1);

    if (CheckSwitchReset() == Pressed) { //reset button
        currentState = START_SCREEN;
        currentSong = 0;
        isPlaying = 0;
        isReset = 1;
        sendPlaybackStatus(isPlaying, currentSong, isReset); // sends uart command
        isReset = 0;
        PLAYBACK_LED_PORT->OUT &= ~PLAYBACK_LED_PIN; //toggles led off
        while (CheckSwitchReset() == Pressed) {}
        return;
    }

    if (currentState != lastState) {
        lastState = currentState;
        switch (currentState) {
        case START_SCREEN: //starting state, puts welcome message
            lcdClearDisplay();
            lcdSetCursor(0, 0);
            lcdPrintString(" Karaoke Machine");
            lcdSetCursor(1, 0);
            lcdPrintString("  Press \"Next\"");
            break;
        case SELECT_SCREEN: // selection state, displays current song and artist
            lcdClearDisplay();
            lcdDisplayTitleArtist(songList[currentSong]);  // Show song title and artist
            break;
        case PLAYING_SCREEN:// playing state, displays current song and artist
            lcdClearDisplay();
            lcdDisplayTitleArtist(songList[currentSong]);  // Show song title and artist
            break;
        }
    }

    switch (currentState) {
    case START_SCREEN: // changes to select if select or next is pressed
        if (CheckSwitchSelect() == Pressed || CheckSwitchNext() == Pressed) {
            currentState = SELECT_SCREEN;
            while (CheckSwitchSelect() == Pressed || CheckSwitchNext() == Pressed) {}
        }
        break;
    case SELECT_SCREEN: // changes to playing if select is pressed
        if (CheckSwitchSelect() == Pressed) {
            currentState = PLAYING_SCREEN;
            isPlaying = !isPlaying; // toggles playing
            sendPlaybackStatus(isPlaying, currentSong, isReset); // sends uart command
            if (isPlaying) {
                PLAYBACK_LED_PORT->OUT |= PLAYBACK_LED_PIN;  // LED ON when playing
            } else {
                PLAYBACK_LED_PORT->OUT &= ~PLAYBACK_LED_PIN; // LED OFF when paused
            }
            lastState = (ScreenState)(-1);
            while (CheckSwitchSelect() == Pressed) {}
        } else if (CheckSwitchNext() == Pressed) {
            currentSong = (currentSong + 1) % SONG_COUNT;  // Increase song index and wrap
            lcdClearDisplay();
            lcdDisplayTitleArtist(songList[currentSong]);  // Update display immediately
            lastState = (ScreenState)(-1);  // Force a UI update
            while (CheckSwitchNext() == Pressed) {}
        }
        break;
    case PLAYING_SCREEN: // Handles playback button presses
        if (CheckSwitchToggle() == Pressed) {
            isPlaying = !isPlaying; // toggles playing variable
            sendPlaybackStatus(isPlaying, currentSong, isReset); // sends uart command
            if (isPlaying) {
                PLAYBACK_LED_PORT->OUT |= PLAYBACK_LED_PIN;  // LED ON when playing
            } else {
                PLAYBACK_LED_PORT->OUT &= ~PLAYBACK_LED_PIN; // LED OFF when paused
            }
            lastState = (ScreenState)(-1);
            while (CheckSwitchToggle() == Pressed) {}
        }
        break;
    }
}

// Debounce function to avoid switch bouncing issues
void debounce(void)
{
    int delay;
    for (delay = 0; delay < 2500; delay++); // Initial delay
    while (!(SwitchPort->IN & (SwitchNext | SwitchSelect | SwitchToggle | SwitchReset))) {
        for (delay = 0; delay < 2500; delay++);  // Additional delay
    }
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

void InitializePlaybackLED(void) { // Initializes Playback LED
    PLAYBACK_LED_PORT->DIR |= PLAYBACK_LED_PIN;  // Set P2.3 as output
    PLAYBACK_LED_PORT->OUT &= ~PLAYBACK_LED_PIN;
}

// Initialize switches
void InitializeSwitches(void)
{
    // Set switch pins as input and enable pull-up resistors
    SwitchPort->DIR &= ~(SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);  // Set as input
    SwitchPort->REN |= (SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);   // Enable pull resistors
    SwitchPort->OUT |= (SwitchNext | SwitchSelect | SwitchToggle | SwitchReset);   // Set pull-up mode
}
