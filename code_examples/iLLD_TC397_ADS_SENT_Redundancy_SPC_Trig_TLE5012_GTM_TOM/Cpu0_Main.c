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
 /*\title SENT Redundancy
 * \abstract Example of SENT SPC communication to interface TLE5012 GMR angle sensor.
 * \description This example demonstrates the Short PWM Code (SPC) protocol by interfacing an AURIX (TM) TC3xx Device 
 *              as master with a TLE5012 angle sensor as slave. The SPC pulse which needs to be sent by the master to 
 *              the slave is periodically triggered on each rising edge of a PWM signal generated by the Generic Timer
 *              Module (GTM) using the Timer Output Module (TOM).
 *
 * \name iLLD_TC397_ADS_SENT_Redundancy_SPC_Trig_TLE5012_GTM_TOM
 * \version V1.0.0
 * \board APPLICATION KIT TC3X7 V2.0, KIT_A2G_TC397_5V_TFT, TC39xXX_B-Step
 * \keywords SENT, SPC, GMR, GTM, TOM, TLE5012 sensor
 * \documents README.md
 * \lastUpdated 2023-09-08
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Bsp.h"
#include "TLE5012BD_SENT_SPC_Redundancy.h"

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
    
    /* Initial LED, SENT and GTM modules */
    initModules();

    while(1)
    {
        checkTle5012SENTredundancy();
        waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 50));    /* Wait 50 milliseconds */
    }
}
