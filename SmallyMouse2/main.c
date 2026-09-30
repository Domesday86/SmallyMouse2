/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2017-2026 Simon Inns <simon.inns@gmail.com>
 *
 * main.c - Main functions
 * SmallyMouse2 - USB to quadrature mouse converter
 */

// Important notice:  If you port this firmware to another hardware design
// it *will be* a derivative of the original and therefore you must pay attention
// to the CC hardware licensing and release your hardware design as per the share-
// alike license.  Keep it open! ...and yes, that includes you Commodore chaps.

// System includes
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/delay.h>
#include <util/atomic.h>

// Note: SmallyMouse2 makes extensive use of the LUFA libraries:
//
// LUFA Library
// Copyright (C) Dean Camera, 2015.
// dean [at] fourwalledcubicle [dot] com
// www.lufa-lib.org

// Include LUFA libraries
#include <LUFA/Drivers/Misc/TerminalCodes.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Platform/Platform.h>

#include "ConfigDescriptor.h"
#include "main.h"

#define MOUSEX	0
#define MOUSEY	1

// Configuration ------------------------------------------------------------------------------------------------------

// Quadrature output frequency limit
//
// This setting limits the maximum frequency of the quadrature output towards the retro computer.
// If the rate of output is too high the retro computer cannot 'count' the input accurately and
// this causes spurious mouse movement.
//
// All quadrature frequencies are given in Hz of full quadrature cycles.  Each cycle is 4 edges
// (phase changes) and each USB movement unit is output as one edge.
//
// Q_MAXRATE is the maximum output frequency with no rate limit (this also limits how often the
// output ISRs are called, so it should not be raised much).
//
// For 8-bit machines it is recommended that the speed doesn't exceed 1000 Hz.  Q_RATELIMIT is
// only applied if the 'slow' configuration jumper is shorted (i.e. on)
//
// On the Acorn 8-bit user port one quadrature line per axis drives a 6522 VIA
// interrupt input (CB1 for X, CB2 for Y) and the other line is read from port B by the
// interrupt handler to get the direction.  The VIA detects the edge in under 1 uS (so the
// VIA itself is not the limit) but the direction line changes one edge after the interrupt
// edge, so the 6502 must reach the handler and read port B within one edge period
// (1 / (4 * Q_RATELIMIT)).  If it doesn't, the direction is misread and the movement stalls
// or reverses.  1000 Hz gives 250 uS, several times the typical MOS interrupt latency.
//
// The VIA interrupts once per quadrature cycle for each axis, so the rate also sets the load
// on the retro computer's CPU when the mouse moves quickly (2 * Q_RATELIMIT interrupts per
// second with both axes moving).  Higher rates leave less time for the running program.
//
// The output is never faster than the configured frequency (the timer period is rounded up)
#define Q_MAXRATE 3906
#ifndef Q_RATELIMIT
#define Q_RATELIMIT 1000
#endif

// Quadrature output drain time
//
// Buffered movement is output over this period (in uS).  10,000 uS matches the report
// interval of a USB mouse reporting at 100 Hz.  If the reports arrive faster than this
// the buffer grows until the output rate matches the input rate (i.e. the output lags
// the USB mouse by about this time).
#define Q_DRAINTIME 10000

// Quadrature output buffer limit
//
// Since the slow rate limit will prevent the quadrature output keeping up with the USB movement
// report input, the quadrature output will lag behind (i.e. the quadrature mouse will continue
// to move after the USB mouse has stopped).  This setting limits the maximum number of buffered
// movements to the quadrature output.  If the buffer reaches this value further USB movements
// will be discarded
//
// When the rate limit is on the buffer is limited to the movement that can be output at
// Q_RATELIMIT in Q_DRAINTIME instead (see Q_RATELIMIT_BUFFERLIMIT), so the quadrature mouse
// lags the USB mouse by no more than Q_DRAINTIME.  Movement faster than the rate limit is
// discarded rather than output late.
#define Q_BUFFERLIMIT 300

// DPI Divider
//
// Some USB mice have very high DPI which causes the quadrature rate to be too high (making the
// mouse move too fast).  If the DPISW header is shorted the following constant will be used to 
// divide the DPI rate to slow things down.  2 or 3 are reasonable values.
#define DPI_DIVIDER 2

