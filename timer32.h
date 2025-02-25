#ifndef TIMER32_H
#define TIMER32_H

#include <stdint.h>
#include "msp.h"

extern uint8_t currentSong;
extern const char *songList[];
extern uint8_t currentSong;

typedef enum MenuState {
    START_SCREEN, SELECT_SCREEN, PLAYING_SCREEN
} ScreenState;

extern ScreenState currentState;


void Timer32_Init(void);
uint32_t getSystemTime(void);

#endif // TIMER32_H
