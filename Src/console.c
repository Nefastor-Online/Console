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
// IMPORTANT : this variable needs to go into its own linker section so it can be placed where DMA can reach it
t_console_state console_state;

extern t_console_block_entry root_block[];		// Must be declared by the application

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

	console_state.console =	console_block;	// Setup the PFS console block (native console commands like "cd..")
	console_state.system = system_block;		// System block for now (application-defined, find a mechanism)
	console_state.root = root_block;
	console_state.block = console_state.root;	// Console starts at the root.

	// Initialize the path string :
	sprintf (console_state.path, "\r\n/%s>", root_block[0].label);

	// Transition to output state immediately after initialization :
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
				console_out (console_state.output, strlen (console_state.output));
			else
				console_out (console_state.path, strlen (console_state.path));
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
	// Currently, input data is processed by UART callback...
	console_get_byte (&console_state.c);	// ... But I need to arm the mechanism first.
	console_fp = console_state_idle;
}

void console_state_idle ()
{
	// Do nothing. Used when waiting for the state machine state to be transitioned from an interrupt handler or callback
	// DANGER : the console will get stuck in that state if a key is pressed outside of the input state !

	// Quick test : maybe I can rearm the read without danger :
	console_get_byte (&console_state.c); // THIS SOLVED THE BUG !!! But how clean is it ? Is it expensive ?
}

