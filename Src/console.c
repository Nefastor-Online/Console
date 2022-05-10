/*
 * console.c
 *
 *  Created on: May 10, 2022
 *      Author: Nefastor
 */

#include "console.h"

// Ingress buffer : a circular FIFO isn't necessary, commands should be short enough. Robustify later.
// Note : in order to support DMA on some MCU I may need to be able to relocate this buffer at an arbitrary address
// This should be done through linker voodoo
unsigned char buffer[256];
int buffer_index = 0;

// Function pointer for the current state of the console :
void (*console_fp)() = cmd_empty;

// Empty function (debug only)
void cmd_empty (unsigned char *buff) {}

// Feed each byte received by the UART or VCP to this function
// Should be called from the UART's "Rx Complete" callback or ISR
void console_in (unsigned char c)
{
	// if the character is a carriage return, echo the whole buffer and reset
	// (this is only while debugging)
	if (c == 13)
	{
		buffer[buffer_index++] = 13;	// Add CR
		console_out (buffer, buffer_index);
		buffer_index = 0;
	}
	else
	{
		console_out (&c, 1);	// echo this new byte
		if (c != 127)				// not sure why, but PuTTY sends 127 for backspace
			buffer[buffer_index++] = c;	// buffer the incoming byte and increment the buffer index
		else					// deal with backspace
			if (buffer_index > 0)	// Because you can't backspace before the first character
				buffer_index--;		// This effectively erases the last character that was buffered
	}

	if (buffer_index == 256)	// Buffer overflow protection
		buffer_index--;
}

// This function should be overridden by the application to match the target platform
__attribute__((weak)) void console_out (unsigned char *buff, int length)
{
	// This function must transmit strlen(c) bytes starting from c.
}

