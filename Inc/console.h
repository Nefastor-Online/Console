/*
 * console.h
 *
 *  Created on: May 10, 2022
 *      Author: Nefastor
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

// Function pointer for the current state of the console :
extern void (*console_fp)();

// Communication functions
void console_in (unsigned char c);		// Feed incoming bytes to this function
void console_out (unsigned char *buff, int length);		// Send buffer out the UART

// Command functions
void cmd_empty (unsigned char *buff);	// This function is empty, it's a safeguard during debugging.

#endif /* INC_CONSOLE_H_ */
