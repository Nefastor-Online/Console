/*
 *  shell.h
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

#ifndef INC_SHELL_H_
#define INC_SHELL_H_

#include <string.h>

#define SHELL_LABEL_LENGTH		80
#define SHELL_BUFFER_SIZE		256

// Pseudo file system command block structure
typedef struct t_shell_block_entry t_shell_block_entry;
struct t_shell_block_entry
{
	char label[SHELL_LABEL_LENGTH];	// Command and parameter list, or block title
	void (*fp)();					// Pointer to the function for this command
	t_shell_block_entry *cb;		// Child block, if the entry is for a "sub block". If this entry is a block title, this points to the parent block, if any
};

// Global instances of blocks declared in separate source files for clarity :
extern t_shell_block_entry shell_block[];
extern t_shell_block_entry system_block[];	// The library provides an empty place-holder (see "shell_pfs.c")

// Type cast macros to simplify coding PFS blocks
#define BLOCK_LEN (void (*)())	// macro to simplify using a function pointer to hold a number
#define CMD_BLOCK (t_shell_block_entry*)

// Macros to simplify coding command functions
#define COMMAND_END	{shell_state.command_fp = 0; shell_fp = shell_state_output;}	// nullifies current command pointer, transitions back to the prompt
#define SHELL_PRINT(S) {shell_out(S, strlen(S)); while (shell_state.busy == 1);}	// prints a string to the terminal, blocks until transmission is complete

// Main data structure, with one global instance
typedef struct s_shell_state
{
	char path[SHELL_BUFFER_SIZE];	// Current path and prompt.
	char input[SHELL_BUFFER_SIZE];	// Input buffer (stores a complete line)
	char output[SHELL_BUFFER_SIZE];	// Output buffer (used for DMA transfers)
	int busy;						// If non-zero, DMA transfer in progress
	int index;						// Input buffer index, used when receiving data from the shell
	void (*command_fp)();			// Pointer to the function for the command in progress, or zero if no command in progress

	// PFS :
	t_shell_block_entry *root;		// Root block for the application-specific command blocks tree
	t_shell_block_entry *block;		// Current block (commands located at "path")
	t_shell_block_entry *shell;		// Shell block;
	t_shell_block_entry *system;	// System block;

	char c;							// Single-byte input buffer
} t_shell_state;

extern t_shell_state shell_state;

// Function pointer for the current state of the shell :
extern void (*shell_fp)();

// Main state machine state functions
void shell_state_init ();			// Initialization state.
void shell_state_output ();		// Output state, sends the prompt to the terminal.
void shell_state_input ();		// Start acquiring user input.
void shell_state_idle ();			// Wait for user input to complete.
void shell_state_parser ();		// Parse user input.

// Portability layer (Shell communication interface. Weak functions to be overridden by target-specific implementations)
void shell_in (char c);		// Feed incoming bytes to this function
void shell_out (char *buff, int length);		// Send buffer out the UART
void shell_get_byte (char *c);		// Called by the shell to read a byte from the UART
void shell_state_error ();			// The shell transitions to this state in case of unrecoverable error.

// Logging function
void shell_log (char *message);
#define LOG(a) shell_log((a))		// In case you prefer your logging function "high-visibility"

#endif /* INC_SHELL_H_ */
