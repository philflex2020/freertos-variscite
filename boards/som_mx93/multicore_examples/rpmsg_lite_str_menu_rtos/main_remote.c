/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// export ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major
// cd work/philflex/freertos-variscite/boards/som_mx93/multicore_examples/rpmsg_lite_str_menu_rtos/armgcc
// sh build_all.sh


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"

//#include "fsl_uart.h"
#include "rsc_table.h"

// build:  devtool build freertos-variscite
// deploy:  scp ./workspace/sources/freertos-variscite/boards/som_mx8mp/multicore_examples/rpmsg_lite_str_menu_rtos/armgcc/debug/rpmsg_lite_str_menu_rtos.bin root@192.168.86.28:/boot
// run : insmod /boot/imx_rpmsg_tty.ko
//        insmod /boot/rpmsg_sample.ko

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define RPMSG_LITE_SHMEM_BASE         (VDEV1_VRING_BASE)
#define RPMSG_LITE_LINK_ID            (RL_PLATFORM_IMX93_M33_A55_USER_LINK_ID)
//#define RPMSG_LITE_LINK_ID            (RL_PLATFORM_IMX8MP_M7_USER_LINK_ID)

// these are the two data channels called end points (ept)
#define RPMSG_LITE_NS_ANNOUNCE_STRING "rpmsg-virtual-tty-channel-1"
#define RPMSG_LITE_NS_DATA_STRING     "rpmsg-client-sample"

#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30)
#endif
#ifndef LOCAL_EPT_ADDR_DATA
#define LOCAL_EPT_ADDR_DATA (31)
#endif

// stack sizes 
#define APP_TASK_STACK_SIZE (512)
#define DATA_TASK_STACK_SIZE (1024)

#define MS_TO_TICKS(ms) ((ms) * configTICK_RATE_HZ / 1000)

/* Globals */
static char tx_msg[512];
static char app_buf[512]; /* Each RPMSG buffer can carry less than 512 payload */
static int counter = 0;

// Data Structures
// Define constants for data structure sizes
#define CELL_COUNT 110
#define TEMP_COUNT 32

// Data structure for the header
#define MODULE_COUNT 4
#define RACK_NUM 3

// rack num is given to us from an incoming message
typedef struct {
    bool running;
    int rx_message_count;
    int counter;
} MySys;

static MySys mySys;


// rack num is given to us from an incoming message
typedef struct {
    uint16_t sequence;
    uint8_t rack_num;
    uint8_t module_num;
    uint8_t data_type;
} DataHeader;

// Define the maximum payload size
#define MAX_PAYLOAD_SIZE (496 - sizeof(DataHeader))

// Data structure for the payload
typedef struct {
    DataHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} DataObject;

// this is a specific type of payload  for a module
//  Module Status
typedef struct {
    uint16_t state;
    uint16_t soh;
} ModuleStatus;

// Data structure for cell data
typedef struct {
    uint16_t voltage;
    uint16_t soc;
} CellData;

// Data structure for temperature data
typedef struct {
    uint16_t temperature;
} TempData;

// Data structure for cell and temperature payload
typedef struct {
    ModuleStatus status;
    CellData cells[CELL_COUNT];
    TempData temps[TEMP_COUNT];
} CellPayload;



DataObject unit_data[MODULE_COUNT];
CellPayload  *cell_data[MODULE_COUNT];
DataHeader    *cell_header[MODULE_COUNT];

static uint32_t seed = 12345; // Seed value for the PRNG

// void srand(uint32_t new_seed) {
//     seed = new_seed;
// }

// Function to generate a 16-bit random number
uint16_t rand16(void) {
    return (uint16_t)(rand() & 0xFFFF);
}

// uint16_t rand16(void) {
//     // LCG parameters
//     const uint32_t a = 1664525;
//     const uint32_t c = 1013904223;

//     // Update seed
//     seed = (a * seed + c);

//     // Return a 16-bit random number
//     return (uint16_t)(seed >> 16);
// }
void initData(int rack_num)
{
    for (int i = 0; i<MODULE_COUNT ; i++)
    {
        cell_header[i] = (DataHeader*)&unit_data[i];
        cell_header[i]->rack_num = rack_num;
        cell_header[i]->module_num = i;
        cell_header[i]->data_type  = 0;
        cell_data[i] = (CellPayload*)&unit_data[i].payload;
    }
}