// Quadrature output timer ---------------------------------------------------------------------------------------------

// Timer1 (X) and Timer3 (Y) are 16-bit timers clocked at F_CPU / 8 (0.5 uS per tick at 16 MHz).
// Q_TIMER_PRESCALE must match the CSn1 prescale setting in initialiseTimers()
#define Q_TIMER_PRESCALE 8UL
#define Q_TICKS_PER_SECOND (F_CPU / Q_TIMER_PRESCALE)

// Convert a quadrature frequency (in Hz of full cycles) into timer ticks per edge (rounded up)
#define Q_HZ_TO_TICKS(hz) ((Q_TICKS_PER_SECOND + (4UL * (hz)) - 1) / (4UL * (hz)))

// Timer ticks per edge at the maximum and rate limited output frequencies
#define Q_MAXRATE_TICKS Q_HZ_TO_TICKS(Q_MAXRATE)
#define Q_RATELIMIT_TICKS Q_HZ_TO_TICKS(Q_RATELIMIT)

// Timer ticks to drain the buffer over (split to avoid overflowing 32-bit arithmetic)
#define Q_DRAINTIME_TICKS (((Q_TICKS_PER_SECOND / 1000UL) * Q_DRAINTIME) / 1000UL)

// Range check the configuration at compile time.  Timer periods must fit into the 16-bit
// timer (TOP = ticks - 1 must be 0-65535), the mouse distance (int16_t) must not overflow
// when a report is added before the buffer limit is applied, and the DPI remainder (int8_t)
// must hold +/-(DPI_DIVIDER - 1)
#if (F_CPU % (Q_TIMER_PRESCALE * 1000UL)) != 0
#error "F_CPU / Q_TIMER_PRESCALE must be a whole number of ticks per mS"
#endif
#if (Q_MAXRATE < 1) || (Q_MAXRATE_TICKS > 65536)
#error "Q_MAXRATE is out of range"
#endif
#if (Q_RATELIMIT < 1) || (Q_RATELIMIT_TICKS > 65536)
#error "Q_RATELIMIT is out of range"
#endif
#if (Q_DRAINTIME_TICKS < 1) || (Q_DRAINTIME_TICKS > 65535)
#error "Q_DRAINTIME is out of range"
#endif
#if (Q_BUFFERLIMIT < 1) || (Q_BUFFERLIMIT > (32767 - 128))
#error "Q_BUFFERLIMIT is out of range"
#endif
#if (DPI_DIVIDER < 1) || (DPI_DIVIDER > 128)
#error "DPI_DIVIDER is out of range"
#endif

// Quadrature output buffer limit when the rate limit is on: the number of edges output at
// Q_RATELIMIT in Q_DRAINTIME (40 at 1000 Hz), at least 1 and no more than Q_BUFFERLIMIT
#define Q_RATELIMIT_DRAINUNITS (Q_DRAINTIME_TICKS / Q_RATELIMIT_TICKS)
#if Q_RATELIMIT_DRAINUNITS < 1
#define Q_RATELIMIT_BUFFERLIMIT 1
#elif Q_RATELIMIT_DRAINUNITS > Q_BUFFERLIMIT
#define Q_RATELIMIT_BUFFERLIMIT Q_BUFFERLIMIT
#else
#define Q_RATELIMIT_BUFFERLIMIT Q_RATELIMIT_DRAINUNITS
#endif

// Interrupt Service Routines for quadrature output -------------------------------------------------------------------

