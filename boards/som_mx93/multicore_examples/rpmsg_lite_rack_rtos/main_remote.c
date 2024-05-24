/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// export ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major
// cd work/philflex/freertos-variscite/boards/som_mx93/multicore_examples/rpmsg_lite_sample_rtos/armgcc
// sh build_all.sh
// scp debug/rpmsg_lite_rack_rtos.elf root@192.168.86.34:/lib/firmware
// on the remote root@192.168.86.34
// cd /sys/class/remoteproc/remoteproc0
// echo rpmsg_lite_sample_rtos.elf > firmware
// echo start > state

// insmod /lib/modules/6.1.36-imx93+ge6ac294d4629/kernel/drivers/rpmsg/imx_rpmsg_tty.ko
// insmod /boot/rpmsg_sample_tcp.ko
// echo -n "start" > /dev/ttyRPMSG30
// echo -n "data" > /dev/ttyRPMSG30
// dmesg
// rmmod rpmsg_sample_tcp
// insmod /boot/rpmsg_sample_tcp.ko
// ls /dev


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
#include "bms_system_rtos.h"

//#include "fsl_uart.h"
#include "rsc_table.h"


//(unsigned long)buf_addr = (unsigned long)

//#define BUFF_ADDR 0x87d80000


// build:  in yocto devtool build freertos-variscite

// build out of yocto
// ARMGCC_DIR=
//cd ~/work/philflex/freertos-variscite/boards/som_mx93/multicore_examples/rpmsg_lite_sample_rtos/armgcc/

//export ARMGCC_DIR=/home/phil/work/var-mcuexpresso/gcc-arm-none-eabi-10-2020-q4-major
// sh build_all.sh
//scp debug/rpmsg_lite_sample_rtos.elf root@192.168.86.34:/lib/firmware
//root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo stop > state
//root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo  rpmsg_lite_rack_rtos.elf > firmware
//root@imx93-var-som:/sys/class/remoteproc/remoteproc0# echo start > state
// run : insmod /boot/imx_rpmsg_tty.ko
//        insmod /boot/rpmsg_sample.ko
// new target
// run : insmod /boot/rpmsg_rack_control.ko
//        insmod /boot/rpmsg_rack_data.ko

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define RPMSG_LITE_SHMEM_BASE         (VDEV1_VRING_BASE)
#define RPMSG_LITE_LINK_ID            (RL_PLATFORM_IMX93_M33_A55_USER_LINK_ID)
//#define RPMSG_LITE_LINK_ID            (RL_PLATFORM_IMX8MP_M7_USER_LINK_ID)

// these are the two data channels called end points (ept)
#define RPMSG_LITE_NS_CONTROL_STRING "rpmsg-rack-control"
#define RPMSG_LITE_NS_DATA_STRING    "rpmsg-rack-data"

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


//import bms_system_rtos.h

// Data Structures
// Define constants for data structure sizes
// #define CELL_COUNT 110
// #define TEMP_COUNT 32

// // Data structure for the header
// #define MODULE_COUNT 4
// #define RACK_NUM 3

// rack num is given to us from an incoming message
typedef struct {
    bool running;
    bool open;
    int rx_message_count;
    int counter;
    int charge;
    int discharge;

} MySys;

static MySys mySys;

//we send 
                //   sequence counter
                //        data type
                //        rack_id
                //        data base
                //        buff_num ( used to be seq)
                //        offset into data area
                //        data  size

// rack num is given to us from an incoming message
typedef struct {
    uint32_t sequence;
    uint32_t data_type;
    uint32_t rack_id;
    uint32_t data_base;
    uint32_t buff_num;
    uint32_t offset;
    uint32_t data_size;
} DataHeader;

// Define the maximum payload size
#define MAX_PAYLOAD_SIZE (496 - sizeof(DataHeader))

// Data structure for the payload
typedef struct {
    DataHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} DataObject;




DataObject unit_data[MODULE_COUNT];
//CellPayload  *cell_data[MODULE_COUNT];
DataHeader    *cell_header[MODULE_COUNT];

