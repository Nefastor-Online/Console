/*
 * console_commands.c
 *
 * The console command block is the part of the PFS that contains the commands native to the console
 *
 *  Created on: May 13, 2022
 *      Author: Nefastor
 */


#include "console.h"		// For the structure declarations and global variables

#include <string.h>
#include <stdio.h>



// ========= Command Functions ==============================================================

// Navigate to the current block's parent block ("cd..")
void command_native_cddoubledot ()
{
	// If the current block is the root block, do nothing ! Otherwise, perform "cd.."
	// if (console_state.block != console_state.root)
	if (console_state.block[0].cb != 0)	// Another way to check it. Root block will not have a pointer to a child block.
	{
		// Truncate the path string
		int i = strlen (console_state.path);
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
			sprintf (console_state.output, "\r\n == %s ==", console_state.block[idx++].label);
			console_fp = console_state_output;	// transition to output state
			state = 2;	// transition to local state 2.
			break;
		case 2:		// wait for communication interface to be ready
			if (console_state.busy == 0) state = 3;
			break;
		case 3:		// print a block entry
			tag = (console_state.block[idx].fp != 0) ? 'C' : '>';			// Start by determining the tag
			sprintf (console_state.output, "\r\n %c %s", tag, console_state.block[idx++].label);
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


t_console_block_entry console_block[] =
{
		{ "", BLOCK_LEN 2, 0 },			// Title shouldn't be necessary, removing it to save space.
		{ "cd..", command_native_cddoubledot, 0},		// navigate towards the root
		{ "ls", command_native_list, 0}			// list commands in the current block
};

// Also declaring an empty system block to allow for compilation and operation even if the user doesn't declare their own
t_console_block_entry system_block[] =
{
		{ "", BLOCK_LEN 0, 0 }			// Zero length : the parser will skip this block
};