// The following globals are used by the interrupt service routines to track the mouse
// movement.  The mouseDirection indicates which direction the mouse is moving in and
// the mouseEncoderPhase tracks the current phase of the quadrature output.
//
// The mouseDistance variable tracks the current distance the mouse has left to move
// (this is incremented by the USB mouse reports and decremented by the ISRs as they
// output the quadrature to the retro host).
//
// The mouseDistance variables are 16-bit and shared with the ISRs, so any access from
// outside the ISRs must be made inside an ATOMIC_BLOCK.
//
// The output pins are derived from the phase as a fixed Gray code (for X; Y has the
// pins swapped):
//
//   Phase:  0  1  2  3
//   X1:     1  1  0  0
//   X2:     0  1  1  0
//
// The phase starts at 3 to match the initial pin state of X1 = 0, X2 = 0.
//
// Each ISR disables its own interrupt when there is no distance left to move (so the
// ISRs do not run while the mouse is idle).  processMouse() restarts it when there is
// new movement to output (see startQuadratureTimers()).
volatile int8_t mouseDirectionX = 0;		// X direction (0 = decrement, 1 = increment)
volatile int8_t mouseEncoderPhaseX = 3;		// X Quadrature phase (0-3)

volatile int8_t mouseDirectionY = 0;		// Y direction (0 = decrement, 1 = increment)
volatile int8_t mouseEncoderPhaseY = 3;		// Y Quadrature phase (0-3)

volatile int16_t mouseDistanceX = 0;		// Distance left for mouse to move
volatile int16_t mouseDistanceY = 0;		// Distance left for mouse to move

volatile uint16_t xTimerTop = Q_MAXRATE_TICKS - 1;	// X axis timer TOP value (16-bit, use an ATOMIC_BLOCK outside the ISRs)
volatile uint16_t yTimerTop = Q_MAXRATE_TICKS - 1;	// Y axis timer TOP value (16-bit, use an ATOMIC_BLOCK outside the ISRs)

// Interrupt Service Routine based on Timer1 for mouse X movement quadrature output
ISR(TIMER1_COMPA_vect)
{
	// Process X output
	if (mouseDistanceX > 0) {
		// Change phase and range check
		if (mouseDirectionX == 0) {
			mouseEncoderPhaseX--;
			if (mouseEncoderPhaseX < 0) mouseEncoderPhaseX = 3;
			} else {
			mouseEncoderPhaseX++;
			if (mouseEncoderPhaseX > 3) mouseEncoderPhaseX = 0;
		}
		
		// Set both output pins according to the current phase
		if (mouseEncoderPhaseX == 0 || mouseEncoderPhaseX == 1) X1_PORT |= X1;	// Set X1 to 1
		else X1_PORT &= ~X1;	// Set X1 to 0
		if (mouseEncoderPhaseX == 1 || mouseEncoderPhaseX == 2) X2_PORT |= X2;	// Set X2 to 1
		else X2_PORT &= ~X2;	// Set X2 to 0
		
		// Decrement the distance left to move
		mouseDistanceX--;
	}
	
	// Set the timer top value for the next interrupt, or stop the interrupt if there is
	// nothing left to output
	if (mouseDistanceX > 0) OCR1A = xTimerTop;
	else TIMSK1 &= ~(1 << OCIE1A);
}

// Interrupt Service Routine based on Timer3 for mouse Y movement quadrature output
ISR(TIMER3_COMPA_vect)
{
	// Process Y output
	if (mouseDistanceY > 0) {
		// Change phase and range check
		if (mouseDirectionY == 0) {
			mouseEncoderPhaseY--;
			if (mouseEncoderPhaseY < 0) mouseEncoderPhaseY = 3;
			} else {
			mouseEncoderPhaseY++;
			if (mouseEncoderPhaseY > 3) mouseEncoderPhaseY = 0;
		}
				
		// Set both output pins according to the current phase
		if (mouseEncoderPhaseY == 1 || mouseEncoderPhaseY == 2) Y1_PORT |= Y1;	// Set Y1 to 1
		else Y1_PORT &= ~Y1;	// Set Y1 to 0
		if (mouseEncoderPhaseY == 0 || mouseEncoderPhaseY == 1) Y2_PORT |= Y2;	// Set Y2 to 1
		else Y2_PORT &= ~Y2;	// Set Y2 to 0

		// Decrement the distance left to move
		mouseDistanceY--;
	}
	
	// Set the timer top value for the next interrupt, or stop the interrupt if there is
	// nothing left to output
	if (mouseDistanceY > 0) OCR3A = yTimerTop;
	else TIMSK3 &= ~(1 << OCIE3A);
}