void setDataSequence(uint16_t seq)
{
    for (int i = 0; i<MODULE_COUNT ; i++)
    {
        cell_header[i]->sequence = seq;
    }
}

void setCellVoltage(uint16_t module_num,uint16_t cell_num, int16_t voltage)
{
    cell_data[module_num]->cells[cell_num].voltage = voltage;
}

void setCellSoc(uint16_t module_num,uint16_t cell_num, int16_t soc)
{
    cell_data[module_num]->cells[cell_num].soc = soc;
}

// int main() {
//     // Create a data object
//     DataObject dataObj;

//     // Access the payload as cell payload
//     CellPayload *cellPayload = (CellPayload *)&dataObj.payload;

//     // Access cell data
//     cellPayload->cells[0].voltage = 1000;
//     cellPayload->cells[0].soc = 50;

//     // Access temperature data
//     cellPayload->temps[0].temperature = 25;

//     return 0;
// }

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t app_task_handle = NULL;
static TaskHandle_t data_task_handle = NULL;

static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
static volatile uint32_t remote_addr;


static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
static volatile rpmsg_queue_handle my_queue        = NULL;

static struct rpmsg_lite_endpoint *volatile my_data_ept = NULL;
static volatile rpmsg_queue_handle my_data_queue        = NULL;

void app_destroy_task(void)
{
    if (app_task_handle)
    {
        vTaskDelete(app_task_handle);
        app_task_handle = NULL;
    }

    if (data_task_handle)
    {
        vTaskDelete(data_task_handle);
        data_task_handle = NULL;
    }

    if (my_ept)
    {
        rpmsg_lite_destroy_ept(my_rpmsg, my_ept);
        my_ept = NULL;
    }

    if (my_queue)
    {
        rpmsg_queue_destroy(my_rpmsg, my_queue);
        my_queue = NULL;
    }

    if (my_rpmsg)
    {
        rpmsg_lite_deinit(my_rpmsg);
        my_rpmsg = NULL;
    }
}

bool running = false;

void data_task(void *param)
{
    void *tx_buf;
    void *rx_buf;
    uint32_t len;
    //int tx_size = 512;
    uint32_t size;
    int32_t result;
    volatile uint32_t data_remote_addr;
    bool first =  false;
    uint16_t seq = 0;

    PRINTF("\r\nRPMSG FreeRTOS RTOS API Demo Data Task v9.0 ...\r\n");
    initData(2); // this sets the rack number as 2 

    for (;;)
    {
//        result =
//           rpmsg_queue_recv_nocopy(my_rpmsg, my_data_queue, (uint32_t *)&data_remote_addr, (char **)&rx_buf, &len, RL_BLOCK);

        // Delay for 1000 ms
        vTaskDelay(pdMS_TO_TICKS(100));
        //vTaskDelay(MS_TO_TICKS(1000));
        // wait till we are running
        if (running) 
        {
            // we have to wait for the first data message before we can continue
            if (!first) 
            {
                result =
                    rpmsg_queue_recv_nocopy(my_rpmsg, my_data_queue, (uint32_t *)&data_remote_addr, (char **)&rx_buf, &len, RL_BLOCK);
                PRINTF("RPMSG Data Result... %d remote_addr %d len %d\n", result, data_remote_addr, len );
                PRINTF("RPMSG Data  %s \n", rx_buf );
                first = true;
                /* Release held RPMsg rx buffer */
                result = rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
                if (result != 0)
                {
                    assert(false);
                }
            }
            else
            {
                result =
                    rpmsg_queue_recv_nocopy(my_rpmsg, my_data_queue, (uint32_t *)&data_remote_addr, (char **)&rx_buf, &len, RL_DONT_BLOCK);
                if(result == 0)
                    PRINTF("RPMSG Data Result... %d remote_addr %d len %d\r\n", result, data_remote_addr, len );
                // PRINTF("RPMSG Data  %s \n", rx_buf );
                // first = true;
                // /* Release held RPMsg rx buffer */
                // result = rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
                // if (result != 0)
                // {
                //     assert(false);
                // }

            }
            counter++;
            if(counter%5 == 0)
            {
                PRINTF("\r\nRPMSG Tick... %d \n", counter);
                setDataSequence(seq++);
                for (int i = 0 ; i < MODULE_COUNT; i++)
                {
                    tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
                    assert(tx_buf);
                    /* Copy data RPMsg tx buffer */
                    memcpy(tx_buf, &unit_data[i], size);
                    result = rpmsg_lite_send_nocopy(my_rpmsg, my_data_ept, data_remote_addr, tx_buf, size);
                    //PRINTF("    RPMSG Send Result... %d  size %d \r\n", result, size);
                }
           }
        }
    }
}


