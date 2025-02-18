#ifndef UART_H_
#define UART_H_

#include <stdint.h>

/**
 * @brief Initializes UART (e.g., eUSCI_A0) at 115200 baud (for 48MHz MCLK).
 */
void initUART(void);

/**
 * @brief Sends one byte over UART.
 *
 * @param data The byte to send.
 */
void sendByte(uint8_t data);

/**
 * @brief Reads one byte from the UART RX buffer (blocking).
 *
 * @return The received byte.
 */
uint8_t readByte(void);

/**
 * @brief UART Echo Function (Debugging).
 */
void uartEcho(void);

#endif /* UART_H_ */