// Main function
int main(void)
{
	// Initialise the SmallyMouse2 hardware
	initialiseHardware();
	
	// Initialise the ISR timers
	initialiseTimers();

	// Enable interrupts (required for USB support and quadrature output ISRs)
	sei();
	
	// Main processing loop
	while(1) {
		// Perform any pending mouse actions
		processMouse();

		// Process the USB host interface
		USB_USBTask();
	}
}

// Initialise the SmallyMouse2 hardware
void initialiseHardware(void)
{
	// Disable the watchdog timer (if set in fuses)
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// Disable the clock divider (if set in fuses)
	clock_prescale_set(clock_div_1);

	// Set the quadrature output pins to output
	X1_DDR |= X1; // Output
	X2_DDR |= X2; // Output
	Y1_DDR |= Y1; // Output
	Y2_DDR |= Y2; // Output
	
	// Set quadrature output pins to zero
	X1_PORT &= ~X1; // Pin = 0
	X2_PORT &= ~X2; // Pin = 0
	Y1_PORT &= ~Y1; // Pin = 0
	Y2_PORT &= ~Y2; // Pin = 0
	
	// Set mouse button output pins open drain
	// (solves issues with some retro machines that
	// don't like 5V to be sources from the pins)
	LB_DDR &= ~LB; // 0 = input
	MB_DDR &= ~MB; // 0 = input
	RB_DDR &= ~RB; // 0 = input
	LB_PORT &= ~LB; // Pin = 0 (off)
	MB_PORT &= ~MB; // Pin = 0 (off)
	RB_PORT &= ~RB; // Pin = 0 (off)

	// Set the rate limit configuration header to input
	RATESW_DDR &= ~RATESW; // Input
	RATESW_PORT |= RATESW; // Turn on weak pull-up
	
	// Configure E7 on the expansion header to act as
	// DPISW (since it is easily jumpered to 0V)	
	DPISW_DDR &= ~DPISW; // Input
	DPISW_PORT |= DPISW; // Turn on weak pull-up
	
	// Initialise the expansion (Ian) header
	E0_DDR |= E0; // Output
	E1_DDR |= E1; // Output
	E2_DDR |= E2; // Output
	E3_DDR |= E3; // Output
	E4_DDR |= E4; // Output
	E5_DDR |= E5; // Output
	E6_DDR |= E6; // Output
	
	E0_PORT &= ~E0; // Pin = 0
	E1_PORT &= ~E1; // Pin = 0
	E2_PORT &= ~E2; // Pin = 0
	E3_PORT &= ~E3; // Pin = 0
	E4_PORT &= ~E4; // Pin = 0
	E5_PORT &= ~E5; // Pin = 0
	E6_PORT &= ~E6; // Pin = 0
	
	// Initialise the LUFA USB stack
	USB_Init();
	
	// By default, SmallyMouse2 will output USB debug events on the 
	// AVR's UART port.  You can monitor the debug console by connecting
	// a serial to USB adapter.  Only the Tx (D3 and 0V pins are required).
	
	// Initialise the serial UART - 9600 baud 8N1
	Serial_Init(9600, false);

	// Create a serial debug stream on stdio
	// Note: The serial UART is available on the 
	// expansion (Ian) header
	Serial_CreateStream(NULL);

	// Output some debug header information to the serial console
	puts_P(PSTR(ESC_FG_YELLOW "SmallyMouse2 V1.4 - Serial debug console\r\n" ESC_FG_WHITE));
	puts_P(PSTR(ESC_FG_YELLOW "(c)2017-2026 Simon Inns\r\n" ESC_FG_WHITE));
	puts_P(PSTR(ESC_FG_YELLOW "http://www.waitingforfriday.com\r\n" ESC_FG_WHITE));
	
	// Now report the status of the various configuration switches
	if ((RATESW_PIN & RATESW) == 0) puts_P(PSTR("Rate limit switch is ON\r\n"));
	else puts_P(PSTR("Rate limit switch is OFF\r\n"));
	if ((DPISW_PIN & DPISW) == 0) puts_P(PSTR("DPI divide switch is ON\r\n"));
	else puts_P(PSTR("DPI divide switch is OFF\r\n"));
}

