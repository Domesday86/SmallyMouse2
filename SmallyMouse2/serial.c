/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2017-2026 Simon Inns <simon.inns@gmail.com>
 *
 * serial.c - Interrupt driven serial debug console
 * SmallyMouse2 - USB to quadrature mouse converter
 */

// System includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>

// LUFA includes
#include <LUFA/Drivers/Peripheral/Serial.h>

#include "serial.h"

#if (SERIAL_TX_BUFFER_SIZE < 2) || (SERIAL_TX_BUFFER_SIZE > 256) || \
	((SERIAL_TX_BUFFER_SIZE & (SERIAL_TX_BUFFER_SIZE - 1)) != 0)
#error "SERIAL_TX_BUFFER_SIZE must be a power of 2 between 2 and 256"
#endif

#define SERIAL_TX_BUFFER_MASK (SERIAL_TX_BUFFER_SIZE - 1)

// Transmit ring buffer
//
// Characters written to stdout are placed in the buffer at txHead (by the main code) and
// sent from txTail by the USART1 data register empty ISR.  The buffer is empty when
// txHead == txTail (so it holds at most SERIAL_TX_BUFFER_SIZE - 1 characters).  The
// indexes are 8-bit, so they are read and written atomically.
static volatile uint8_t txBuffer[SERIAL_TX_BUFFER_SIZE];
static volatile uint8_t txHead = 0;
static volatile uint8_t txTail = 0;

static int serialPutChar(char c, FILE *stream);
static FILE serialStream = FDEV_SETUP_STREAM(serialPutChar, NULL, _FDEV_SETUP_WRITE);

// Interrupt Service Routine for USART1 data register empty (send the next character)
ISR(USART1_UDRE_vect)
{
	// The buffer can be empty here if serialPutChar() re-enabled the interrupt after
	// the last character was sent (its read-modify-write of UCSR1B was interrupted)
	if (txTail == txHead) {
		UCSR1B &= ~(1 << UDRIE1);
		return;
	}

	UDR1 = txBuffer[txTail];
	txTail = (txTail + 1) & SERIAL_TX_BUFFER_MASK;

	// Stop the interrupt when there is nothing left to send
	if (txTail == txHead) UCSR1B &= ~(1 << UDRIE1);
}

// Initialise the serial UART (8N1) and direct stdout to it
void serialInit(uint32_t baudRate)
{
	Serial_Init(baudRate, false);
	stdout = &serialStream;
}

// Send any buffered characters by polling the USART
//
// This is used when interrupts are disabled (i.e. during initialisation or from inside
// an ISR) as the data register empty ISR cannot run to empty the buffer
void serialFlush(void)
{
	while (txTail != txHead) {
		while (!(UCSR1A & (1 << UDRE1)));
		UDR1 = txBuffer[txTail];
		txTail = (txTail + 1) & SERIAL_TX_BUFFER_MASK;
	}
}

// Write a character to the serial debug console (stdout stream)
static int serialPutChar(char c, FILE *stream)
{
	uint8_t nextHead = (txHead + 1) & SERIAL_TX_BUFFER_MASK;

	// If interrupts are disabled, send the buffered characters and this one directly
	// (the characters are sent in order and nothing waits on the ISR)
	if (!(SREG & (1 << SREG_I))) {
		serialFlush();
		while (!(UCSR1A & (1 << UDRE1)));
		UDR1 = c;
		return 0;
	}

	// Wait for space in the buffer (only if more than a buffer's worth is written at once)
	while (nextHead == txTail);

	txBuffer[txHead] = c;
	txHead = nextHead;

	// Start (or keep running) the data register empty ISR
	UCSR1B |= (1 << UDRIE1);

	return 0;
}
