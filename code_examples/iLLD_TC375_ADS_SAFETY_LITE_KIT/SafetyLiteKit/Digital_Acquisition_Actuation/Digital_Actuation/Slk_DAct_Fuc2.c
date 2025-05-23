/**********************************************************************************************************************
 * \file Slk_DAct_Fuc2.c
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


/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Digital_Acquisition_Actuation/Digital_Actuation/Slk_DAct_Fuc2.h"
#include "Digital_Acquisition_Actuation/Slk_DA_Global.h"
#include "Digital_Acquisition_Actuation/Slk_Tim_Clock_Monitor.h"
#include "SafetyLiteKit/Slk_Cfg.h"
#include "SafetyLiteKit/Slk_Main.h"
#include "SafetyLiteKit/Smu/SMU.h"
#include "IfxCpu_Irq.h"
#include "IfxGtm_Tom.h"
#include "IfxGtm_Tom_Pwm.h"
#include "IfxGtm_Tim_In.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
IFX_EXTERN IfxGtm_Tim_In            g_timPwmMissionHandler; /* Handler for TIM PWM mission configuration*/
IFX_EXTERN IfxGtm_Tom_Pwm_Driver    g_tomPwmMissionHandler; /* Handler for TOM PWM configuration */

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void initMissionGtmTomDactFuc2(void);
void initMonitorGtmTimDactFuc2(void);

