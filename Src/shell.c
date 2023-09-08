/*
 *  shell.c
 *
 *  Created on: May 10, 2022
 *      Author: Jean Roch - Nefastor.com
 *
 *  Copyright 2022 Jean Roch
 *
 *  This file is part of STM Shell.
 *
 *  STM Shell is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  STM Shell is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with STM Shell.
 *  If not, see <https://www.gnu.org/licenses/>.
 */

#include "shell.h"

#include <string.h>
#include <stdio.h>

// Global instance of the shell state structure
// IMPORTANT : this variable needs to go into its own linker section so it can be placed where DMA can reach it
// If necessary, modify your linker script to add the ".shell" section and make sure it ends-up at an address that
// is compatible with DMA transfers to the shell's serial interface.
// It is OK for your linker script to not have this section : the linker will then ignore the attribute.
__attribute__ ((section (".shell"))) t_shell_state shell_state;

extern t_shell_block_entry root_block[];		// Must be declared by the application

// Function pointer for the current state of the shell :
void (*shell_fp)() = shell_state_init;

// ======= Main state machine state functions =======

// Initial state - Runs once; performs initialization
void shell_state_init ()
{
	// Empty the buffers by making them zero-length null-terminated strings :
	shell_state.input[0] = 0;
	shell_state.output[0] = 0;
	shell_state.busy = 0;			// 0 == No transfer in progress, 1 == Transfer in progress
	shell_state.index = 0;

	shell_state.command_fp = 0;	// No command in progress

	shell_state.shell =	shell_block;	// Setup the PFS shell block (native shell commands like "cd.." and "ls")
	shell_state.system = system_block;		// System block for now (application-defined, find a mechanism)
	shell_state.root = root_block;
	shell_state.block = shell_state.root;	// Shell starts at the root.

	// Initialize the path string :
	sprintf (shell_state.path, "\r\n/%s>", root_block[0].label);

	// Transition to output state immediately after initialization :
	shell_fp = shell_state_output;
}

// Displays the shell prompt or a line of command output, then transitions to user input state or back to command in progress
void shell_state_output ()
{
	static int state = 0;		// This state has sub-states

	switch (state)
	{
		case 0:		// Wait for previous DMA transfer to complete
			if (shell_state.busy == 0)
				state = 1;		// transition to next state if DMA is ready
			break;
		case 1:		// Transfer the output buffer : if a command isn't in progress, send the path instead
			if (shell_state.command_fp != 0) // then we're here because a command wants to send some text to the terminal
				shell_out (shell_state.output, strlen (shell_state.output));
			else
				shell_out (shell_state.path, strlen (shell_state.path));
			state = 2;			// transition to next state
			break;
		case 2:		// Wait for transfer to complete
			if (shell_state.busy == 0)
				state = 3;
			break;
		case 3:		// Transition to input state, unless a command is in progress :
			shell_fp = (shell_state.command_fp != 0) ? shell_state.command_fp : shell_state_input;
			state = 0;	// reset this state machine
			break;
	}
}

// WIP !!!
// PROBLEM : I have no way to reset its state. It should transition to a safe function (idle)
void shell_state_input ()
{
	// Currently, input data is processed by UART callback...
	shell_get_byte (&shell_state.c);	// ... But I need to arm the mechanism first.
	shell_fp = shell_state_idle;
}

void shell_state_idle ()
{
	// Do nothing. Used when waiting for the state machine state to be transitioned from an interrupt handler or callback
	// DANGER : the shell will get stuck in that state if a key is pressed outside of the input state !

	// Quick test : maybe I can rearm the read without danger :
	shell_get_byte (&shell_state.c); // THIS SOLVED THE BUG !!! But how clean is it ? Is it expensive ?
}

