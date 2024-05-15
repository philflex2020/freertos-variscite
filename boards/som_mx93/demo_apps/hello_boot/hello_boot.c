/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 
 ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major

 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "board.h"
#include <string.h>
#include <stdlib.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
#define BUFFER_SIZE 64
char buffer[BUFFER_SIZE];
int buf_idx;

/*!
 * @brief Main function
 */
int main(void)
{
    char ch;
    buf_idx = 0;
    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("hello boot.\r\n");

    while (1)
    {
        ch = GETCHAR();
        if (ch == '\r' || ch == '\n')  // Enter key pressed
        {
            buffer[buf_idx] = '\0';  // Null-terminate the string
            PRINTF("\r\nBuffer contents: [%s]\r\n", buffer);
            if (strncmp(buffer, "mem", 3) == 0)
            {
                char *token;
                unsigned long addr = 0;
                int size = 0;

                // Tokenize the input
                token = strtok(buffer, " ");
                token = strtok(NULL, " ");  // Get the address
                if (token)
                {
                    addr = strtoul(token, NULL, 16);  // Convert hex address to unsigned long
                }

                token = strtok(NULL, " ");  // Get the size
                if (token)
                {
                    size = atoi(token);  // Convert size to integer
                }

                PRINTF("Decoded command: mem 0x%08lX %d\r\n", addr, size);
            }
            buf_idx = 0;  // Reset index for next input
        }
        else if (buf_idx < BUFFER_SIZE - 1)
        {
            buffer[buf_idx++] = ch;
            PUTCHAR(ch);  // Echo the character back
        }
    }
    return 0;
}