// this is an example of a tp function to decode an incoming message

void decode_message(MySys *mySysp, char **rx_buf, int *rxlen, char **xtx_buf, int * xtxlen)
{
    /* Copy string from RPMsg rx buffer */
    assert(*rxlen < sizeof(app_buf));
    memcpy(app_buf, rx_buf, *rxlen);
    app_buf[*rxlen] = 0; /* End string by '\0' */


    if(mySysp->running)
        mySysp->counter++;

    if ((*rxlen == 2) && (app_buf[0] == 0xd) && (app_buf[1] == 0xa))
    PRINTF("Get New Line From Master Side\r\n");
    else
    {
        PRINTF("Get Message From Master Side : \"%s\" [len : %d]\r\n", app_buf, *rxlen);
        if (strncmp(app_buf, "start", strlen("start"))  == 0 )
        {
            if (!mySysp->running)
            {
                PRINTF(" starting \r\n");
                mySysp->running = true;
                snprintf(tx_msg, sizeof(tx_msg), "%s", "starting");
            }
            else
            {
                snprintf(tx_msg, sizeof(tx_msg), "%s", "already started");
            }

        }
        else if (strncmp(app_buf, "stop", strlen("stop"))  == 0 )
        {
            if (mySysp->running)
            {
                PRINTF(" stopping \r\n");
                mySysp->running = false;
                snprintf(tx_msg, sizeof(tx_msg), "%s", "stopping");
            }
            else
            {
                snprintf(tx_msg, sizeof(tx_msg), "%s", "already stopped");
            }
        }
        else if (strncmp(app_buf, "data", strlen("data"))  == 0 )
        {
            PRINTF(" get data  \r\n");
            snprintf(tx_msg, sizeof(tx_msg), "counter [%d]", mySysp->counter);
        }
        else
        {
            snprintf(tx_msg, sizeof(tx_msg), "%s unknown", app_buf);
        }
    }
    PRINTF("  Running [%s] Counter : %d\r\n", (running?"true":"false"), mySysp->counter);

    return;

}


void app_task(void *param)
{
    //volatile uint32_t remote_addr;
    void *rx_buf;
    uint32_t len;
    int32_t result;
    void *tx_buf;
    uint32_t size;

    /* Print the initial banner */
    PRINTF("\r\nRPMSG FreeRTOS RTOS API Demo Counter v9.0 ...\r\n");

#ifdef MCMGR_USED
    uint32_t startupData;

    /* Get the startup data */
    (void)MCMGR_GetStartupData(kMCMGR_Core1, &startupData);

    my_rpmsg = rpmsg_lite_remote_init((void *)startupData, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);

    /* Signal the other core we are ready */
    (void)MCMGR_SignalReady(kMCMGR_Core1);
#else
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#endif /* MCMGR_USED */

    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);

//#define RPMSG_LITE_NS_ANNOUNCE_STRING "rpmsg-virtual-tty-channel-1"
//#define RPMSG_LITE_NS_DATA_STRING     "rpmsg-client-sample"
// static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
// static volatile rpmsg_queue_handle my_queue        = NULL;

