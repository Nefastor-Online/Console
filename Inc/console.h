/*
 * console.h
 *
 *  Created on: May 10, 2022
 *      Author: Jean Roch - Nefastor.com
 *
 *  Copyright 2022 Jean Roch
 *
 *  This file is part of The Console.
 *
 *  The Console is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  The Console is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with The Console.
 *  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

#include <string.h>

#define CONSOLE_LABEL_LENGTH	80
#define CONSOLE_BUFFER_SIZE		256

// Pseudo file system command block structure
typedef struct t_console_block_entry t_console_block_entry;
struct t_console_block_entry
{
	char label[CONSOLE_LABEL_LENGTH];	// Command and parameter list, or block title
	void (*fp)();				// Pointer to the function for this command
	t_console_block_entry *cb;			// Child block, if the entry is for a "sub block". If this entry is a block title, this points to the parent block, if any
};

// Global instances of blocks declared in separate source files for clarity :
extern t_console_block_entry console_block[];
extern t_console_block_entry system_block[];	// The library provides an empty place-holder (see "console_pfs.c")

// Type cast macros to simplify coding PFS blocks
#define BLOCK_LEN (void (*)())	// macro to simplify using a function pointer to hold a number
#define CMD_BLOCK (t_console_block_entry*)

// Macros to simplify coding command functions
#define COMMAND_END	{console_state.command_fp = 0; console_fp = console_state_output;}	// nullifies current command pointer, transitions back to the prompt
#define CONSOLE_PRINT(S) {console_out(S, strlen(S)); while (console_state.busy == 1);}	// prints a string to the console, blocks until transmission is complete

// Main data structure, with one global instance
typedef struct s_console_state
{
	char path[CONSOLE_BUFFER_SIZE];	// Current path and prompt.
	char input[CONSOLE_BUFFER_SIZE];	// Input buffer (stores a complete line)
	char output[CONSOLE_BUFFER_SIZE];	// Output buffer (used for DMA transfers)
	int busy;					// If non-zero, DMA transfer in progress
	int index;					// Input buffer index, used when receiving data from the console
	void (*command_fp)();		// Pointer to the function for the command in progress, or zero if no command in progress

	// PFS :
	t_console_block_entry *root;	// Root block for the application-specific command blocks tree
	t_console_block_entry *block;	// Current block (commands located at "path")
	t_console_block_entry *console;	// Console block;
	t_console_block_entry *system;		// System block;

	char c;			// Single-byte input buffer
} t_console_state;

extern t_console_state console_state;

// Function pointer for the current state of the console :
extern void (*console_fp)();

// Main state machine state functions
void console_state_init ();			// Initialization state.
void console_state_output ();		// Output state, sends the prompt to the console.
void console_state_input ();		// Start acquiring user input.
void console_state_idle ();			// Wait for user input to complete.
void console_state_parser ();		// Parse user input.

// Portability layer (Console communication interface. Weak functions to be overridden by target-specific implementations)
void console_in (char c);		// Feed incoming bytes to this function
void console_out (char *buff, int length);		// Send buffer out the UART
void console_get_byte (char *c);		// Called by the console to read a byte from the UART
void console_state_error ();			// The console transitions to this state in case of unrecoverable error.

#endif /* INC_CONSOLE_H_ */
