#include "msp.h"
#include "uart.h"

void initUART(void) {
    // Put eUSCI_A0 in reset
    EUSCI_A0->CTLW0 = EUSCI_A_CTLW0_SWRST;

    EUSCI_A0->CTLW0 |= EUSCI_A_CTLW0_SSEL__SMCLK;
    EUSCI_A0->BRW = 5;
    EUSCI_A0->MCTLW = 0; // disable oversampling

    // Configure TX (P1.3) and RX (P1.2)
    P1->SEL0 |= BIT2 | BIT3;
    P1->SEL1 &= ~(BIT2 | BIT3);

    // Enable UART Interrupts (Better than polling)
    EUSCI_A0->IE |= EUSCI_A_IE_RXIE; // Enable RX interrupt

    // Release from reset
    EUSCI_A0->CTLW0 &= ~EUSCI_A_CTLW0_SWRST;
}

// Send a single byte
void sendByte(uint8_t data) {
    while (!(EUSCI_A0->IFG & EUSCI_A_IFG_TXIFG));  // Wait for TX buffer
    EUSCI_A0->TXBUF = data;  // Send byte
}

// Read a single byte (Blocking)
uint8_t readByte(void) {
    while (!(EUSCI_A0->IFG & EUSCI_A_IFG_RXIFG));  // Wait for RX buffer
    return EUSCI_A0->RXBUF;  // Read received byte
}

void uartEcho(void) {
    uint8_t receivedByte;
    while (1) {
        receivedByte = readByte(); // Read incoming byte
        sendByte(receivedByte); // Echo back

    }
}
