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

// Type cast macros to simplify coding PFS blocks
#define BLOCK_LEN (void (*)())	// macro to simplify using a function pointer to hold a number
#define CMD_BLOCK (t_console_block_entry*)


// The console command block is the part of the PFS that contains the commands native to the console
t_console_block_entry console_block[] =
{
		{ "", BLOCK_LEN 2, 0 },			// Title shouldn't be necessary, removing it to save space.
		{ "cd..", command_native_cddoubledot, 0},		// navigate towards the root
		{ "ls", command_native_list, 0}			// list commands in the current block
};

// Demo system block, should be declared by the application.
t_console_block_entry system_block[] =
{
		{ "", BLOCK_LEN 1, 0 },			// Title shouldn't be necessary, removing it to save space.
		{"count - this is a demo function", command_count, 0}		// Demo function
};

// Demo root command block, should be declared by the application.
// I need to come up with an initialization mechanism to get the pointer to the console_state structure.
// TEST ONLY : sub-blocks for navigation testing
t_console_block_entry level_2_block[] =
{
		{"Level 2", BLOCK_LEN 2, 0},	// Title block. Parent block is level 1 block
		{"count - this is a demo function", command_count, 0},		// Demo function
		{"count - this is a demo function", command_count, 0}		// Demo function
};

t_console_block_entry level_1_block[] =
{
		{"Level 1", BLOCK_LEN 3, 0},	// Title block. Parent block is root.
		{"count - this is a demo function", command_count, 0},		// Demo function
		{"count - this is a demo function", command_count, 0},		// Demo function
		{"lvl2 - sub-block 2", 0, CMD_BLOCK level_2_block}
};

t_console_block_entry root_block[] =
{
		{"STM32", BLOCK_LEN 2, 0},	// Title block. Root, so no parent block. No function. Function pointer replaced by command count in the block
		{"count - this is a demo function", command_count, 0},		// Demo function
		{"lvl1 - sub-block 1", 0, CMD_BLOCK level_1_block}
};

// Function pointer for the current state of the console :
void (*console_fp)() = console_state_init;

