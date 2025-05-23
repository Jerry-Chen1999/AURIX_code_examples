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
 /*\title System time via STM
 * \abstract The STM module is used to get the current system time (days:hours:minutes:seconds). The time is stored in a structure.
 * \description The System Timer (STM) module counts the number of ticks since the last Application Reset.
 *              This number is used to calculate the current system time in days, hours, minutes and seconds.
 *
 * \name STM_System_Time_1_KIT_TC375_LK
 * \version V2.0.0
 * \board AURIX TC375 lite Kit, KIT_A2G_TC375_LITE, TC37xTP_A-Step
 * \keywords AURIX, STM, STM_System_Time_1, System timer, Time
 * \documents README.MD
 * \documents https://www.infineon.com/aurix-expert-training/TC37A_iLLD_UM_1_0_1_17_0.chm
 * \lastUpdated 2024-04-23
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "STM_System_Time.h"
#include "Bsp.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define WAIT_TIME   1000        /* Number of milliseconds to wait               */

IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

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
    
    /* Initialize a time variable */
    Ifx_TickTime ticksFor1s = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, WAIT_TIME);

    while(1)
    {
        getTime();              /* Call the function which get the system time  */
        waitTime(ticksFor1s);   /* Wait one second                              */
    }
}