//static uint32_t seed = 12345; // Seed value for the PRNG

// void srand(uint32_t new_seed) {
//     seed = new_seed;
// }

// Function to generate a 16-bit random number
// uint16_t rand16(int val) {
//     return (uint16_t)(rand() & 0xFFFF);
// }
// Function to generate a 16-bit random number within the range 0 to val
uint16_t rand16(int val) {
    return (uint16_t)(rand() % (val + 1));
}

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

void update_memory(unsigned long addr, int offset , int16_t value)
{
    //int i, j;
    int16_t *ptr;

    ptr = (int16_t *)(addr + offset);
    *ptr =  value;
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

// OK we pick up blob shared memory at BUFF_ADDR
//Ths points to an array  of 4 Module Items  ( lets hope its big enough)
// Data structure for Module payload
// typedef struct {
//     int message_id;
//     int rack_id;
//     int module_id;
//     int cell_id;
//     ModuleStatus status;
//     CellData cells[CELL_COUNT];
//     TempData temps[TEMP_COUNT];
// } ModulePayload;

    	// blob_reserved: blob_reserved@87d80000 {
        // 	compatible = "shared-dma-pool";
        // 	reg = <0 0x87d80000 0 0x40000>;  // 256 KB 262144 bytes
        // 	no-map;
    	// };

#define BUFF_COUNT 16

RackPayload *rack_payloads = (RackPayload *)BUFF_ADDR;


void init_module_payload(ModulePayload* payload, int rack_id, int module_id)
{
    payload->message_id = 0;
    payload->rack_id = rack_id;
    payload->module_id = module_id;
    payload->cell_id = -1; // we are getting the whole cell data
    for (int i = 0 ; i < CELL_COUNT; i++) {
        payload->cells[i].voltage = 321*100;
        payload->cells[i].soc = 100*100;

    }
    for (int i = 0 ; i < TEMP_COUNT; i++) {
        payload->temps[i].temperature = 25*1000;
    }

}

void process_module_payload(ModulePayload* payload, int rack_id, int module_id)
{

    int rand_val = 256;

    // Generate a random fluctuation between 0 and 1 (scaled from 0 to 100 and divided by 100.0)
    int16_t random_fluctuation; // = rand16(rand_val);
    int16_t new_voltage;
    payload->seq_id = 666;

    payload->message_id = 0;
    payload->rack_id = rack_id;
    payload->module_id = module_id;
    payload->cell_id = -1; // we are getting the whole cell data
    for (int i = 0 ; i < CELL_COUNT; i++) {
        random_fluctuation = rand16(rand_val);
        new_voltage = payload->cells[i].voltage - rand_val +  random_fluctuation; // Ensure proper scaling and rounding
        payload->cells[i].voltage = new_voltage;
        payload->cells[i].soc = 100*100;
    //     if(i == 0)
    //     {
    //         payload->cells[i].voltage = 12300;
    //         payload->cells[i].soc = 456;
    //     }
    //    if(i == 1)
    //     {
    //         payload->cells[i].voltage = 45600;
    //         payload->cells[i].soc = 789;
    //     }

    }
    for (int i = 0 ; i < TEMP_COUNT; i++) {
        payload->temps[i].temperature = 25*1000;
    }

}

void init_rack_payload(RackPayload* payload, int idx, int rack_id)
{
    payload->seq_id = 0x666 + idx;
    payload->message_id = idx;
    payload->rack_id = rack_id;
    //payload->module_id = module_id;
    //payload->cell_id = -1; // we are getting the whole cell data
    for (int i = 0 ; i < MODULE_COUNT; i++) {
        init_module_payload(&payload->modules[i], rack_id, i);
    }
} 

void process_rack_payload(RackPayload* payload, int idx, int rack_id)
{
    payload->seq_id = 0x555 + idx;
    payload->message_id = idx;
    payload->rack_id = rack_id;
    //payload->module_id = module_id;
    //payload->cell_id = -1; // we are getting the whole cell data
    for (int i = 0 ; i < MODULE_COUNT; i++) {
        process_module_payload(&payload->modules[i], rack_id, i);
    }
} 

// use BUFF_COUNT
// send this in the message  &rack_payloads[i]
// each lump is , currently , 3144 bytes longnthe rpmsg_rack_data module has to deal with this.

// RPMSG FreeRTOS setting up rack payload at 87D80000 ...
// RPMSG FreeRTOS setting up rack payload at 87D80C48 ...
// RPMSG FreeRTOS setting up rack payload at 87D81890 ...
// RPMSG FreeRTOS setting up rack payload at 87D824D8 ...
// RPMSG FreeRTOS setting up rack payload at 87D83120 ...
// RPMSG FreeRTOS setting up rack payload at 87D83D68 ...
// RPMSG FreeRTOS setting up rack payload at 87D849B0 ...
// RPMSG FreeRTOS setting up rack payload at 87D855F8 ...
// RPMSG FreeRTOS setting up rack payload at 87D86240 ...
// RPMSG FreeRTOS setting up rack payload at 87D86E88 ...
// RPMSG FreeRTOS setting up rack payload at 87D87AD0 ...
// RPMSG FreeRTOS setting up rack payload at 87D88718 ...
// RPMSG FreeRTOS setting up rack payload at 87D89360 ...
// RPMSG FreeRTOS setting up rack payload at 87D89FA8 ...
// RPMSG FreeRTOS setting up rack payload at 87D8ABF0 ...
// RPMSG FreeRTOS setting up rack payload at 87D8B838 ...

// this just configures one data area we really need a series of these buffers
void initData(int rack_num)
{
    //int rack_id = 2; //get this from the hello message
    for (int i = 0; i<BUFF_COUNT ; i++)
    {
        PRINTF("\r\nRPMSG FreeRTOS setting up rack payload at %p ...",&rack_payloads[i]);
        init_rack_payload(&rack_payloads[i],i, rack_num);
    }
    PRINTF("\r\n");
}

void setDataSequence(uint16_t seq)
{
    for (int i = 0; i<MODULE_COUNT ; i++)
    {
        // cell_header[i]->sequence = seq;
    }
}

// // Function to set cell voltage with random fluctuation
// void setCellVoltage(uint16_t module_num, uint16_t cell_num, int16_t voltage) {
//     // Ensure the module_num and cell_num are within bounds (add bounds checking if necessary)
//     int rand_val = 256;
//     // Generate a random fluctuation between 0 and 1 (scaled from 0 to 100 and divided by 100.0)
//     int16_t random_fluctuation = rand16(rand_val);
    
//     // Calculate the new voltage with the random fluctuation
//     int16_t new_voltage = voltage - rand_val +  random_fluctuation; // Ensure proper scaling and rounding
    
//     // Set the voltage for the specified cell
//     cell_data[module_num]->cells[cell_num].voltage = new_voltage;
// }
// void setCellVoltage(uint16_t module_num,uint16_t cell_num, int16_t voltage)
// {
//     cell_data[module_num]->cells[cell_num].voltage = uint16_t(voltage + rand16(100)/100.0);
// }

// void setCellSoc(uint16_t module_num,uint16_t cell_num, int16_t soc)
// {
//     cell_data[module_num]->cells[cell_num].soc = soc;
// }

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
    int32_t *tx_regs;
    uint32_t len;
    //int tx_size = 512;
    uint32_t size;
    int32_t result;
    volatile uint32_t data_remote_addr;
    bool first =  false;
    uint16_t seq = 0;
    int buff_num = 0;
    int rack_id = 2;


    PRINTF("RPMSG FreeRTOS RTOS API Data Task v1.0 ...\r\n");
    initData(rack_id); // this sets the rack number as 2 

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
                // just in case 
                if ( len > 0)
                {
                    ((char *)rx_buf)[len -1] = 0;
                    PRINTF("RPMSG Data  %s \n", rx_buf );
                }
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

                // set up the message data in &rack_payloads[buff_num] where buff_num goes from 0 to BUFF_COUNT

                RackPayload* rack_payload = &rack_payloads[0];
                rack_payload->seq_id = 0x9999;
                rack_payload = &rack_payloads[buff_num];
                process_rack_payload(rack_payload, seq, rack_id);
                


                // wrap this all up
                tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
                assert(tx_buf);
                
                //update_memory(BUFF_ADDR, 0, seq);

                // now populate the buffers normally
                tx_regs = (int32_t *)tx_buf;
                //we send 
                //   rpmsg_rack_data  module must match
                //   sequence counter
                //        data type
                //        rack_id
                //        data base
                //        buff_num ( used to be seq)
                //        offset into data area
                //        data  size


                tx_regs[0] = (int32_t)counter;
                tx_regs[1] = 2;        // data_type  rack data
                tx_regs[2] = rack_id;        // rack_id
                tx_regs[3] = (int32_t)(&rack_payloads[0]); // we need the offfset from the start of shared memory
                tx_regs[4] = buff_num;
                tx_regs[5] = (int32_t)(buff_num * sizeof(RackPayload)); // we need the offfset from the start of shared memory
                tx_regs[6] = (int32_t)(sizeof(RackPayload)); // we need the offfset from the start of shared memory
                    
                buff_num = (buff_num+1) % BUFF_COUNT;
                result = rpmsg_lite_send_nocopy(my_rpmsg, my_data_ept, data_remote_addr, tx_buf, size);
                //PRINTF("    RPMSG Send Result... %d  size %d \r\n", result, size);
                
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
        else if (strncmp(app_buf, "close", strlen("close"))  == 0 )
        {
            if (mySysp->open)
            {
                PRINTF(" closing \r\n");
                mySysp->open = false;
                snprintf(tx_msg, sizeof(tx_msg), "%s", "closing");
            }
            else
            {
                snprintf(tx_msg, sizeof(tx_msg), "%s", "already closed");
            }
        }
        else if (strncmp(app_buf, "open", strlen("open"))  == 0 )
        {
            if (!mySysp->open)
            {
                PRINTF(" opening \r\n");
                mySysp->open = true;
                snprintf(tx_msg, sizeof(tx_msg), "%s", "opening");
            }
            else
            {
                snprintf(tx_msg, sizeof(tx_msg), "%s", "already open");
            }
        }
        else if (strncmp(app_buf, "charge", strlen("charge"))  == 0 )
        {
            // todo decode "charge 12345" any number of digits 
            int charge_val = 1234;
            if (mySysp->charge == 0)
            {
                PRINTF(" starting charge \r\n");
            }
            mySysp->charge = charge_val;
            mySysp->discharge = 0;
            snprintf(tx_msg, sizeof(tx_msg), "%s %d", "charging", mySysp->charge);

        }
        else if (strncmp(app_buf, "discharge", strlen("discharge"))  == 0 )
        {
            // todo decode "charge 12345" any number of digits 
            int discharge_val = 1234;
            if (mySysp->discharge == 0)
            {
                PRINTF(" starting discharge \r\n");
            }
            mySysp->discharge = discharge_val;
            mySysp->charge = 0;
            snprintf(tx_msg, sizeof(tx_msg), "%s %d", "discharging", mySysp->discharge);

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
    PRINTF("\r\nRPMSG FreeRTOS RTOS API Prototype v1.0 ...\r\n");

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

//#define RPMSG_LITE_NS_CONTROL_STRING "rpmsg-virtual-tty-channel-1"
//#define RPMSG_LITE_NS_DATA_STRING     "rpmsg-client-sample"
// static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
// static volatile rpmsg_queue_handle my_queue        = NULL;

// static struct rpmsg_lite_endpoint *volatile my_data_ept = NULL;
// static volatile rpmsg_queue_handle my_data_queue        = NULL;

    my_queue = rpmsg_queue_create(my_rpmsg);
    my_ept   = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, my_queue);
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_CONTROL_STRING, RL_NS_CREATE);

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
    srand(0xBEADBEEF);


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