// static struct rpmsg_lite_endpoint *volatile my_data_ept = NULL;
// static volatile rpmsg_queue_handle my_data_queue        = NULL;

    my_queue = rpmsg_queue_create(my_rpmsg);
    my_ept   = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, my_queue);
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, RL_NS_CREATE);

    my_data_queue = rpmsg_queue_create(my_rpmsg);
    my_data_ept   = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR_DATA, rpmsg_queue_rx_cb, my_data_queue);
    (void)rpmsg_ns_announce(my_rpmsg, my_data_ept, RPMSG_LITE_NS_DATA_STRING, RL_NS_CREATE);


    PRINTF("Nameservice sent, ready for incoming messages...\r\n");

    for (;;)
    {
        /* Get RPMsg rx buffer with message */
        result =
            rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, (uint32_t *)&remote_addr, (char **)&rx_buf, &len, RL_BLOCK);
        //counter++;
        //if (result == -EAGAIN)
        //{
        //    vTaskDelay(MS_TO_TICKS(10));
        //    counter++;

	    // no message sleep for a bit
            //assert(false);
        //}
        //else 
        //rpmsg_lite_send_params_t send_param;
        // /* Send the message over RPMsg */
        // send_param.data = tx_buf;
        // send_param.size = strlen(message_with_counter) + 1; // +1 to include the null terminator
        // send_param.dst = RL_ADDR_ANY;
        // rpmsg_lite_send(my_rpmsg, &send_param);
        if ( result == 0)
	    {
          /* Copy string from RPMsg rx buffer */
          assert(len < sizeof(app_buf));
          memcpy(app_buf, rx_buf, len);
          app_buf[len] = 0; /* End string by '\0' */

          if(running)
             counter++;

          if ((len == 2) && (app_buf[0] == 0xd) && (app_buf[1] == 0xa))
            PRINTF("Get New Line From Master Side\r\n");
          else
          {
            PRINTF("Get Message From Master Side : \"%s\" [len : %d]\r\n", app_buf, len);
            if (strncmp(app_buf, "start", strlen("start"))  == 0 )
            {
               if (!running)
               {
                  PRINTF(" starting \r\n");
                  running = true;
 	          snprintf(tx_msg, sizeof(tx_msg), "%s", "starting");
               }
               else
               {
 	          snprintf(tx_msg, sizeof(tx_msg), "%s", "already started");
               }

            }
	        else if (strncmp(app_buf, "stop", strlen("stop"))  == 0 )
            {
               if (running)
               {
                  PRINTF(" stopping \r\n");
                  running = false;
 	              snprintf(tx_msg, sizeof(tx_msg), "%s", "stopping");
               }
               else
               {
                  snprintf(tx_msg, sizeof(tx_msg), "%s", "already stopped");
               }
            }
            else if (strncmp(app_buf, "data", strlen("data"))  == 0 )
            {
               PRINTF(" get data  \r\n");
 	           snprintf(tx_msg, sizeof(tx_msg), "counter [%d]", counter);
            }
            else
            {
 	            snprintf(tx_msg, sizeof(tx_msg), "%s unknown", app_buf);
            }
          }
          PRINTF("  Running [%s] Counter : %d\r\n", (running?"true":"false"), counter);

          // now we decode the message once we get this building

          /* Get tx buffer from RPMsg */
          tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
          assert(tx_buf);
          /* Copy string to RPMsg tx buffer */
          memcpy(tx_buf, tx_msg, strlen(tx_msg) + 1);
          /* Echo back received message with nocopy send */
          result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, remote_addr, tx_buf, strlen(tx_msg)+1);
          if (result != 0)
          {
            assert(false);
          }
          /* Release held RPMsg rx buffer */
          result = rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
          if (result != 0)
          {
            assert(false);
          }
        }
	else
	{
            PRINTF("   Got error  %d\r\n", result);
	    assert(false);
        }
    }
}

void app_create_task(void)
{
    if (app_task_handle == NULL &&
        xTaskCreate(app_task, "APP_TASK", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &app_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create application task\r\n");
        for (;;)
            ;
    }
}

void data_create_task(void)
{
    if (data_task_handle == NULL &&
        xTaskCreate(data_task, "DATA_TASK", DATA_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &data_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create data task\r\n");
        for (;;)
            ;
    }
}


/*!
 * @brief Main function
 */
int main(void)
{
    srand(0xDEADBEEF);


    /* Initialize standard SDK demo application pins */
    BOARD_InitBootPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    /* Initialize standard SDK demo application pins */
    /* M7 has its local cache and enabled by default,
     * need to set smart subsystems (0x28000000 ~ 0x3FFFFFFF)
     * non-cacheable before accessing this address region */
    //BOARD_InitMemory();

    /* Board specific RDC settings */
    //BOARD_RdcInit();

    //BOARD_InitBootPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();

    copyResourceTable();

    mySys.running = false;
    mySys.counter = 0;
    

#ifdef MCMGR_USED
    /* Initialize MCMGR before calling its API */
    (void)MCMGR_Init();
#endif /* MCMGR_USED */

    app_create_task();
    data_create_task();
    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;)
        ;
}
