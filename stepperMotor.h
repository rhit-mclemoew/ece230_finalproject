/*! \file */
/*!
 * stepperMotor.h
 *
 * Description: Stepper motor ULN2003 driver for MSP432P4111 Launchpad.
 *              Assumes SMCLK configured with 48MHz HFXT as source.
 *              Uses Timer_A3 and P2.7, P2.6, P2.5, P2.4
 *
 *  Created on:
 *      Author:
 */

#ifndef STEPPERMOTOR_H_
#define STEPPERMOTOR_H_

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#include <msp.h>

#define SMCLK 48000000 //Hz
//Stepper definition
//stepper_period_count =60xFtimer/(RPM*32*64)
#define TimerA3Prescaler 48 //Timer A prescaler
#define TimerA3Clock  SMCLK/TimerA3Prescaler
#define RPM13 13
#define STEPPER_13RPM 60*TimerA3Clock/(32*64*RPM13)  //Timer count value

#define STEPPER_PORT                    P2
#define STEPPER_MASK                    (0x00F0)
#define STEPPER_IN1                     BIT7
#define STEPPER_IN2                     BIT6
#define STEPPER_IN3                     BIT5
#define STEPPER_IN4                     BIT4

// DONE set initial step timer period for 1 RPM (based on 1MHz clock rate)
#define INIT_PERIOD               29296
#define STEP_SEQ_CNT              4
//stepper status to be changed by S2 in advanced implementation
enum {CLOCKWISE, COUNTER_CLOCKWISE} StepperStatus;



/*!
 * \brief This function configures pins and timer for stepper motor driver
 *
 * This function configures P2.4, P2.5, P2.6, and P2.6 as output pins
 *  for the ULN2003 stepper driver IN port, and initializes Timer_A3 to
 *  increment step position with compare interrupt
 *
 * Modified bits 4 to 7 of \b P2DIR register and \b P2SEL registers.
 * Modified \b TA3CTL register and \b TA3CCTL0 registers.
 *
 * \return None
 */
extern void ConfigureStepper(void);


/*!
 * \brief This starts stepper motor rotation by turning on Timer_A3
 *
 * This function starts stepper motor rotation by turning on Timer_A3.
 * Assumes stepper motor has already been configured by initStepperMotor().
 *
 * Modified \b TA3CTL register.
 *
 * \return None
 */
void enableStepperMotor(void);


/*!
 * \brief This stops stepper motor rotation by turning off timer
 *
 * This function stops stepper motor rotation by turning off Timer_A3.
 * Stepper motor is still configured after calling this function.
 *
 * Modified \b TA3CTL register.
 *
 * \return None
 */
void disableStepperMotor(void);

/*!
 * \brief This increments step clockwise
 *
 * This function increments to next clockwise step position
 *
 * Modified bit 4, 5, 6, and 7 of \b P2OUT register.
 *
 * \return None
 */
extern void stepClockwise(void);

/*!
 * \brief This increments step counter-clockwise
 *
 * This function increments to next counter-clockwise step position
 *
 * Modified bit 4, 5, 6, and 7 of \b P2OUT register.
 *
 * \return None
 */
extern void stepCounterClockwise(void);


/*!
 * \brief This increments servo angle 10 degrees, with wrap-around
 *
 * This function increments servo angle by 10 degrees. If new angle exceeds max
 *  angle (+90), it wraps around to min angle (-90)
 *
 * Modified \b TA2CCR1 register.
 *
 * \return None
 */
extern void incrementTenDegree(void);


/*!
 * \brief This function sets angle of servo
 *
 * This function sets angle of servo to \a angle (between -90 to 90)
 *
 *  \param angle Angle in degrees to set servo (between -90 to 90)
 *
 * Modified \b TA2CCR1 register.
 *
 * \return None
 */

void ResetStepperSpeed(float rpm, unsigned int StepperTimerClock);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif /* STEPPERMOTOR_H_ */
