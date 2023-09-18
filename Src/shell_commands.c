/*
 *  shell_commands.c
 *
 *  The shell command block is the part of the PFS that contains the commands native to the shell
 *
 *  Created on: May 13, 2022
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


#include "shell.h"		// For the structure declarations and global variables

#include <string.h>
#include <stdio.h>



// ========= Command Functions ==============================================================

// Navigate to the current block's parent block ("cd..")
void command_native_cddoubledot ()
{
	// If the current block is the root block, do nothing ! Otherwise, perform "cd.."
	// if (shell_state.block != shell_state.root)
	if (shell_state.block[0].cb != 0)	// Another way to check it. Root block will not have a pointer to a child block.
	{
		// Truncate the path string
		int i = strlen (shell_state.path);
		while (i--)	// move back towards the start of the path string
			if (shell_state.path[i] == '/')	// find the last slash...
			{
				shell_state.path[i++] = '>';	// ... and terminate the string there
				shell_state.path[i] = 0;
				break;
			}
		// Update the current block pointer
		shell_state.block = shell_state.block[0].cb;	// "cb" of title entry is parent block pointer
	}

	// In all cases, transition to the prompt
	COMMAND_END
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
			if (shell_state.busy == 0) state = 1;
			break;
		case 1:		// print the title of the current block
			// Print straight to the output buffer
			sprintf (shell_state.output, "\r\n == %s ==", shell_state.block[idx++].label);
			shell_fp = shell_state_output;	// transition to output state
			state = 2;	// transition to local state 2.
			break;
		case 2:		// wait for communication interface to be ready
			if (shell_state.busy == 0) state = 3;
			break;
		case 3:		// print a block entry
			tag = (shell_state.block[idx].fp != 0) ? 'C' : '>';			// Start by determining the tag
			sprintf (shell_state.output, "\r\n %c %s", tag, shell_state.block[idx++].label);
			shell_fp = shell_state_output;	// transition to output state
			// The local state transition depends on the value of idx
			if (idx > (int) shell_state.block[0].fp)	// check against number of commands in the block
				state = 4;	// End of block reached, command will complete on next call
			else
				state = 2;	// Loop to wait for DMA completion and transfer next entry
			break;
		case 4:	// end of command
			COMMAND_END
			state = idx = 0;		// reset this command's state before leaving
			break;
	}

}


t_shell_block_entry shell_block[] =
{
		{ "", BLOCK_LEN 2, 0 },			// Title shouldn't be necessary, removing it to save space.
		{ "cd..", command_native_cddoubledot, 0},		// navigate towards the root
		{ "ls", command_native_list, 0}			// list commands in the current block
};

// Also declaring an empty system block to allow for compilation and operation even if the user doesn't declare their own
#ifndef SHELL_SYSTEM_BLOCK
t_shell_block_entry system_block[] =
{
		{ "", BLOCK_LEN 0, 0 }			// Zero length : the parser will skip this block
};
#endif

// Same for the application-specific command block
#ifndef SHELL_ROOT_BLOCK
t_shell_block_entry root_block[] =
{
		{"STM32", BLOCK_LEN 0, 0}	// Title block. Root, so no parent block. No function. Function pointer replaced by command count in the block
};
#endif