// Initialise the ISR timers
void initialiseTimers(void)
{
	// The frequency of the reports from the USB mouse is about 100-125 Hz (i.e.
	// 100-125 reports per second).  Each report can indicate up to 127 position
	// movements in each direction (X & Y)
	
	// If we can receive 127 movements per report at a rate of 125 Hz
	// then the maximum movement per second is 100-125 Hz * 127 movements =
	// 12,700 - 15,875 movements per second (i.e. the quadrature output has 
	// to be 12,700 - 15,875 Hz to keep up)
	
	// The setting of the timers controls the maximum interrupt speed and 
	// therefore the maximum possible quadrature rate output.
	// 
	// Each movement unit reported by the USB device causes a single phase
	// change in the quadrature output (which means there is a 4:1 ratio
	// between the USB movement units and the quadrature movement output)
	
	// Timer1 and Timer3 are 16-bit timers in CTC mode, so each interrupt
	// period is (TOP + 1) ticks with a TOP of 0-65535.
	
	// Timer prescale is /8 and the AVR clock speed is 16,000,000 Hz
	// 16,000,000 Hz / 8 prescale = 2,000,000 ticks per second
	//
	// 1,000,000 uS per second / 2,000,000 ticks per second = 0.5 uS per tick
	//
	// The fine tick allows the output rate to closely follow the buffered
	// movement.  The fastest output is limited to Q_MAXRATE by
	// processMouseMovement() and the slowest possible is 32.768 mS per edge.

	// Configure Timer1 to interrupt (16-bit timer)
	OCR1A = Q_MAXRATE_TICKS - 1; // In CTC mode OCR1A = TOP
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS11); // Clear timer on compare match (CTC) mode, /8 prescale
	TIMSK1 = 0; // Timer1 interrupt disabled (started when there is movement to output)
		
	// Configure Timer3 to interrupt (16-bit timer)
	OCR3A = Q_MAXRATE_TICKS - 1; // In CTC mode OCR3A = TOP
	TCCR3A = 0;
	TCCR3B = (1 << WGM32) | (1 << CS31); // Clear timer on compare match (CTC) mode, /8 prescale
	TIMSK3 = 0; // Timer3 interrupt disabled (started when there is movement to output)
}

