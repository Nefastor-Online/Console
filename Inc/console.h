/*
 * console.h
 *
 *  Created on: May 10, 2022
 *      Author: Nefastor
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

// Pseudo file system command block structure
typedef struct t_console_block_entry t_console_block_entry;
struct t_console_block_entry
{
	char label[80];	// Command and parameter list, or block title
	void (*fp)();				// Pointer to the function for this command
	t_console_block_entry *cb;			// Child block, if the entry is for a "sub block". If this entry is a block title, this points to the parent block, if any
};

// Global instances of blocks declared in separate source files for clarity :
extern t_console_block_entry console_block[];
extern t_console_block_entry system_block[];	// The library provides an empty place-holder (see "console_pfs.c")

// Type cast macros to simplify coding PFS blocks
#define BLOCK_LEN (void (*)())	// macro to simplify using a function pointer to hold a number
#define CMD_BLOCK (t_console_block_entry*)

// Main data structure, with one global instance
typedef struct s_console_state
{
	char path[256];	// Current path and prompt.
	char input[256];	// Input buffer (stores a complete line)
	char output[256];	// Output buffer (used for DMA transfers)
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
void console_state_init ();			// Initial state of the console. Performs initializations.
void console_state_output ();		// Output state, sends the prompt to the console.
void console_state_input ();		// Acquire user input
void console_state_idle ();
void console_state_parser ();		// Parse user input

// Portability layer (Console communication interface. Weak functions to be overridden by target-specific implementations)
void console_in (char c);		// Feed incoming bytes to this function
void console_out (char *buff, int length);		// Send buffer out the UART
void console_get_byte (char *c);		// Called by the console to read a byte from the UART
void console_state_error ();			// The console transitions to this state in case of unrecoverable error.

#endif /* INC_CONSOLE_H_ */
