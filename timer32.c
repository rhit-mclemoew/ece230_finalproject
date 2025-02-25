#include "lcd.h"
#include "timer32.h"


// Definitions
volatile uint32_t millis = 0;
extern volatile uint8_t updateLCD;

void Timer32_Init(void) {
    TIMER32_1->LOAD = (SystemCoreClock / 1000) - 1;
    TIMER32_1->CONTROL = TIMER32_CONTROL_SIZE | TIMER32_CONTROL_MODE | TIMER32_CONTROL_IE | TIMER32_CONTROL_ENABLE;

    NVIC_SetPriority(T32_INT1_IRQn, 2);  // Lower priority so button presses get priority
    NVIC_EnableIRQ(T32_INT1_IRQn);
}


// Timer32 Interrupt Handler (called every 1ms)
void T32_INT1_IRQHandler(void) {
    millis++;

    if ((currentState == SELECT_SCREEN || currentState == PLAYING_SCREEN) &&
        (millis % SCROLL_DELAY_MS == 0)) {
        updateLCD = 1;
    }

    TIMER32_1->INTCLR = 0;  // Clear interrupt flag
}

uint32_t getSystemTime(void) {
    return millis;
}
