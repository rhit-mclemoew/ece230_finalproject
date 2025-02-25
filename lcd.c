//File Name: lcd_template.c
//ECE230 Winter 2024-2025
//Date: January 17, 2025
/*!
 * lcd.c
 *
 *      Description: Helper file for LCD library. For Hitachi HD44780 parallel LCD
 *               in 8-bit mode. Assumes the following connections:
 *               P6.7 <-----> RS
 *               P6.6 <-----> E
 *                            R/W --->GND
 *                P4  <-----> DB
 *
 *          This module uses SysTick timer for delays.
 *
 *      Author: ece230
 */

#include <msp.h>
#include <string.h>
#include "lcd.h"
#include "sysTickDelays.h"
#include "stdio.h"
#include "timer32.h"

#define NONHOME_MASK        0xF
#define LCD_WIDTH 16
#define LONG_INSTR_DELAY    2000
#define SHORT_INSTR_DELAY   50



typedef struct {
    int offset;         // Current scroll position
    unsigned int isScrolling;  // Whether this line needs to scroll
} LineState;

static struct {
    LineState title;
    LineState artist;
    uint32_t lastUpdateTime;
} displayState = {{0, 0}, {0, 0}, 0};


void lcdDisplayTitleArtist(const char *songInfo) {
    static char lastTitle[32] = "";
    static char lastArtist[32] = "";

    char formattedTitle[32];  // Store "1. Song Title"
    char title[32] = {0};
    char artist[32] = {0};
    char displayBuffer[LCD_WIDTH + 1];
    uint32_t currentTime;
    int splitIndex;
    int titleLen;
    int artistLen;
    int i;
    const char *artistStart;

    // Find the separator (hyphen)
    splitIndex = -1;
    for (i = 0; songInfo[i] != '\0'; i++) {
        if (songInfo[i] == '-') {
            splitIndex = i;
            break;
        }
    }

    // Extract title and artist
    if (splitIndex == -1) {
        strncpy(title, songInfo, sizeof(title) - 1);
        artist[0] = '\0';
    } else {
        strncpy(title, songInfo, splitIndex);
        title[splitIndex] = '\0';

        artistStart = songInfo + splitIndex + 1;
        while (*artistStart == ' ') artistStart++; // Skip leading spaces
        strncpy(artist, artistStart, sizeof(artist) - 1);
    }

    // Format title with song number
    snprintf(formattedTitle, sizeof(formattedTitle), "%d. %s", currentSong + 1, title);

    // Only update display if content changed
    if (strcmp(formattedTitle, lastTitle) != 0 || strcmp(artist, lastArtist) != 0) {
        lcdClearDisplay();  // Only clear if necessary
        strncpy(lastTitle, formattedTitle, sizeof(lastTitle));
        strncpy(lastArtist, artist, sizeof(lastArtist));
    }

    // Get lengths and determine if scrolling is needed
    titleLen = strlen(formattedTitle);
    artistLen = strlen(artist);

    displayState.title.isScrolling = (titleLen > LCD_WIDTH) ? 1 : 0;
    displayState.artist.isScrolling = (artistLen > LCD_WIDTH) ? 1 : 0;

    // Get current system time
    currentTime = getSystemTime();

    // Update scroll positions if enough time has passed
    if ((currentTime - displayState.lastUpdateTime) >= SCROLL_DELAY_MS) {
        displayState.lastUpdateTime = currentTime;

        if (displayState.title.isScrolling) {
            displayState.title.offset = (displayState.title.offset + 1) % (titleLen + SCROLL_PADDING);
        }

        if (displayState.artist.isScrolling) {
            displayState.artist.offset = (displayState.artist.offset + 1) % (artistLen + SCROLL_PADDING);
        }
    }

    // Display formatted title (top row)
    lcdSetCursor(0, 0);
    if (displayState.title.isScrolling) {
        scrollText(displayBuffer, formattedTitle, titleLen, displayState.title.offset);
    } else {
        centerText(displayBuffer, formattedTitle, titleLen);
    }
    lcdPrintString(displayBuffer);

    // Display artist (bottom row)
    lcdSetCursor(1, 0);
    if (displayState.artist.isScrolling) {
        scrollText(displayBuffer, artist, artistLen, displayState.artist.offset);
    } else {
        centerText(displayBuffer, artist, artistLen);
    }
    lcdPrintString(displayBuffer);
}



static void scrollText(char *dest, const char *src, int srcLen, int offset) {
    int i;
    int totalLen = srcLen + SCROLL_PADDING;  // Add padding between wrap

    for (i = 0; i < LCD_WIDTH; i++) {
        int pos = (offset + i) % totalLen;
        dest[i] = pos < srcLen ? src[pos] : ' ';
    }
    dest[LCD_WIDTH] = '\0';
}

static void centerText(char *dest, const char *src, int srcLen) {
    int padding = (LCD_WIDTH - srcLen) / 2;
    int i;

    // Fill with spaces first
    for (i = 0; i < LCD_WIDTH; i++) {
        dest[i] = ' ';
    }
    dest[LCD_WIDTH] = '\0';

    // Copy text in center position
    for (i = 0; i < srcLen && i < LCD_WIDTH; i++) {
        dest[padding + i] = src[i];
    }
}

