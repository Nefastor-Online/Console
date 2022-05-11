/*
 * console.h
 *
 *  Created on: May 10, 2022
 *      Author: Nefastor
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

// Main data structure, with one global instance
typedef struct s_console_state
{
	unsigned char path[256];	// Current path and prompt.
	unsigned char input[256];	// Input buffer (stores a complete line)
	unsigned char output[256];	// Output buffer (used for DMA transfers)
	int busy;					// If non-zero, DMA transfer in progress
	int index;					// Input buffer index, used when receiving data from the console
	void (*command_fp)();		// Pointer to the function for the command in progress, or zero if no command in progress
	unsigned char c;			// Single-byte buffer
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

// Communication functions
void console_in (unsigned char c);		// Feed incoming bytes to this function
void console_out (unsigned char *buff, int length);		// Send buffer out the UART
void console_get_byte (unsigned char *c);

// Command functions
// void cmd_empty (unsigned char *buff);	// This function is empty, it's a safeguard during debugging.

#endif /* INC_CONSOLE_H_ */
