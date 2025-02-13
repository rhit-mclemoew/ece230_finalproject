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
#define Switch1         0b00000100      // P3.2
#define Switch2         0b00001000      // P3.3
#define Switch3         0b00100000      // P3.5
#define Switch4         0b01000000      // P3.6

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

// Function prototypes
void InitializeRGBLEDs(void);
void InitializeSwitches(void);
SwitchState CheckSwitch1(void);
SwitchState CheckSwitch2(void);
SwitchState CheckSwitch3(void);
SwitchState CheckSwitch4(void);
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
        // sends initialization sequence and configuration to LCD
    initLCD();

//    printChar('Y');
//    printChar('A');
//    printChar('Y');
//    printChar(' ');
//    printChar('E');
//    printChar('C');
//    printChar('E');
//    printChar('2');
//    printChar('3');
//    printChar('0');
//    printChar('!');

      printString(" Karoke Machine");
      setCursor(1, 0); // Move to row 2, column 0
      printString("  Press 'Next'");


    while (1) {
        // Check switches and control LEDs accordingly
        if (CheckSwitch1() == Pressed) {

                clearDisplay();
                setCursor(0, 0);
                printString("1.    Happy");

            CurrentLED = RED;
        } else if (CheckSwitch2() == Pressed) {
            CurrentLED = GREEN;
        } else if (CheckSwitch3() == Pressed) {
            CurrentLED = BLUE;
        } else if (CheckSwitch4() == Pressed) {
            CurrentLED = PURPLE;
        } else {
            CurrentLED = NONE;
        }

        // Set LED color based on switch state
        SetLED(CurrentLED);
    }

    return 0;
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
SwitchState CheckSwitch1(void)
{
    if (!(SwitchPort->IN & Switch1)) {
        debounce();
        return (!(SwitchPort->IN & Switch1)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 2 is pressed
SwitchState CheckSwitch2(void)
{
    if (!(SwitchPort->IN & Switch2)) {
        debounce();
        return (!(SwitchPort->IN & Switch2)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 3 is pressed
SwitchState CheckSwitch3(void)
{
    if (!(SwitchPort->IN & Switch3)) {
        debounce();
        return (!(SwitchPort->IN & Switch3)) ? Pressed : NotPressed;
    }
    return NotPressed;
}

// Check if Switch 4 is pressed
SwitchState CheckSwitch4(void)
{
    if (!(SwitchPort->IN & Switch4)) {
        debounce();
        return (!(SwitchPort->IN & Switch4)) ? Pressed : NotPressed;
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
    SwitchPort->DIR &= ~(Switch1 | Switch2 | Switch3 | Switch4);  // Set as input
    SwitchPort->REN |= (Switch1 | Switch2 | Switch3 | Switch4);   // Enable pull resistors
    SwitchPort->OUT |= (Switch1 | Switch2 | Switch3 | Switch4);   // Set pull-up mode
}