// Read the mouse USB report and process the information
void processMouse(void)
{
	USB_MouseReport_Data_t MouseReport;
	bool limitRate = false;
	bool dpiDivide = false;
		
	// Only process the mouse if a mouse is attached to the USB port
	if (USB_HostState != HOST_STATE_Configured)	return;
	
	// Get the state of the rate limiting (slow) header
	if ((RATESW_PIN & RATESW) == 0) limitRate = true;
	
	// Get the state of the DPI divider header
	if ((DPISW_PIN & DPISW) == 0) dpiDivide = true;
	
	// Select mouse data pipe
	Pipe_SelectPipe(MOUSE_DATA_IN_PIPE);

	// Unfreeze mouse data pipe
	Pipe_Unfreeze();

	// Has a packet arrived from the USB device?
	if (!(Pipe_IsINReceived())) {
		// Nothing received, exit...
		Pipe_Freeze();
		return;
	}

	// Ensure pipe is in the correct state before reading
	if (Pipe_IsReadWriteAllowed()) {
		// Set USB report processing activity on expansion port pin D0
		// Note: This can be used to analyze the rate at which USB mouse
		// reports are received by measuring the rate using an oscilloscope
		// or frequency counter on D0
		E0_PORT |= E0; // Pin = 1
		
		// Read in mouse report data.  The report is only used if a whole report is in the
		// pipe (so a short packet is never joined to the next one) and it was read without
		// error (e.g. the device was not detached during the read)
		if ((Pipe_BytesInPipe() < sizeof(MouseReport)) ||
			(Pipe_Read_Stream_LE(&MouseReport, sizeof(MouseReport), NULL) != PIPE_RWSTREAM_NoError)) {
			E0_PORT &= ~E0; // Pin = 0
			Pipe_ClearIN();
			Pipe_Freeze();
			return;
		}
		
		// The USB mouse report contains 3 variables: button, X and Y
		//
		// Button is an 8-bit flag containing up to 5 button states:
		//   (LSB)1 = Left button
		//        2 = Right button
		//        4 = Middle button
		//
		// Note: Buttons are active high (i.e. 1 = pressed)
		//
		// X and Y indicate the number of steps the mouse moved since the
		// last report was sent.
		//
		// +X = Mouse going right
		// -X = Mouse going left
		// +Y = Mouse going down
		// -Y = Mouse going up
		//
		// X and Y have a range of -127 to +127
		
		// If the mouse movement changes X direction then disregard any remaining movement
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			if (MouseReport.X > 0 && mouseDirectionX == 0) {
				mouseDistanceX = 0;
				mouseDirectionX = 1;
			} else if (MouseReport.X < 0 && mouseDirectionX == 1) {
				mouseDistanceX = 0;
				mouseDirectionX = 0;
			}
		}

		// If the mouse movement changes Y direction then disregard any remaining movement
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			if (MouseReport.Y > 0 && mouseDirectionY == 0) {
				mouseDistanceY = 0;
				mouseDirectionY = 1;
			} else if (MouseReport.Y < 0 && mouseDirectionY == 1) {
				mouseDistanceY = 0;
				mouseDirectionY = 0;
			}
		}
		
		// Process mouse X and Y movement -------------------------------------
		uint16_t xTop = processMouseMovement(MouseReport.X, MOUSEX, limitRate, dpiDivide);
		uint16_t yTop = processMouseMovement(MouseReport.Y, MOUSEY, limitRate, dpiDivide);
		
		// The timer TOP values are 16-bit and read by the ISRs
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			xTimerTop = xTop;
			yTimerTop = yTop;
		}
		
		// Restart the quadrature output if it stopped and there is new movement
		startQuadratureTimers();
		
		// Process mouse buttons ----------------------------------------------
		
		// Check for left mouse button
		if ((MouseReport.Button & 0x01) == 0) {
			// Open-drain
			LB_PORT &= ~LB;
			LB_DDR &= ~LB;
		} else {
			// Set to 0V
			LB_DDR |= LB; // 1 = output
			LB_PORT &= ~LB; // Button low
		}
			
		// Check for middle mouse button
		if ((MouseReport.Button & 0x04) == 0) {
			// Open-drain
			MB_PORT &= ~MB;
			MB_DDR &= ~MB;
		} else {
			// Set to 0V
			MB_DDR |= MB; // 1 = output
			MB_PORT &= ~MB; // Button low
		}
			
		// Check for right mouse button
		if ((MouseReport.Button & 0x02) == 0) {
			// Open-drain
			RB_PORT &= ~RB;
			RB_DDR &= ~RB;
		} else {
			// Set to 0V
			RB_DDR |= RB; // 1 = output
			RB_PORT &= ~RB; // Button low
		}
		
		// Clear USB report processing activity on expansion port pin D0
		E0_PORT &= ~E0; // Pin = 0
	}

	// Clear the IN endpoint, ready for next data packet
	Pipe_ClearIN();

	// Refreeze mouse data pipe
	Pipe_Freeze();
}

// Start the quadrature output ISRs if they are stopped and there is movement to output
//
// The first edge is output after the minimum period (Q_MAXRATE_TICKS), then the ISR
// loads the TOP value calculated from the buffered movement.  The counter is reset
// and any pending compare match is cleared, so the ISR never finds the counter
// already past a new TOP value (which would stall the output until the counter wraps).
void startQuadratureTimers(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (mouseDistanceX > 0 && !(TIMSK1 & (1 << OCIE1A))) {
			TCNT1 = 0;
			OCR1A = Q_MAXRATE_TICKS - 1;
			TIFR1 = (1 << OCF1A); // Clear any pending interrupt (write 1 to clear)
			TIMSK1 |= (1 << OCIE1A);
		}
		
		if (mouseDistanceY > 0 && !(TIMSK3 & (1 << OCIE3A))) {
			TCNT3 = 0;
			OCR3A = Q_MAXRATE_TICKS - 1;
			TIFR3 = (1 << OCF3A); // Clear any pending interrupt (write 1 to clear)
			TIMSK3 |= (1 << OCIE3A);
		}
	}
}