void shell_state_parser ()
{
	// Getting here means shell_state.command_fp must be zero, but for now let's just make sure
	shell_state.command_fp = 0;	// No command is currently executing (or we wouldn't be here)

	// Look for a matching command within the current block :
	int len = (int) shell_state.block[0].fp;	// The function pointer in a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is not supported
	for (int k = 1; k <= len; k++)	// Loop through the block. Skip the title entry
	{
		// Determine the index of the first space in the command label
		int j = 0;
		while (shell_state.block[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

		// Compare the command line's start to the command label in the block entry
		// if (strncmp(shell_state.input, shell_state.block[k].label, strlen(shell_state.block[k].label)) == 0)
		if (strncmp(shell_state.input, shell_state.block[k].label, j) == 0)
		{
			// Found a match ! Determine if it's a command or a sub-block
			if (shell_state.block[k].fp != 0)	// then it's a command !
			{
				shell_fp = shell_state.block[k].fp;
				shell_state.command_fp = shell_fp;	// Necessary for now, try to optimize-out in the future
				return;
			}
			if (shell_state.block[k].cb != 0) // then it's a child block (cb) !
			{
				// Edit the path string
				char *temp = shell_state.path + strlen (shell_state.path) - 1;	// Go to the last byte of the path string ('>'), which will be overwritten
				sprintf (temp, "/%s>", shell_state.block[k].cb[0].label);	// Append a slash, the child block's label, and a fresh ">"
				shell_state.block[k].cb->cb = shell_state.block;	// Initialize back-navigation pointer (cb of cb is actually parent block of cb, and that's the current block)
				shell_state.block = shell_state.block[k].cb;  // update the current block to the child block
				shell_fp = shell_state_output;	// transition back to prompt
				return;
			}

			// Getting here means that the matching block contains two null pointers, which is illegal : transition to the error state
			shell_fp = shell_state_error;
			return;
		}
	}

	// Getting here means the user's command wasn't found in the current block.
	// Let's check the system block (application commands available from any path)
	// This one is easier because it's flat (no sub-blocks)
	len = (int) shell_state.system[0].fp;	// function pointer on a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is supported : it is ignored
	if (len > 0)
	{
		for (int k = 1; k <= len; k++)	// skip the title entry
		{
			// Determine the index of the first space in the command label
			int j = 0;
			while (shell_state.system[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

			// Compare the command line's start to the command label in the block entry
			// if (strncmp(shell_state.input, shell_state.block[k].label, strlen(shell_state.block[k].label)) == 0)
			if (strncmp(shell_state.input, shell_state.system[k].label, j) == 0)
			{
				// Found a match ! It's a command, as there are no sub-blocks here
				if (shell_state.system[k].fp != 0)	// then it's a command !
				{
					shell_fp = shell_state.system[k].fp;
					shell_state.command_fp = shell_fp;	// Necessary for now, try to optimize-out in the future
				}
				return;	// job done
			}
		}
	}

	// Getting here means the user's command wasn't found in the current or the system block.
	// Let's check the shell block (native shell commands)
	// This one is easier because it's flat (no sub-blocks)
	// Because the shell block is not meant to be tailored to the application, safety checks are removed.
	len = (int) shell_state.shell[0].fp;	// function pointer on a block's title entry holds number of commands in the block
	// Note : a block with no command (len == 0) is not supported
	for (int k = 1; k <= len; k++)	// skip the title entry
	{
		// Determine the index of the first space in the command label
		int j = 0;
		while (shell_state.shell[k].label[j] != 32) j++; // Note : the index of the first space is also the length of the first word.

		// Compare the command line's start to the command label in the block entry
		// if (strncmp(shell_state.input, shell_state.block[k].label, strlen(shell_state.block[k].label)) == 0)
		if (strncmp(shell_state.input, shell_state.shell[k].label, j) == 0)
		{
			// Found a match ! It's a command, as there are no sub-blocks here
			shell_fp = shell_state.shell[k].fp;
			shell_state.command_fp = shell_fp;	// Necessary for now, try to optimize-out in the future
			return;	// job done
		}
	}

	// At the end of the parser, if no match has been found, go back to the prompt :
	shell_fp = shell_state_output;
}

///////////////// PORTABILITY LAYER //////////////////////////////////////////////////////////

// Feed each byte received by the UART or VCP to this function
// Should be called from the UART's "Rx Complete" callback or ISR
// Definitive version : uses the library's own buffer
// PROBLEM with the echo being too slow (but only cosmetic : input works fine). Look into it later.
// Actually this whole process may need some hardening, but it'll work for now.
void shell_in (char c)
{
	// Only process bytes if the shell is in the "idle" state (which follows the start of the input process)
	if (shell_fp == shell_state_idle)
	{

		// if the character is a carriage return, echo the whole buffer and reset
		// (this is only while debugging)
		if (c == 13)
		{
			shell_state.input[shell_state.index++] = 0;		// Add null termination
			shell_state.index = 0;	// reset index for next time
			// reception complete : transition to either the parser or the command in progress :
			shell_fp = (shell_state.command_fp != 0) ? shell_state.command_fp : shell_state_parser;
		}
		else
		{
			shell_get_byte (&shell_state.c);	// start receiving the next byte (it's not over until I get a CR)

			// while (shell_state.busy != 0);	// Don't echo unless the interface is ready (this may be unnecessary here)
			shell_out (&c, 1);	// echo this new byte. PROBLEM : too slow, on copy-paste to shell, not every character is echoed (but all are received successfully nonetheless)

			if (c != 127)				// not sure why, but PuTTY sends 127 for backspace. Should be 8 (ASCII)
				shell_state.input[shell_state.index++] = c;	// buffer the incoming byte and increment the buffer index
			else					// deal with backspace
			{
				if (shell_state.index > 0)	// Because you can't backspace before the first character
					shell_state.index--;		// This effectively erases the last character that was buffered
			}
		}

		if (shell_state.index == SHELL_BUFFER_SIZE)	// Buffer overflow protection
			shell_state.index--;
	}
}

// This function must be overridden by the application to match the target platform
__attribute__((weak)) void shell_out (char *buff, int length)
{
	// This function must transmit strlen(c) bytes starting from c.
}

// This function must be overridden by the application to match the target platform
// Its purpose is to read the next byte coming from the terminal and store it using the pointer argument
__attribute__((weak)) void shell_get_byte (char *c)
{
	// This function must request one byte from the communication interface, i.e. using the target's interrupt-based reception API
}

// This function is an error state : if the shell transitions to it, it means the library found
// itself in an unrecoverable situation. Example : the PFS contains a command with two null pointers,
// which is illegal. Override this function to implement application-specific handling of shell errors
__attribute__((weak)) void shell_state_error ()
{

}