/* Declarations of the ISR functions */
IFX_INTERRUPT(isrGtmTimPwmMonitorDActFuc2, CPU1_RUNNING_TASK, ISR_PRIORITY_GTM_TIM_MISSION_DACT_FUC2);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* ISR handler */
void isrGtmTimPwmMonitorDActFuc2(void)
{
    IfxCpu_enableInterrupts();

    IfxGtm_Tim_In_onIsr(&g_timPwmMissionHandler);

    /* Corner case not covered by IfxGtm_Tim_In_update(driver); --> overflow without NEWVAL notification */
    if(!g_timPwmMissionHandler.newData)
    {
        g_timPwmMissionHandler.overflowCnt = IfxGtm_Tim_Ch_isCntOverflowEvent(g_timPwmMissionHandler.channel);
        if (g_timPwmMissionHandler.overflowCnt )
        {
            softwareCoreAlarmTriggerSMU(SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
            g_timPwmMissionHandler.overflowCnt = FALSE;
        }
    }
    else
    {
        if(g_timPwmMissionHandler.dataCoherent == FALSE)
        {
            /* Duty and period values were not measured from the same period */
            softwareCoreAlarmTriggerSMU(SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
        }
        plausibilityCheckDactFuc2();
    }
}

/*
 * This function perform plausibility check
 * */
void plausibilityCheckDactFuc2(void)
{
    /* Process data */
    /* SM:TOM_TIM_MONITORING */
    /* Get pointers to CM0 and CM1 channel of the respective TOM which is used for the signal generation */
    Ifx_GTM_TOM_CH_CM0 *tomChCM0Reg = (Ifx_GTM_TOM_CH_CM0*) ((uint32) g_tomPwmMissionHandler.tom
            + (uint32) g_tomPwmMissionHandler.tomChannel * TOM_CHANNEL_OFFSET + TOM_CM0_REG_OFFSET);
    Ifx_GTM_TOM_CH_CM1 *tomChCM1Reg = (Ifx_GTM_TOM_CH_CM1*) ((uint32) g_tomPwmMissionHandler.tom
            + (uint32) g_tomPwmMissionHandler.tomChannel * TOM_CHANNEL_OFFSET + TOM_CM1_REG_OFFSET);

    /* Compare periodTick (PWM period) and pulseLengthTIck (PWM duty cycle) of the signal measured by TIM with the
     * signal generated by TOM */
    uint32 periodTickDifference =
            g_timPwmMissionHandler.periodTick > tomChCM0Reg->B.CM0 ?
                    g_timPwmMissionHandler.periodTick - tomChCM0Reg->B.CM0 :
                    tomChCM0Reg->B.CM0 - g_timPwmMissionHandler.periodTick;
    uint32 pulseLengthTickDifference =
            g_timPwmMissionHandler.pulseLengthTick > tomChCM1Reg->B.CM1 ?
                    g_timPwmMissionHandler.pulseLengthTick - tomChCM1Reg->B.CM1 :
                    tomChCM1Reg->B.CM1 - g_timPwmMissionHandler.pulseLengthTick;

    /* If difference is higher as the allowed tolerance trigger an SMU alarm */
    if(periodTickDifference > TICK_TOLERANCE || pulseLengthTickDifference > TICK_TOLERANCE)
    {
        /* trigger SMU alarm */
        softwareCoreAlarmTriggerSMU(SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
    }
    g_timPwmMissionHandler.periodTick = 0;
    g_timPwmMissionHandler.pulseLengthTick= 0;
}

/*
 * initialize TOM to generate PWM
 * */
void initMissionGtmTomDactFuc2(void)
{
    IfxGtm_Tom_Pwm_Config pwmOutputSignalConfig;
    IfxGtm_Tom_Pwm_initConfig(&pwmOutputSignalConfig, &MODULE_GTM); /* Initialize default parameters */

    /* Select the TOM */
    pwmOutputSignalConfig.tom = GTM_TOM_SAFE_PIN.tom;
    /* Select the channel */
    pwmOutputSignalConfig.tomChannel = GTM_TOM_SAFE_PIN.channel;
    /* Set the port pin as output */
    pwmOutputSignalConfig.pin.outputPin = &GTM_TOM_SAFE_PIN;
    /* Select the clock source*/
    pwmOutputSignalConfig.clock = DA_TOM_CLOCK_SOURCE;
    /* Enable synchronous update */
    pwmOutputSignalConfig.synchronousUpdateEnabled = TRUE;
    /* Set the timer period */
    pwmOutputSignalConfig.period = TOM_PWM_PERIOD;
    pwmOutputSignalConfig.dutyCycle = (uint32) (TOM_DUTY_CYCLE * 0.01 * pwmOutputSignalConfig.period);

    /* Initialize the GTM TOM */
    boolean success = IfxGtm_Tom_Pwm_init(&g_tomPwmMissionHandler, &pwmOutputSignalConfig);
    if(!success)
    {
        softwareCoreAlarmTriggerSMU(SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
        return;
    }

    /* Validate if configuration was written successfully to the registers */
    slkTomPwmConfigReadBack(&pwmOutputSignalConfig, SOFT_SMU_ALM_DIGITAL_ACQ_ACT);

    /* Start the PWM */
    IfxGtm_Tom_Pwm_start(&g_tomPwmMissionHandler, TRUE);
}

/*
 * Initialize TIM to capture PWM
 * */
void initMonitorGtmTimDactFuc2(void)
{
    IfxGtm_Tim_In_Config timPwmMeasMissionConfig;
    /* Initialize default parameters */
    IfxGtm_Tim_In_initConfig(&timPwmMeasMissionConfig, &MODULE_GTM);

    /* Configure input pin */
    timPwmMeasMissionConfig.filter.inputPin = &GTM_TIM_MISSION;

    /* Configure clocks */
    timPwmMeasMissionConfig.capture.clock = DA_TIM_CLOCK_SOURCE;
    timPwmMeasMissionConfig.timeout.clock = DA_TIM_CLOCK_SOURCE;

    /* ISR configuration */
    timPwmMeasMissionConfig.capture.irqOnNewVal = TRUE; /* Trigger interrupt on new value */
    timPwmMeasMissionConfig.capture.irqOnCntOverflow = TRUE; /* Trigger int on counter overflow */
    timPwmMeasMissionConfig.isrProvider = IfxCpu_Irq_getTos(CPU1_RUNNING_TASK);
    timPwmMeasMissionConfig.isrPriority = ISR_PRIORITY_GTM_TIM_MISSION_DACT_FUC2;

    /* Initialize the TIM */
    boolean success = IfxGtm_Tim_In_init(&g_timPwmMissionHandler, &timPwmMeasMissionConfig);
    if(!success)
    {
        softwareCoreAlarmTriggerSMU(SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
        return;
    }

    /* Validate if configuration was written successfully to the registers */
    slkTimInConfigReadback(&timPwmMeasMissionConfig, SOFT_SMU_ALM_DIGITAL_ACQ_ACT);
}
/*
 * Initial function for Digital Actuation FUC2
 * */
void initDActFuc2(void)
{
    resetDAconfiguration();

    /* Part 1: Initialize SM:TIM_CLOCK_MONITORING */
    initEclkMonitoring();

    /* Part 2: Configure GTM TOM for the output PWM signal */
    initMissionGtmTomDactFuc2();

    /* Part 3: Configure GTM TIM for measuring the generated PWM signal */
    initMonitorGtmTimDactFuc2();
}