// Process the mouse movement units from the USB report
uint16_t processMouseMovement(int8_t movementUnits, uint8_t axis, bool limitRate, bool dpiDivide)
{
	uint16_t bufferedUnits = 0;
	uint32_t periodTicks = 0;
	int16_t bufferLimit = limitRate ? Q_RATELIMIT_BUFFERLIMIT : Q_BUFFERLIMIT;
	static int8_t dpiRemainder[2] = {0,0};
	volatile int16_t *mouseDistance = (axis == MOUSEX) ? &mouseDistanceX : &mouseDistanceY;
	
	// Apply DPI limiting if required
	//
	// The remainder of each division is carried forward to the next report so that
	// the long-term ratio is exactly 1/DPI_DIVIDER at all speeds (this avoids the
	// inverse mouse acceleration effect where slow movement is lost or at full DPI)
	if (dpiDivide) {
		// Discard any remainder left over from movement in the opposite direction
		if ((movementUnits > 0 && dpiRemainder[axis] < 0) ||
			(movementUnits < 0 && dpiRemainder[axis] > 0)) dpiRemainder[axis] = 0;
		
		int16_t totalUnits = movementUnits + dpiRemainder[axis];
		movementUnits = totalUnits / DPI_DIVIDER; // Truncates towards zero
		dpiRemainder[axis] = totalUnits % DPI_DIVIDER; // Same sign as totalUnits
	}
	
	// Add the movement units to the quadrature output buffer (the direction has already
	// been set from the sign of the report), apply the quadrature output buffer limit
	// and get the current value of the buffer.  This must be atomic as the ISRs are
	// also modifying the 16-bit buffer.
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (movementUnits > 0) *mouseDistance += movementUnits;
		else *mouseDistance -= movementUnits;
		
		if (*mouseDistance > bufferLimit) *mouseDistance = bufferLimit;
		
		bufferedUnits = *mouseDistance;
	}
	
	// Since the USB reports arrive at 100-125 Hz (even if there is only
	// a small amount of movement, we have to output the quadrature
	// at minimum rate to keep up with the reports (otherwise it creates
	// a slow lag).  The buffered movement is output over Q_DRAINTIME, so
	// with the default of 10,000 uS (i.e. 100 Hz of reports) the following
	// is true:
	//
	// 127 movements = 12,700 interrupts/sec
	// 100 movements = 10,000 interrupts/sec
	//  50 movements =  5,000 interrupts/sec
	//  10 movements =  1,000 interrupts/sec
	//   1 movement  =    100 interrupts/sec
	//
	// Timer speed is 2,000,000 ticks per second = 0.5 uS per tick and
	// Q_DRAINTIME is 20,000 ticks, so the ticks per interrupt are:
	// 20,000 / 127 = 157.48 ticks (78.5 uS)
	// 20,000 / 100 = 200 ticks (100 uS)
	// 20,000 / 50 = 400 ticks (200 uS)
	// 20,000 / 10 = 2,000 ticks (1,000 uS)
	// 20,000 / 1 = 20,000 ticks (10,000 uS)
	//
	// The ticks are rounded down (so the output drains slightly faster than
	// required, by at most 0.5 uS per interrupt).  The timer TOP value is
	// ticks - 1 (as the timer counts from 0 to TOP).
	
	if (bufferedUnits > 0) periodTicks = (uint16_t)Q_DRAINTIME_TICKS / bufferedUnits;
	else periodTicks = Q_MAXRATE_TICKS; // Nothing to output, avoid divide by zero
	
	// Don't exceed the maximum quadrature output rate
	if (periodTicks < Q_MAXRATE_TICKS) periodTicks = Q_MAXRATE_TICKS;
	
	// If the 'Slow' configuration jumper is shorted; apply the quadrature rate limit
	if (limitRate) {
		// Rate limit is on
		
		// Rate limit is provided in hertz
		// Each timer tick is 0.5 uS
		//
		// Convert hertz into period in uS
		// 1000 Hz = 1,000,000 / 1000 = 1000 uS
		//
		// Convert period into timer ticks (/ 4 due to quadrature)
		// 1000 uS / (0.5 * 4) = 500 ticks
		//
		// The ticks are rounded up, so the output never exceeds the
		// rate limit (see Q_HZ_TO_TICKS)
		
		// If the period is less than the rate limit, we output at the
		// maximum allowed rate.  This will cause additional lag that
		// is handled by the quadrature output buffer limit above.
		if (periodTicks < Q_RATELIMIT_TICKS) periodTicks = Q_RATELIMIT_TICKS;
	}
	
	// Return the timer TOP value (periodTicks is always 1-65536)
	return (uint16_t)(periodTicks - 1);
}

