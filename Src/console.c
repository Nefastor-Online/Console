/*
 * console.c
 *
 *  Created on: May 10, 2022
 *      Author: Nefastor
 */

#include "console.h"

#include <string.h>
#include <stdio.h>

// Global instance of the console state structure
// Important : this variable needs to go into its own linker section so it can be placed where DMA can reach it
t_console_state console_state;

// Ingress buffer : a circular FIFO isn't necessary, commands should be short enough. Robustify later.
// Note : in order to support DMA on some MCU I may need to be able to relocate this buffer at an arbitrary address
// This should be done through linker voodoo
unsigned char buffer[256];
int buffer_index = 0;

// Function pointer for the current state of the console :
void (*console_fp)() = console_state_init;

// Main state machine state functions
void console_state_init ()
{
	// Empty the buffers by making them zero-length null-terminated strings :
	console_state.input[0] = 0;
	console_state.output[0] = 0;
	console_state.busy = 0;			// No transfer in progress
	console_state.index = 0;

	console_state.command_fp = 0;	// No command in progress

	// Initialize the path string :
	sprintf ((char *) console_state.path, "/>");

	// Transition to output state
	console_fp = console_state_output;
}

// SHOULD BE COMPLETE
void console_state_output ()
{
	// TO DO : Send prompt to console, transition to input state
	static int state = 0;		// This state has sub-states

	switch (state)
	{
		case 0:		// Wait for previous DMA transfer to complete
			if (console_state.busy == 0)
				state = 1;		// transition to next state if DMA is ready
			break;
		case 1:		// Transfer the output buffer : if a command isn't in progress, send the path instead
			if (console_state.command_fp != 0) // then we're here because a command wants to send some text to the console
				console_out (console_state.output, strlen ((char *) console_state.output));
			else
				console_out (console_state.path, strlen ((char *) console_state.path));
			state = 2;			// transition to next state
			break;
		case 2:		// Wait for transfer to complete
			if (console_state.busy == 0)
				state = 3;
			break;
		case 3:		// Transition to input state, unless a command is in progress :
			console_fp = (console_state.command_fp != 0) ? console_state.command_fp : console_state_input;
			state = 0;	// reset this state machine
			break;
	}
}

// WIP !!!
// PROBLEM : I have no way to reset its state. It should transition to a safe function (idle)
void console_state_input ()
{
	// Currently, input data is processed by UART callback. But I may need to arm the mechanism first.
	console_get_byte (&console_state.c);
	console_fp = console_state_idle;
}

void console_state_idle ()
{
	// Do nothing. Used when waiting for the state machine state to be transitioned from an interrupt handler or callback
}

void console_state_parser ()
{
	static int state = 0;

	// This state machine is for debug only, all it does is return what's been entered by the user
#if 0
	switch (state)
	{
		case 0:
			console_out(console_state.input, strlen((char *)console_state.input));
			state = 1;
			break;
		case 1:
			// Go back to the output state to start all over again
			console_fp = console_state_output;
			state = 0;		// reset this state machine
			break;
	}
#endif

	// The real parser
	switch (state)
	{

	}
}

// Empty function (debug only)
// void cmd_empty (unsigned char *buff) {}

// Feed each byte received by the UART or VCP to this function
// Should be called from the UART's "Rx Complete" callback or ISR
// Definitive version : uses the library's own buffer
// PROBLEM with the echo being too slow (but only cosmetic : input works fine). Look into it later.
void console_in (unsigned char c)
{
	// Only process bytes if the console is in the "idle" state (which follows the start of the input process)
	if (console_fp == console_state_idle)
	{

		// if the character is a carriage return, echo the whole buffer and reset
		// (this is only while debugging)
		if (c == 13)
		{
			console_state.input[console_state.index++] = 13;		// Add CR
			console_state.input[console_state.index++] = 10;		// Add LF
			console_state.input[console_state.index++] = 0;		// Add null termination
			// console_out (buffer, buffer_index);	// was debug : send out full buffer
			console_state.index = 0;	// reset index for next time
			// reception complete : transition to either the parser or the command in progress :
			console_fp = (console_state.command_fp != 0) ? console_state.command_fp : console_state_parser;
		}
		else
		{
			console_get_byte (&console_state.c);	// start receiving the next byte (it's not over until I get a CR)
			console_out (&c, 1);	// echo this new byte. PROBLEM : too slow, on copy-paste to console, not every character is echoed (but all are received successfully nonetheless)

			if (c != 127)				// not sure why, but PuTTY sends 127 for backspace
				console_state.input[console_state.index++] = c;	// buffer the incoming byte and increment the buffer index
			else					// deal with backspace
			{
				if (console_state.index > 0)	// Because you can't backspace before the first character
					console_state.index--;		// This effectively erases the last character that was buffered
			}
		}

		if (console_state.index == 254)	// Buffer overflow protection
			console_state.index--;
	}
}

// This function must be overridden by the application to match the target platform
__attribute__((weak)) void console_out (unsigned char *buff, int length)
{
	// This function must transmit strlen(c) bytes starting from c.
}

// This function must be overridden by the application to match the target platform
// Its purpose is to read the next byte coming from the console and store it using the pointer argument
__attribute__((weak)) void console_get_byte (unsigned char *c)
{
	// This function must transmit strlen(c) bytes starting from c.
}

