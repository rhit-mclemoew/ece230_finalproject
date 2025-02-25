/*! \file */
/*!
 * stepperMotorTemplate.c
 * ECE230 Winter 2024-2025
 *
 * Description: Stepper motor ULN2003 driver for MSP432P4111 Launchpad.
 *              Assumes SMCLK configured with 48MHz HFXT as source.
 *              Uses Timer_A3 and P2.7, P2.6, P2.5, P2.4
 *
 *  Created on:
 *      Author:
 */

#include "stepperMotor.h"
#include "msp.h"

/* Global Variables  */
const uint8_t stepperSequence[STEP_SEQ_CNT] =  { 0b1000, 0b0100, 0b0010, 0b0001 };
uint16_t stepPeriod = 2500;
uint8_t currentStep = 0;


void initStepperMotor(void) {
    // set stepper port pins as GPIO outputs
    STEPPER_PORT->SEL0 = (STEPPER_PORT->SEL0) & ~STEPPER_MASK;
    STEPPER_PORT->SEL1 = (STEPPER_PORT->SEL1) & ~STEPPER_MASK;
    STEPPER_PORT->DIR = (STEPPER_PORT->DIR) | STEPPER_MASK;

    // initialize stepper outputs to LOW
    STEPPER_PORT->OUT = (STEPPER_PORT->OUT) & ~STEPPER_MASK;

    /* Configure Timer_A3 and CCR0 */
    // Set period of Timer_A3 in CCR0 register for Up Mode
    TIMER_A3->CCR[0] = stepPeriod;

    TIMER_A3->CCTL[0] = TIMER_A_CCTLN_CCIE;
    // Configure Timer_A3 in Stop Mode, with source SMCLK, prescale 12:1, and
    //  interrupt disabled  -  tick rate will be 4MHz (for SMCLK = 48MHz)

    TIMER_A3->CTL = 0x2C0;
        //0x2C0 0x02D4 0x280
        TIMER_A3->EX0 = 0b101;

    /* Configure global interrupts and NVIC */
    // Enable TA3CCR0 compare interrupt by setting IRQ bit in NVIC ISER0 register

    NVIC->ISER[0] = 1 << (TA3_0_IRQn);  // Enable Timer_A3 CCR0 interrupt
    //TIMER_A3->CCTL[0] &= ~TIMER_A_CCTLN_CCIFG;  // Clear interrupt flag


    __enable_irq();                             // Enable global interrupt
}

void enableStepperMotor(void) {

    TIMER_A3->CTL |= TIMER_A_CTL_MC__UP;  // Set Timer_A3 to Up Mode

}

void disableStepperMotor(void) {
    //  Configure Timer_A3 in Stop Mode (leaving remaining configuration unchanged)
    TIMER_A3->CTL = (TIMER_A3->CTL & ~TIMER_A_CTL_MC_MASK) | TIMER_A_CTL_MC__STOP;

}

void stepClockwise(void) {
   // currentStep = (currentStep + 1) % STEP_SEQ_CNT;  // increment to next step position
    currentStep = ((uint8_t)(currentStep + 1)) % STEP_SEQ_CNT;  // decrement to previous step position (counter-clockwise)
    //  do this as a single assignment to avoid transient changes on driver signals

    STEPPER_PORT->OUT = (STEPPER_PORT->OUT & 0x0F) + (stepperSequence[currentStep] << 4);

}

void stepCounterClockwise(void) {
    currentStep = ((uint8_t)(currentStep - 1)) % STEP_SEQ_CNT;  // decrement to previous step position (counter-clockwise)
    //  update output port for current step pattern
    //  do this as a single assignment to avoid transient changes on driver signals
    //STEPPER_PORT->OUT |= (stepperSequence[currentStep] << 4);
    STEPPER_PORT->OUT = (STEPPER_PORT->OUT & 0x0F) + (stepperSequence[currentStep] << 4);
}

// Timer A3 CCR0 interrupt service routine
void TA3_0_IRQHandler(void)
{
    /* Not necessary to check which flag is set because only one IRQ
     *  mapped to this interrupt vector     */
    stepClockwise();

    TIMER_A3->CCTL[0] &= ~TIMER_A_CCTLN_CCIFG;      // Clear CCR0 interrupt flag


}
