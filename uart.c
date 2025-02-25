#include "msp.h"
#include "uart.h"
#include "stdio.h"

void initUART(void) {
    CS->KEY = CS_KEY_VAL;         // Unlock CS registers
    CS->CTL0 = CS_CTL0_DCORSEL_3; // Set DCO to 12MHz
    CS->CTL1 = CS_CTL1_SELA_2 |   // Set ACLK = REFOCLK
               CS_CTL1_SELS_3 |   // Set SMCLK = DCO (12MHz)
               CS_CTL1_SELM_3;    // Set MCLK = DCO (12MHz)
    CS->KEY = 0;                  // Lock CS registers
    // Put eUSCI_A0 in reset
    EUSCI_A0->CTLW0 = EUSCI_A_CTLW0_SWRST;

    EUSCI_A0->CTLW0 |= EUSCI_A_CTLW0_SSEL__SMCLK;
    EUSCI_A0->BRW = 6;
    EUSCI_A0->MCTLW = (8 << EUSCI_A_MCTLW_BRF_OFS) | (0x20 << EUSCI_A_MCTLW_BRS_OFS) | EUSCI_A_MCTLW_OS16;

    // Configure TX (P1.3) and RX (P1.2)
    P1->SEL0 |= BIT2 | BIT3;
    P1->SEL1 &= ~(BIT2 | BIT3);

    // Enable UART Interrupts (Better than polling)
    EUSCI_A0->IE |= EUSCI_A_IE_RXIE; // Enable RX interrupt

    // Release from reset
    EUSCI_A0->CTLW0 &= ~EUSCI_A_CTLW0_SWRST;
}

void sendString(const char *str) {
    while (*str) {
        sendByte(*str++);  // Send each character one by one
    }
}


void sendPlaybackStatus(uint8_t isPlaying, uint8_t songIndex, uint8_t isReset) {
    char buffer[25];
    sprintf(buffer, "P:%u S:%u R:%u\n", isPlaying, songIndex, isReset);  // Using %u for unsigned integers
    sendString(buffer);
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