void console_state_parser ()
{
	// Getting here means console_state.command_fp must be zero, but for now let's just make sure
	console_state.command_fp = 0;	// No command is currently executing (or we wouldn't be here)

	// Look for a matching command within the current block :
	int len = (int) console_state.block[0].fp;	// The function pointer in a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is not supported
	for (int k = 1; k <= len; k++)	// Loop through the block. Skip the title entry
	{
		// Determine the index of the first space in the command label
		int j = 0;
		while (console_state.block[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

		// Compare the command line's start to the command label in the block entry
		// if (strncmp(console_state.input, console_state.block[k].label, strlen(console_state.block[k].label)) == 0)
		if (strncmp(console_state.input, console_state.block[k].label, j) == 0)
		{
			// Found a match ! Determine if it's a command or a sub-block
			if (console_state.block[k].fp != 0)	// then it's a command !
			{
				console_fp = console_state.block[k].fp;
				console_state.command_fp = console_fp;	// Necessary for now, try to optimize-out in the future
				return;
			}
			if (console_state.block[k].cb != 0) // then it's a child block (cb) !
			{
				// Edit the path string
				char *temp = console_state.path + strlen (console_state.path) - 1;	// Go to the last byte of the path string ('>'), which will be overwritten
				sprintf (temp, "/%s>", console_state.block[k].cb[0].label);	// Append a slash, the child block's label, and a fresh ">"
				console_state.block[k].cb->cb = console_state.block;	// Initialize back-navigation pointer (cb of cb is actually parent block of cb, and that's the current block)
				console_state.block = console_state.block[k].cb;  // update the current block to the child block
				console_fp = console_state_output;	// transition back to prompt
				return;
			}

			// Getting here means that the matching block contains two null pointers, which is illegal : transition to the error state
			console_fp = console_state_error;
			return;
		}
	}

	// Getting here means the user's command wasn't found in the current block.
	// Let's check the system block (application commands available from any path)
	// This one is easier because it's flat (no sub-blocks)
	len = (int) console_state.system[0].fp;	// function pointer on a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is supported : it is ignored
	if (len > 0)
	{
		for (int k = 1; k <= len; k++)	// skip the title entry
		{
			// Determine the index of the first space in the command label
			int j = 0;
			while (console_state.system[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

			// Compare the command line's start to the command label in the block entry
			// if (strncmp(console_state.input, console_state.block[k].label, strlen(console_state.block[k].label)) == 0)
			if (strncmp(console_state.input, console_state.system[k].label, j) == 0)
			{
				// Found a match ! It's a command, as there are no sub-blocks here
				if (console_state.system[k].fp != 0)	// then it's a command !
				{
					console_fp = console_state.system[k].fp;
					console_state.command_fp = console_fp;	// Necessary for now, try to optimize-out in the future
				}
				return;	// job done
			}
		}
	}

	// Getting here means the user's command wasn't found in the current or the system block.
	// Let's check the console block (native console commands)
	// This one is easier because it's flat (no sub-blocks)
	// Because the console block is not meant to be tailored to the application, safety checks are removed.
	len = (int) console_state.console[0].fp;	// function pointer on a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is not supported
	for (int k = 1; k <= len; k++)	// skip the title entry
	{
		// Determine the index of the first space in the command label
		int j = 0;
		while (console_state.console[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

		// Compare the command line's start to the command label in the block entry
		// if (strncmp(console_state.input, console_state.block[k].label, strlen(console_state.block[k].label)) == 0)
		if (strncmp(console_state.input, console_state.console[k].label, j) == 0)
		{
			// Found a match ! It's a command, as there are no sub-blocks here
			console_fp = console_state.console[k].fp;
			console_state.command_fp = console_fp;	// Necessary for now, try to optimize-out in the future
			return;	// job done
		}
	}

	// At the end of the parser, if no match has been found, go back to the prompt :
	console_fp = console_state_output;
}

///////////////// PORTABILITY LAYER //////////////////////////////////////////////////////////

// Feed each byte received by the UART or VCP to this function
// Should be called from the UART's "Rx Complete" callback or ISR
// Definitive version : uses the library's own buffer
// PROBLEM with the echo being too slow (but only cosmetic : input works fine). Look into it later.
// Actually this whole process may need some hardening, but it'll work for now.
void console_in (char c)
{
	// Only process bytes if the console is in the "idle" state (which follows the start of the input process)
	if (console_fp == console_state_idle)
	{

		// if the character is a carriage return, echo the whole buffer and reset
		// (this is only while debugging)
		if (c == 13)
		{
			console_state.input[console_state.index++] = 0;		// Add null termination
			console_state.index = 0;	// reset index for next time
			// reception complete : transition to either the parser or the command in progress :
			console_fp = (console_state.command_fp != 0) ? console_state.command_fp : console_state_parser;
		}
		else
		{
			console_get_byte (&console_state.c);	// start receiving the next byte (it's not over until I get a CR)

			// while (console_state.busy != 0);	// Don't echo unless the interface is ready (this may be unnecessary here)
			console_out (&c, 1);	// echo this new byte. PROBLEM : too slow, on copy-paste to console, not every character is echoed (but all are received successfully nonetheless)

			if (c != 127)				// not sure why, but PuTTY sends 127 for backspace. Should be 8 (ASCII)
				console_state.input[console_state.index++] = c;	// buffer the incoming byte and increment the buffer index
			else					// deal with backspace
			{
				if (console_state.index > 0)	// Because you can't backspace before the first character
					console_state.index--;		// This effectively erases the last character that was buffered
			}
		}

		if (console_state.index == 256)	// Buffer overflow protection
			console_state.index--;
	}
}

// This function must be overridden by the application to match the target platform
__attribute__((weak)) void console_out (char *buff, int length)
{
	// This function must transmit strlen(c) bytes starting from c.
}

// This function must be overridden by the application to match the target platform
// Its purpose is to read the next byte coming from the console and store it using the pointer argument
__attribute__((weak)) void console_get_byte (char *c)
{
	// This function must request one byte from the communication interface, i.e. using the target's interrupt-based reception API
}

// This function is an error state : if the console transitions to it, it means the library found
// itself in an unrecoverable situation. Example : the PFS contains a command with two null pointers,
// which is illegal. Override this function to implement application-specific handling of console errors
__attribute__((weak)) void console_state_error ()
{

}