// Main state machine state functions
void console_state_init ()
{
	// Small design issue : I can't initialize the sub-blocks' parent block pointers directly, due to the order of block declarations.
	// I'll need to do it from an init function :
	// Setup PFS navigation links
	level_1_block[0].cb = root_block;
	level_2_block[0].cb = level_1_block;

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
	sprintf ((char *) console_state.path, "\r\n/%s>", root_block[0].label);

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

#if 0	// This state machine is for debug only, all it does is return what's been entered by the user
	static int state = 0;
	int len;

	switch (state)
	{
		case 0:
			len = strlen((char *)console_state.input);
			if (len > 0)	// Don't echo empty string, this is somehow problematic.
				console_out(console_state.input, len);
			state = 1;
			break;
		case 1:
			if (console_state.busy == 0)
				state = 2;		// transition after output complete
			break;
		case 2:
			// Go back to the output state to start all over again
			console_fp = console_state_output;
			state = 0;		// reset this state machine
			break;
	}
#endif

	// The real parser isn't a state machine, it does its job and transitions right away
#if 0
	// Simple parsing test : in the future, replace with loops through blocks
	if (strncmp((char *) console_state.input, "count", 5) == 0)
	{
		// launch the demo counter function
		console_fp = command_count;
		console_state.command_fp = console_fp;	// Necessary for now, try to optimize-out in the future
		// return;
	}
	else
		console_fp = console_state_output;	// ignore any other command line and return to prompt
#endif

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
		// if (strncmp((char *) console_state.input, (char *) console_state.block[k].label, strlen((char *) console_state.block[k].label)) == 0)
		if (strncmp((char *) console_state.input, (char *) console_state.block[k].label, j) == 0)
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
				char *temp = (char *) console_state.path;	// Start of the path string
				temp += strlen ((char *) console_state.path);	// Go to one byte after the end of the path string
				temp--;		// step back to the last character (the ">" at the end of the prompt) to overwrite it
				sprintf (temp, "/%s>", console_state.block[k].cb[0].label);	// Append a slash, the child block's label, and a fresh ">"
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
	// Note : a block with no command (len == 0) is not supported
	for (int k = 1; k <= len; k++)	// skip the title entry
	{
		// Determine the index of the first space in the command label
		int j = 0;
		while (console_state.system[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

		// Compare the command line's start to the command label in the block entry
		// if (strncmp((char *) console_state.input, (char *) console_state.block[k].label, strlen((char *) console_state.block[k].label)) == 0)
		if (strncmp((char *) console_state.input, (char *) console_state.system[k].label, j) == 0)
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
		// if (strncmp((char *) console_state.input, (char *) console_state.block[k].label, strlen((char *) console_state.block[k].label)) == 0)
		if (strncmp((char *) console_state.input, (char *) console_state.console[k].label, j) == 0)
		{
			// Found a match ! It's a command, as there are no sub-blocks here
//			if (console_state.console[k].fp != 0)	// then it's a command !
//			{
				console_fp = console_state.console[k].fp;
				console_state.command_fp = console_fp;	// Necessary for now, try to optimize-out in the future
//			}
			return;	// job done
		}
	}

	// The parser will test multiple blocks, but should stop as soon as it finds a match.
	// To do that, test console_state.command_fp for a non-zero value, denoting that a match has been found,
	// and return early :
	//if (console_state.command_fp != 0)	// PROBLEM : DOES NOT DETECT A SIMPLE CHANGE OF PATH
	//	return;

	// At the end of the parser, if no match has been found, go back to the prompt :
	console_fp = console_state_output;
}

// Empty function (debug only)
// void cmd_empty (unsigned char *buff) {}

// Feed each byte received by the UART or VCP to this function
// Should be called from the UART's "Rx Complete" callback or ISR
// Definitive version : uses the library's own buffer
// PROBLEM with the echo being too slow (but only cosmetic : input works fine). Look into it later.
// Actually this whole process may need some hardening, but it'll work for now.
void console_in (unsigned char c)
{
	// Only process bytes if the console is in the "idle" state (which follows the start of the input process)
	if (console_fp == console_state_idle)
	{

		// if the character is a carriage return, echo the whole buffer and reset
		// (this is only while debugging)
		if (c == 13)
		{
			// In the definitive code I can strip the CR+LF
			// console_state.input[console_state.index++] = 13;		// Add CR
			// console_state.input[console_state.index++] = 10;		// Add LF
			console_state.input[console_state.index++] = 0;		// Add null termination
			// console_out (buffer, buffer_index);	// was debug : send out full buffer
			console_state.index = 0;	// reset index for next time
			// reception complete : transition to either the parser or the command in progress :
			console_fp = (console_state.command_fp != 0) ? console_state.command_fp : console_state_parser;
		}
		else
		{
			console_get_byte (&console_state.c);	// start receiving the next byte (it's not over until I get a CR)

			// while (console_state.busy != 0);	// Don't echo unless the interface is ready (this may be unnecessary here)
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

// This function is an error state : if the console transitions to it, it means the library found
// itself in an unrecoverable situation. Example : the PFS contains a command with two null pointers,
// which is illegal. Override this function to implement application-specific handling of console errors
__attribute__((weak)) void console_state_error ()
{

}

// Demo command functions, test / debug only :
void command_count ()
{
	static int cnt = 0;
	static int state = 0;
	static volatile long long accu = 0;
	int k;

	switch (state)
	{
		case 0:		// wait for previous DMA transfer to complete, and then transition
			if (console_state.busy == 0)
				state = 1;
			break;
		case 1:		// send out the counter's value as a string and increment
			// Print straight to the output buffer
			sprintf ((char *) console_state.output, "\r\nValues : %i %li", cnt++, (long) (accu / 10000));
			console_fp = console_state_output;	// transition to output state
			state = 2;	// transition to local state 2.
			break;
		case 2:		// end test
			state = 0;		// loop back to keep counting or reset the state machine
			if (cnt == 1000)
			{
				cnt = 0;							// clear the counter
				console_fp = console_state_output;	// transition to output state...
				console_state.command_fp = 0;		// ... but this command ends, so we'll be returning to the prompt
			}
			else
			{
				// Do some time-wasting processing (load check)
				for (k = 0; k < 10000; k++)
					accu += k * cnt;
			}
			break;
	}

}

// Console commands ============================================================================

// Navigate to the current block's parent block ("cd..")
void command_native_cddoubledot ()
{
	// If the current block is the root block, do nothing ! Otherwise, perform "cd.."
	// if (console_state.block != console_state.root)
	if (console_state.block[0].cb != 0)	// Another way to check it. Root block will not have a pointer to a child block.
	{
		// Truncate the path string
		int i = strlen ((char *) console_state.path);
		while (i--)	// move back towards the start of the path string
			if (console_state.path[i] == '/')	// find the last slash...
			{
				console_state.path[i++] = '>';	// ... and terminate the string there
				console_state.path[i] = 0;
				break;
			}
		// Update the current block pointer
		console_state.block = console_state.block[0].cb;	// "cb" of title entry is parent block pointer
	}

	// In all cases, transition to the prompt
	console_state.command_fp = 0;
	console_fp = console_state_output;
}

// List the contents of the current block ("ls")
void command_native_list ()
{
	static int state = 0;
	static int idx = 0;
	char tag;		// Command entries get a "C" on their line, sub-blocks have a ">"

	switch (state)
	{
		case 0:		// wait for communication interface to be ready
			if (console_state.busy == 0) state = 1;
			break;
		case 1:		// print the title of the current block
			// Print straight to the output buffer
			sprintf ((char *) console_state.output, "\r\n == %s ==", (char *) console_state.block[idx++].label);
			console_fp = console_state_output;	// transition to output state
			state = 2;	// transition to local state 2.
			break;
		case 2:		// wait for communication interface to be ready
			if (console_state.busy == 0) state = 3;
			break;
		case 3:		// print a block entry
			tag = (console_state.block[idx].fp != 0) ? 'C' : '>';			// Start by determining the tag
			sprintf ((char *) console_state.output, "\r\n %c %s", tag, (char *) console_state.block[idx++].label);
			console_fp = console_state_output;	// transition to output state
			// The local state transition depends on the value of idx
			if (idx > (int) console_state.block[0].fp)	// check against number of commands in the block
				state = 4;	// End of block reached, command will complete on next call
			else
				state = 2;	// Loop to wait for DMA completion and transfer next entry
			break;
		case 4:	// end of command
			console_state.command_fp = 0;
			console_fp = console_state_output;	// back to the prompt
			state = idx = 0;		// reset this command's state before leaving
			break;
	}

}

