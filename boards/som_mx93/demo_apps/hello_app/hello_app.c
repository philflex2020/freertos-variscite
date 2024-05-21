/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.

 ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major
 sh build_all.sh
 scp debug/hello_app.elf root@192.168.86.34:/lib/firmware
 scp debug/hello_app.bin root@192.168.86.34:/boot

 
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "board.h"

/*******************************************************************************
 * Definitions

 we'll try to load the elf file to this location

 blob_reserved: blob_reserved@87d80000 {
        	compatible = "shared-dma-pool";
        	reg = <0 0x87d80000 0 0x20000>;  // 128 KB
        	no-map;
    	};
******************************************************************************/


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
    char ch;

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("hello app.\r\n");
    // Display the address of the main function
    PRINTF("Address of main: 0x%08lX\r\n", (unsigned long)main);

    while (1)
    {
        ch = GETCHAR();
        PUTCHAR(ch);
    }
}