// LUFA event handlers ------------------------------------------------------------------------------------------------

// Event handler for the USB_DeviceAttached event. This indicates that a device has been attached to the host, and
// starts the library USB task to begin the enumeration and USB management process.
void EVENT_USB_Host_DeviceAttached(void)
{
	puts_P(PSTR(ESC_FG_GREEN "USB Device attached\r\n" ESC_FG_WHITE));
}

// Event handler for the USB_DeviceUnattached event. This indicates that a device has been removed from the host, and
// stops the library USB task management process.
void EVENT_USB_Host_DeviceUnattached(void)
{
	puts_P(PSTR(ESC_FG_GREEN "USB Device detached\r\n" ESC_FG_WHITE));
}

// Event handler for the USB_DeviceEnumerationComplete event. This indicates that a device has been successfully
// enumerated by the host and is now ready to be used by the application.
void EVENT_USB_Host_DeviceEnumerationComplete(void)
{
	puts_P(PSTR("Getting configuration data from device...\r\n"));

	uint8_t ErrorCode;

	/* Get and process the configuration descriptor data */
	if ((ErrorCode = ProcessConfigurationDescriptor()) != SuccessfulConfigRead) {
		if (ErrorCode == ControlError) {
			puts_P(PSTR(ESC_FG_RED "Control Error (Get configuration)!\r\n"));
		} else {
			puts_P(PSTR(ESC_FG_RED "Invalid Device!\r\n"));
		}

		printf_P(PSTR(" -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);
		return;
	}

	// Set the device configuration to the first configuration (rarely do devices use multiple configurations)
	if ((ErrorCode = USB_Host_SetDeviceConfiguration(1)) != HOST_SENDCONTROL_Successful) {
		printf_P(PSTR(ESC_FG_RED "Control Error (Set configuration)!\r\n"
		                         " -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);
		return;
	}
	
	// HID class request to set the mouse protocol to the Boot Protocol
	USB_ControlRequest = (USB_Request_Header_t) {
			.bmRequestType = (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE),
			.bRequest      = HID_REQ_SetProtocol,
			.wValue        = 0,
			.wIndex        = MouseInterfaceNumber,
			.wLength       = 0,
	};

	// Select the control pipe for the request transfer
	Pipe_SelectPipe(PIPE_CONTROLPIPE);

	// Send the request, display error and wait for device detach if request fails
	if ((ErrorCode = USB_Host_SendControlRequest(NULL)) != HOST_SENDCONTROL_Successful) {
		printf_P(PSTR(ESC_FG_RED "Control Error (Set protocol)!\r\n"
								 " -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);

		USB_Host_SetDeviceConfiguration(0);
		return;
	}

	puts_P(PSTR("USB Mouse enumeration successful\r\n"));
}

// Event handler for the USB_HostError event. This indicates that a hardware error occurred while in host mode.
void EVENT_USB_Host_HostError(const uint8_t ErrorCode)
{
	USB_Disable();

	printf_P(PSTR(ESC_FG_RED "Host Mode Error!\r\n"
	                       " -- Error Code %d\r\n" ESC_FG_WHITE), ErrorCode);

	while(1);
}

// Event handler for the USB_DeviceEnumerationFailed event. This indicates that a problem occurred while
// enumerating an attached USB device.
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t ErrorCode,
                                            const uint8_t SubErrorCode)
{
	printf_P(PSTR(ESC_FG_RED "Device Enumeration Error!\r\n"
	                         " -- Error Code %d\r\n"
	                         " -- Sub Error Code %d\r\n"
	                         " -- In State %d\r\n" ESC_FG_WHITE), ErrorCode, SubErrorCode, USB_HostState);
}


