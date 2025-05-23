/**********************************************************************************************************************
 * \file Cpu0_Main.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 * 
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of 
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 * 
 * Boost Software License - Version 1.0 - August 17th, 2003
 * 
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and 
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 * 
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all 
 * derivative works of the Software, unless such copies or derivative works are solely in the form of 
 * machine-executable object code generated by a source language processor.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN 
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 *********************************************************************************************************************/
 /*\title DMA transfer between memories
 * \abstract The DMA is used to transfer ten words (32-bit) of data from one memory location to another without any CPU load.
 * \description The transfer of data is triggered by SW. The source is the Data Scratch Pad SRAM of CPU0 (DSPR0) and
 *              the destination is the Distributed Local Memory Unit (DLMU RAM). At the end of the transactions,
 *              the data is verified by comparing the source and destination buffers. The success of the data transfer
 *              is signaled through the LED connected to pin 0 of port 13. Otherwise, the LED connected to pin 1 of
 *              port 13 is used. The same cycle is repeated each second.
 *
 * \name DMA_Mem_to_Mem_1_KIT_TC397_TFT
 * \version V2.0.0
 * \board APPLICATION KIT TC3X7 V2.0, KIT_A2G_TC397_5V_TFT, TC39xXX_B-Step
 * \keywords AURIX, DMA, DMA_Mem_to_Mem_1, memory, transfer
 * \documents README.MD
 * \documents https://www.infineon.com/aurix-expert-training/TC39B_iLLD_UM_1_0_1_17_0.chm
 * \lastUpdated 2024-02-22
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "DMA_Mem_to_Mem.h"
#include "Bsp.h"

/*********************************************************************************************************************/
/*-------------------------------------------------------Macros------------------------------------------------------*/
/*********************************************************************************************************************/
#define WAIT_TIME   1000U

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;        /* Provides sync mechanism */

/*********************************************************************************************************************/
/*---------------------------------------------- Function Prototypes ------------------------------------------------*/
/*********************************************************************************************************************/
static void wait_ms(uint32 ms);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
void core0_main(void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
    
    /* Initialize DMA module */
    initDMA();

    while(1)
    {
        /* Run DMA Example */
        runDMA();

        /* Wait one second before the next DMA Transaction */
        wait_ms(WAIT_TIME);
    }
}

/* Function to wait (milliseconds) */
static void wait_ms(uint32 ms)
{
    sint32 Fsys = IfxStm_getFrequency(&MODULE_STM0);
    Ifx_TickTime waitms = (Fsys / (1000 / ms));

    wait(waitms);
}