void configLCD(uint32_t clkFreq) {
    // configure pins as GPIO
    LCD_DB_PORT->SEL0 = 0;
    LCD_DB_PORT->SEL1 = 0;
    LCD_RS_PORT->SEL0 &= ~LCD_RS_MASK;
    LCD_RS_PORT->SEL1 &= ~LCD_RS_MASK;
    LCD_EN_PORT->SEL0 &= ~LCD_EN_MASK;
    LCD_EN_PORT->SEL1 &= ~LCD_EN_MASK;
    // initialize En output to Low
    LCD_EN_PORT->OUT &= ~LCD_EN_MASK;
    // set pins as outputs
    LCD_DB_PORT->DIR = 0xFF;
    LCD_RS_PORT->DIR |= LCD_RS_MASK;
    LCD_EN_PORT->DIR |= LCD_EN_MASK;

    initDelayTimer(clkFreq);
}

/*!
 * Delay method based on instruction execution time.
 *   Execution times from Table 6 of HD44780 data sheet, with buffer.
 *
 * \param mode RS mode selection
 * \param instruction Instruction/data to write to LCD
 *
 * \return None
 */
void instructionDelay(uint8_t mode, uint8_t instruction) {
    // if instruction is Return Home or Clear Display, use long delay for
    //  instruction execution; otherwise, use short delay
    if ((mode == DATA_MODE) || (instruction & NONHOME_MASK)) {
        delayMicroSec(SHORT_INSTR_DELAY);
    }
    else {
        delayMicroSec(LONG_INSTR_DELAY);
    }
}

/*!
 * Function to write instruction/data to LCD.
 *
 * \param mode          Write mode: 0 - control, 1 - data
 * \param instruction   Instruction/data to write to LCD
 *
 * \return None
 */
void writeInstruction(uint8_t mode, uint8_t instruction) {
    // DONE set 8-bit data on LCD DB port
    LCD_DB_PORT->OUT = instruction;

    // DONE set RS for data or control instruction mode
    //      use bit-masking to avoid affecting other pins of port
    if (mode == DATA_MODE) {
        LCD_RS_PORT->OUT |= LCD_RS_MASK; // Set RS high for data mode
    } else {
        LCD_RS_PORT->OUT &= ~LCD_RS_MASK; // Set RS low for control mode
    }
    // pulse E to execute instruction on LCD
    // DONE set Enable signal high
    //      use bit-masking to avoid affecting other pins of port

    LCD_EN_PORT->OUT |= LCD_EN_MASK;  // Set Enable signal high
    delayMicroSec(1);
    // DONE set Enable signal low
    //      use bit-masking to avoid affecting other pins of port
    LCD_EN_PORT->OUT &= ~LCD_EN_MASK; // Set Enable signal low
    // delay to allow instruction execution to complete
    instructionDelay(mode, instruction);
}

/*!
 * Function to write command instruction to LCD.
 *
 * \param command Command instruction to write to LCD
 *
 * \return None
 */
void commandInstruction(uint8_t command) {
    writeInstruction(CTRL_MODE, command);
}

/*!
 * Function to write data instruction to LCD. Writes ASCII value to current
 *  cursor location.
 *
 * \param data ASCII value/data to write to LCD
 *
 * \return None
 */
void dataInstruction(uint8_t data) {
    writeInstruction(DATA_MODE, data);
}

void initLCD(void) {
    // follows initialization sequence described for 8-bit data mode in
    //  Figure 23 of HD447780 data sheet
    delayMilliSec(40);
    commandInstruction(FUNCTION_SET_MASK | DL_FLAG_MASK);
    delayMilliSec(5);
    commandInstruction(FUNCTION_SET_MASK | DL_FLAG_MASK);
    delayMicroSec(150);
    commandInstruction(FUNCTION_SET_MASK | DL_FLAG_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);
    commandInstruction(FUNCTION_SET_MASK | DL_FLAG_MASK | N_FLAG_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);
    commandInstruction(DISPLAY_CTRL_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);
    commandInstruction(CLEAR_DISPLAY_MASK);
    delayMicroSec(SHORT_INSTR_DELAY);
    commandInstruction(ENTRY_MODE_MASK | ID_FLAG_MASK);
    delayMicroSec(LONG_INSTR_DELAY);

    // after initialization and configuration, turn display ON
    commandInstruction(DISPLAY_CTRL_MASK | D_FLAG_MASK);
    lcdClearDisplay();
    lcdSetCursor(0, 0);
}

void printChar(char character) {
    // print ASCII \b character to current cursor position
    dataInstruction(character);
}

void lcdPrintString(const char *string) { // Prints string to lcd
    int pos = 0;
    int row = 0;

    while (*string) {
        if (pos == LCD_WIDTH) {
            row++;
            if (row > 1) break;

            lcdSetCursor(1, 0);
            pos = 0;
        }

        dataInstruction(*string);
        string++;
        pos++;
    }
}

void lcdSetCursor(uint8_t row, uint8_t col) { // Sets cursor at select row and col
    uint8_t address;
    if (row == 0) {
        address = 0x80 + col;  // Line 1 starts at 0x80
    } else {
        address = 0xC0 + col;  // Line 2 starts at 0xC0
    }
    commandInstruction(address);
}


void lcdClearDisplay() {
    // clear the LCD display and return cursor to home position
    commandInstruction(CLEAR_DISPLAY_MASK);
    delayMicroSec(LONG_INSTR_DELAY);
}
