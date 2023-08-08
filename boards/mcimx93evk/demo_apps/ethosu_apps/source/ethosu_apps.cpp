/*
 * Copyright 2021-2022 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstring>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "board.h"
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"

#include "src.h"

#include "ethosu_driver.h"
#include "ethosu_core_interface.h"
#include "inference_process.hpp"
#include "conv2d_model.hpp"
//#include "depthwise_conv2d_model.hpp"
//#include "fullconnected_model.hpp"
//#include "relu_model.hpp"
//#include "softmax_model.hpp"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define ETHOSU_BASE_ADDRESS     0x4A900000
#define ETHOSU_IRQ		178

#ifndef TENSOR_ARENA_SIZE
#define TENSOR_ARENA_SIZE	(384 * 1024)
#endif

#define DDR_MEMORY_ADDRESS	0x8C000000
#define OCRAM_MEMORY_ADDRESS	0x204E0000

#define BYTES_TOPRINT		0x30000

#if (!defined(__ICCARM__))
using namespace std;
using namespace InferenceProcess;
#endif

__attribute__((section(".bss.tensor_arena_ocram"), aligned(16))) uint8_t inferenceProcessTensorArena[TENSOR_ARENA_SIZE];

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

using namespace EthosU;

struct ethosu_driver ethosu_drv;

void SRC_ML_Init(void)
{
    uint32_t src_tmp_val, mix_status_val;

#if 0
    /* sync with a-core SRC mlmix configuration */
    R32(REG_SRC_ML_AUTHEN_CTRL, src_tmp_val);
    W32(REG_SRC_ML_AUTHEN_CTRL, src_tmp_val | BIT(9));

    R32(REG_SRC_ML_PSW_ACK_CTRL_0, src_tmp_val);
    W32(REG_SRC_ML_PSW_ACK_CTRL_0, src_tmp_val & ~BIT(28));
    R32(REG_SRC_ML_PSW_ACK_CTRL_0, src_tmp_val);
    W32(REG_SRC_ML_PSW_ACK_CTRL_0, src_tmp_val | BIT(29));

    R32(REG_SRC_GLOBAL_SCR, src_tmp_val);
    W32(REG_SRC_GLOBAL_SCR, src_tmp_val | BIT(4));

    R32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val);
    W32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val | BIT(2));

    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val | BIT(31));
    R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    while(!(src_tmp_val & SRC_MIX_SLICE_FUNC_STAT_PSW_STAT)) {
        PRINTF("***\r\n");
	R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    }

    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val & ~BIT(31));
    R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    while((src_tmp_val & SRC_MIX_SLICE_FUNC_STAT_ISO_STAT) > 0) {
        PRINTF("---\r\n");
        R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    }

    R32(0x4a810000, src_tmp_val);
#else
    R32(REG_SRC_GLOBAL_SCR, src_tmp_val);
    W32(REG_SRC_GLOBAL_SCR, src_tmp_val | BIT(7));
    R32(REG_SRC_NIC_FUNC_STAT, mix_status_val);
    while((mix_status_val & SRC_MIX_SLICE_FUNC_STAT_RST_STAT) == 0) {
        R32(REG_SRC_NIC_FUNC_STAT, mix_status_val);
    }

    R32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val);
    W32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val | BIT(0) | BIT(2));
    R32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val);
    W32(REG_SRC_ML_MEM_MEM_CTRL, src_tmp_val & ~BIT(0));
    R32(REG_SRC_ML_MEM_MEM_STAT, src_tmp_val);
    while((src_tmp_val & SRC_MEM_MEM_STAT_PGEN_STAT) > 0) {
        R32(REG_SRC_ML_MEM_MEM_STAT, src_tmp_val);
    }

    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val | BIT(4));
    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val & ~BIT(4));
    R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    while((src_tmp_val & SRC_MIX_SLICE_FUNC_STAT_PSW_STAT) > 0) {
        R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    }

    R32(REG_SRC_GLOBAL_SCR, src_tmp_val);
    W32(REG_SRC_GLOBAL_SCR, src_tmp_val | BIT(4));
    R32(REG_SRC_ML_FUNC_STAT, mix_status_val);
    while((mix_status_val & SRC_MIX_SLICE_FUNC_STAT_RST_STAT) == 0) {
        R32(REG_SRC_ML_FUNC_STAT,mix_status_val);
    }

    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val | BIT(2));
    R32(REG_SRC_ML_SW_CTRL, src_tmp_val);
    W32(REG_SRC_ML_SW_CTRL, src_tmp_val & ~BIT(2));
    R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    while((src_tmp_val & SRC_MIX_SLICE_FUNC_STAT_ISO_STAT) > 0) {
        R32(REG_SRC_ML_FUNC_STAT, src_tmp_val);
    }
#endif
}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Init board hardware. */
    BOARD_InitHardware();

    /* Remove SRC initialization of mlmix for the current boot mode */
    // SRC_ML_Init();

    PRINTF("Initialize Arm Ethos-U\r\n");
    if (ethosu_init(&ethosu_drv, (void *)ETHOSU_BASE_ADDRESS, nullptr, 0, 0, 0)) {
        PRINTF("Failed to initialize Arm Ethos-U\n");
	return 1;
    }

    NVIC_SetVector((IRQn_Type)ETHOSU_IRQ, (uint32_t)&ethosu_irq_handler);
    NVIC_EnableIRQ((IRQn_Type)ETHOSU_IRQ);

    std::vector<InferenceProcess::DataPtr> ifm, ofm, expectedOutput;

    memcpy(reinterpret_cast<void *>(OCRAM_MEMORY_ADDRESS), reinterpret_cast<void *>(model_data), sizeof(model_data));
    InferenceProcess::DataPtr networkModel(reinterpret_cast<void *>(OCRAM_MEMORY_ADDRESS), sizeof(model_data));
    ifm.push_back(InferenceProcess::DataPtr(reinterpret_cast<void *>(input_data), sizeof(input_data)));

    std::vector<uint8_t> pmuEventConfig(ETHOSU_CORE_PMU_MAX);

    InferenceProcess::InferenceJob job("job", networkModel, ifm, ofm, expectedOutput, pmuEventConfig, 0,
				       &ethosu_drv, 0, nullptr, false);
    job.invalidate();

    InferenceProcess::InferenceProcess inferenceprocess(inferenceProcessTensorArena, TENSOR_ARENA_SIZE);
    bool failed = inferenceprocess.runJob(job);
    job.clean();

    if (failed == true)
        PRINTF("Inference status: failed\r\n");
    else
        PRINTF("Inference status: success\r\n");

    return 0;
}