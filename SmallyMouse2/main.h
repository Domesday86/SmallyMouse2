/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2017-2026 Simon Inns <simon.inns@gmail.com>
 *
 * main.h - Main functions
 * SmallyMouse2 - USB to quadrature mouse converter
 */

#ifndef _MAIN_H_
#define _MAIN_H_

// Hardware map

// Quadrature mouse output port
// Right mouse button
#define RB_PORT	PORTC
#define RB_PIN	PINC
#define RB_DDR	DDRC
#define RB		(1 << 6)

// Middle mouse button
#define MB_PORT	PORTC
#define MB_PIN	PINC
#define MB_DDR	DDRC
#define MB		(1 << 5)

// Left mouse button
#define LB_PORT	PORTC
#define LB_PIN	PINC
#define LB_DDR	DDRC
#define LB		(1 << 4)

// Y axis output
#define Y2_PORT	PORTC
#define Y2_PIN	PINC
#define Y2_DDR	DDRC
#define Y2		(1 << 3)

#define Y1_PORT	PORTC
#define Y1_PIN	PINC
#define Y1_DDR	DDRC
#define Y1		(1 << 1)

// X axis output
#define X2_PORT	PORTC
#define X2_PIN	PINC
#define X2_DDR	DDRC
#define X2		(1 << 2)

#define X1_PORT	PORTC
#define X1_PIN	PINC
#define X1_DDR	DDRC
#define X1		(1 << 0)

// Rate limit on/off switch (marked 'slow')
#define RATESW_PORT	PORTA
#define RATESW_PIN	PINA
#define RATESW_DDR	DDRA
#define RATESW		(1 << 4)

// Expansion (Ian) header
#define E0_PORT	PORTD
#define E0_PIN	PIND
#define E0_DDR	DDRD
#define E0		(1 << 0)

#define E1_PORT	PORTD
#define E1_PIN	PIND
#define E1_DDR	DDRD
#define E1		(1 << 1)

#define E2_PORT	PORTD
#define E2_PIN	PIND
#define E2_DDR	DDRD
#define E2		(1 << 2)

#define E3_PORT	PORTD
#define E3_PIN	PIND
#define E3_DDR	DDRD
#define E3		(1 << 3)

#define E4_PORT	PORTD
#define E4_PIN	PIND
#define E4_DDR	DDRD
#define E4		(1 << 4)

#define E5_PORT	PORTD
#define E5_PIN	PIND
#define E5_DDR	DDRD
#define E5		(1 << 5)

#define E6_PORT	PORTD
#define E6_PIN	PIND
#define E6_DDR	DDRD
#define E6		(1 << 6)


// E7 is now used as the DPISW header
#define DPISW_PORT	PORTD
#define DPISW_PIN	PIND
#define DPISW_DDR	DDRD
#define DPISW		(1 << 7)

// Function prototypes
void initialiseHardware(void);
void initialiseTimers(void);
void processMouse(void);
uint8_t mapButtons(uint8_t buttons);
void setButtonMapping(uint16_t vendorId, uint16_t productId);
void reportButtons(uint8_t buttons);
void startQuadratureTimers(void);
uint16_t processMouseMovement(int8_t movementUnits, uint8_t axis, bool limitRate, bool dpiDivide);

// USB callback event handlers (LUFA)
void EVENT_USB_Host_HostError(const uint8_t ErrorCode);
void EVENT_USB_Host_DeviceAttached(void);
void EVENT_USB_Host_DeviceUnattached(void);
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t ErrorCode, const uint8_t SubErrorCode);
void EVENT_USB_Host_DeviceEnumerationComplete(void);

void ReadNextReport(void);

#endif

