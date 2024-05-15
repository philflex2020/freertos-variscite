/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 
 ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major
 scp debug/hello_boot.elf root@192.168.86.34:/lib/firmware
 
 root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo stop > state
 root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo hello_boot.elf > firmware
 root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo start > state

[ 4718.324395] remoteproc remoteproc0: powering up imx-rproc
[ 4718.330296] remoteproc remoteproc0: Booting fw image hello_boot.elf, size 275692
[ 4718.337796] remoteproc remoteproc0: header-less resource table
[ 4718.343645] remoteproc remoteproc0: No resource table in elf
[ 4718.876177] remoteproc remoteproc0: remote processor imx-rproc is now up

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


void display_memory(unsigned long addr, int size)
{
    int i, j;
    unsigned char *ptr;

    for (i = 0; i < size; i++)
    {
        ptr = (unsigned char *)(addr + i * 16);
        PRINTF("0x%08lX: ", (unsigned long)ptr);

        for (j = 0; j < 16; j++)
        {
            PRINTF("%02X ", ptr[j]);
        }
        PRINTF("\r\n");
    }
}
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
    // Display the address of the main function
    PRINTF("Address of main: 0x%08lX\r\n", (unsigned long)main);

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
                // Display memory if address is >= 0x80000000
                if (addr >= 0x00000000)
                {
                    display_memory(addr, size);
                }
                else
                {
                    PRINTF("Address must be >= 0x00000000\r\n");
                }

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
