/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2017-2026 Simon Inns <simon.inns@gmail.com>
 *
 * serial.h - Interrupt driven serial debug console
 * SmallyMouse2 - USB to quadrature mouse converter
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

// Transmit buffer size in bytes (must be a power of 2, no more than 256)
#define SERIAL_TX_BUFFER_SIZE 128

// Function prototypes
void serialInit(uint32_t baudRate);
void serialFlush(void);

#endif
