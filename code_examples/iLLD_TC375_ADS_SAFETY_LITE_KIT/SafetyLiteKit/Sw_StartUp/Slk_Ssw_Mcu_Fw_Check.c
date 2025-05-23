/**********************************************************************************************************************
 * \file Slk_Ssw_Mcu_Fw_Check.c
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
#include "SafetyLiteKit/Slk_Main.h"
#include "SafetyLiteKit/Sw_StartUp/Slk_Ssw.h"
#include "SafetyLiteKit/Sw_StartUp/Slk_Ssw_Mcu_Fw_Check.h"
#include "SafetyLiteKit/Sw_StartUp/Slk_Ssw_Mcu_Fw_Check_Tables_TC375A.h"
#include "IfxMtu.h"
#include "IfxMtu_cfg.h"
#include "IfxSmu.h"
#include "IfxDmu_Reg.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*-------------------------------------------------Data Structures---------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
volatile Ifx_SMU_AG g_SlkSmuAlarmRegStatus[IFXSMU_NUM_ALARM_GROUPS];
McuFwCheckStatus g_mcuFwCheckStatus;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
boolean slkFwCheckSmuStmemLclcon(const FwCheckStruct *fwCheckTable, const int struct_size,
        const SlkResetType resetType, FwCheckVerificationStruct *fwCheckVerification);
IfxMtu_MbistSel slkFwCheckSsh(const SlkResetType resetType);
IfxMtu_MbistSel slkFwCheckCheckSshRegisters (const MemoryTestedStruct *sshTable, int tableSize);
boolean slkFwCheckEvaluateRamInit(uint16 memoryMask);
boolean slkFwCheckEvaluateLmuInit(uint16 memoryMask);
void slkFwCheckClearSmuAlarms(const FwCheckStruct *fwCheckTable, const int tableSize);
void slkFwCheckClearSSH(const SlkResetType resetType);
void slkFwCheckRetriggerCheck(const SlkResetType resetType);
void clearFaultStatusAndECCDetectionFSIRAM(void);
void slkFwCheckClearResetStatus(const SlkResetType resetType);
void slkFwCheckClearAppAndSysStatus(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/*
 * SM:SYS:MCU_FW_CHECK
 * */
void slkSswMcuFwCheck(void)
{
    g_mcuFwCheckStatus.mcuFwCheckSmu = FALSE;
    g_mcuFwCheckStatus.mcuFwCheckScuStem = FALSE;
    g_mcuFwCheckStatus.mcuFwCheckScuLclcon = FALSE;
    g_mcuFwCheckStatus.mcuFwCheckSsh = FALSE;

    /* Enable MTU module if not yet enabled
     * MTU is required especially for the check SSH because MTU controls SSH instances */
    boolean mtuWasEnabled = IfxMtu_isModuleEnabled();
    if (mtuWasEnabled == FALSE)
    {
        /* Enable MTU module */
        IfxMtu_enableModule();
    }
    /* Take a snapshot of the SMU alarm registers before executing the FW check */
    for(uint8 alarmReg = 0; alarmReg < IFXSMU_NUM_ALARM_GROUPS; alarmReg++)
    {
        g_SlkSmuAlarmRegStatus[alarmReg].U = MODULE_SMU.AG[alarmReg].U;
    }
    /* Increment the firmware check execution counter */
    /* Read SMU alarm register values and compare with expected ones (listed in Appendix A of the Safety Manual) */
    /* Note: depending on the device and reset type different register values are expected */

    if (TRUE
            == slkFwCheckSmuStmemLclcon(fwCheckSMUTC37A, fwCheckSMUTC37A_size, g_SlkStatus.resetCode.resetType,
                    fwCheckVerificationSMU))
    {
        g_mcuFwCheckStatus.mcuFwCheckSmu = TRUE;
    }

    if (TRUE
            == slkFwCheckSmuStmemLclcon(fwCheckSTMEMTC37A, fwCheckSTMEMTC37A_size,
                    g_SlkStatus.resetCode.resetType, fwCheckVerificationSTMEM))
    {
        g_mcuFwCheckStatus.mcuFwCheckScuStem = TRUE;
    }

    if (TRUE
            == slkFwCheckSmuStmemLclcon(fwCheckLCLCONTC37A, fwCheckLCLCONTC37A_size,
                    g_SlkStatus.resetCode.resetType, fwCheckVerificationLCLCON))
    {
        g_mcuFwCheckStatus.mcuFwCheckScuLclcon = TRUE;
    }

    if (IfxMtu_MbistSel_none == slkFwCheckSsh(g_SlkStatus.resetCode.resetType))
    {
        g_mcuFwCheckStatus.mcuFwCheckSsh = TRUE;
    }

    /* Verify if all checks have passed */
    if  ((g_mcuFwCheckStatus.mcuFwCheckSmu &&
          g_mcuFwCheckStatus.mcuFwCheckScuStem &&
          g_mcuFwCheckStatus.mcuFwCheckScuLclcon &&
          g_mcuFwCheckStatus.mcuFwCheckSsh))
    {
        g_SlkStatus.sswStatus.mcuFwcheckStatus = passed;

        /* If all registers and SMU alarm registers have reported the expected values .. */
        /* clear the content of the registers mentioned in the Appendix table */
        slkFwCheckClearSSH(g_SlkStatus.resetCode.resetType);
        /* clear the SMU alarms SMU_AG0..11 */
        slkFwCheckClearSmuAlarms(fwCheckSMUTC37A, fwCheckSMUTC37A_size);
        /* clear the corresponding reset status bits in RSTSTAT register */
        slkFwCheckClearResetStatus(g_SlkStatus.resetCode.resetType);
    }

    else
    {
        g_SswRunCount->mcuFwcheckFailCount++;
        g_SlkStatus.sswStatus.mcuFwcheckStatus = failed;
        /* If FW check has failed during its first execution trigger the check again */
        if(g_SswRunCount->mcuFwcheckFailCount < SLK_MCU_FW_CHECK_MAX_RUNS)
        {
            /* clear the corresponding reset status bits in RSTSTAT register */
             slkFwCheckClearResetStatus(g_SlkStatus.resetCode.resetType);
            /* Debugger issue : this line needs to be commented during debug state */
            slkFwCheckRetriggerCheck(g_SlkStatus.resetCode.resetType);
        }
    }

    /* Disable MTU module if it was disabled */
    if(mtuWasEnabled == FALSE)
    {
        /* Disable again */
        IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
        MTU_CLC.B.DISR = 1;
        IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    }

}

/* This function is comparing the actual register values of the registers listed in Safety Manual Appendix A with the expected ones.
 * The expected values are depending on the device type and also the reset type.
 * const FwCheckStruct *fwCheckTable => pointer to a structure of type FwCheckStruct. The table consists of a pointer to the address of the register which should be verified and accordingly the expected register values for each reset type.
 * const int tableSize => amount of entries in the fwCheckTable
 * const SlkResetType resetType => type of the reset
 * FwCheckVerificationStruct *fwCheckVerification => pointer to a structure where the test result will be written into. Can be observed via debugger in case the FW check has failed
 * */
boolean slkFwCheckSmuStmemLclcon(const FwCheckStruct *fwCheckTable, const int tableSize, const SlkResetType resetType, FwCheckVerificationStruct *fwCheckVerification)
{
    boolean fwcheckHasPassed = TRUE;
    uint32 registerValue;
    uint32 expectedRegisterValue;

    /* A pointer to the corresponding FwCheckRegisterCheckStruct inside the fwCheckTable.
     * The corresponding structure of interest depends on the reset type. */
    const FwCheckRegisterCheckStruct* ptr_register_check_strct;

    /* Iterate through the fwCheckTable */
    for(uint8 i = 0; i < tableSize; i++)
    {
        /* Set the pointer to the structure containing the expected register values */
        switch(resetType)
        {
            case SlkResetType_coldpoweron:
            case SlkResetType_lbist:
                ptr_register_check_strct = &fwCheckTable[i].coldPORST;
                break;
            case SlkResetType_warmpoweron:
                ptr_register_check_strct = &fwCheckTable[i].warmPORST;
                break;
            case SlkResetType_system:
                ptr_register_check_strct = &fwCheckTable[i].systemReset;
                break;
            case SlkResetType_application:
                ptr_register_check_strct = &fwCheckTable[i].applicationReset;
                break;
            default:
                __debug();
                break;
        }

        /* Read the value of the register which is checked during this iteration */
        registerValue          =   *(volatile uint32 *)fwCheckTable[i].regUnderTest;
        /* Mask the register value in case there are any exceptions. (Refer to the Appendix A of the Safety Manual) */
        registerValue          &=  ptr_register_check_strct->mask;
        /* Read the expected register value */
        expectedRegisterValue =   ptr_register_check_strct->expectedRegVal;

        /* Compare the register value with the expected value and write both the test result and the actual
         * register value into the verification structure. */
        fwCheckVerification[i].regVal = registerValue;
        fwCheckVerification[i].testHasPassed = (registerValue == expectedRegisterValue) ? TRUE : FALSE;
        /* Set fwcheckHasPassed to FALSE in case test has failed for any register during the iteration. */
        if(!fwCheckVerification[i].testHasPassed)
        {
            fwcheckHasPassed = FALSE;
        }
    }

    /* Return result after iteration and comparison of all registers listed in the fwCheckTable */
    return fwcheckHasPassed;
}

/*
 * Check the SSH registers and compare with the expected values depending on the reset type
 * */
IfxMtu_MbistSel slkFwCheckSsh(const SlkResetType resetType)
{
    IfxMtu_MbistSel fwcheckSshResult;
    switch(resetType)
    {
        case SlkResetType_coldpoweron:
        case SlkResetType_lbist:
            fwcheckSshResult = slkFwCheckCheckSshRegisters (coldPorstSSHTC37A,   coldPorstSSHTC37A_size);
            break;
        case SlkResetType_warmpoweron:
            fwcheckSshResult = slkFwCheckCheckSshRegisters (warmPorstSSHTC37A,   warmPorstSSHTC37A_size);
            break;
        case SlkResetType_system:
            fwcheckSshResult = slkFwCheckCheckSshRegisters (systemSSHTC37A,       systemSSHTC37A_size);
            break;
        case SlkResetType_application:
            fwcheckSshResult = slkFwCheckCheckSshRegisters (applicationSSHTC37A,  applicationSSHTC37A_size);
            break;
        default:
            __debug();
            break;
    }
    return fwcheckSshResult;
}

/*
 * Clear SSH register
 * */
void slkFwCheckClearSSH(const SlkResetType resetType)
{
    /* Get pointer to specific table and set variable about the size of this table */
    const MemoryTestedStruct* sshTable;
    int tableSize;

    switch(resetType)
    {
        case SlkResetType_coldpoweron:
        case SlkResetType_lbist:
                sshTable    = coldPorstSSHTC37A;
                tableSize  = coldPorstSSHTC37A_size;
            break;
        case SlkResetType_warmpoweron:
                sshTable    = warmPorstSSHTC37A;
                tableSize  = warmPorstSSHTC37A_size;
            break;
        case SlkResetType_system:
                sshTable    = systemSSHTC37A;
                tableSize  = systemSSHTC37A_size;
            break;
        case SlkResetType_application:
                sshTable    = applicationSSHTC37A;
                tableSize  = applicationSSHTC37A_size;
            break;
        default:
            __debug();
            break;
    }

    /* Now iterate through the table and clear the ECCD, FAULTSTS and ERRINFO register values */
    IfxMtu_MbistSel mbistSel;
    Ifx_MTU_MC *mc;
    int a;
    uint16 password = IfxScuWdt_getSafetyWatchdogPassword();

    for (a = 0; a < tableSize ; a++ )
    {
       mbistSel = sshTable[a].sshUnderTest;

       mc = &MODULE_MTU.MC[mbistSel];
       mc->ECCD.U = 0x0;

       IfxScuWdt_clearSafetyEndinit(password);

       mc->FAULTSTS.U = 0x0;
       mc->ECCD.B.TRC = 1;  /* This will clear ERRINFO */

       IfxScuWdt_setSafetyEndinit(password);
   }

}

/*
 * Clear reset application and system reset register
 * */
void slkFwCheckClearAppAndSysStatus(void)
{
    uint16         password;
    password = IfxScuWdt_getSafetyWatchdogPassword();
    IfxScuWdt_clearSafetyEndinitInline(password);

    MODULE_SCU.RSTCON.B.SW = 0;
    IfxScuWdt_setSafetyEndinit(password);
}

/*
 * Clear reset status
 * */
void slkFwCheckClearResetStatus(const SlkResetType resetType)
{
    switch(resetType)
    {
        case SlkResetType_coldpoweron:
        case SlkResetType_lbist:
            IfxScuRcu_clearColdResetStatus();
            break;
        case SlkResetType_warmpoweron:
            IfxScuRcu_clearColdResetStatus();
            break;
        case SlkResetType_system:
        case SlkResetType_application:
            slkFwCheckClearAppAndSysStatus();
            break;

        default:
            __debug();
            break;
    }
}

/*
 * This is the implementation of Erratum from Errata sheet
 * Erratum: [SMU_TC.H012]
 * The SMU alarms ALM7[1] and ALM7[0] are set intentionally after PORST and system reset and shall be
 * cleared by the application SW (cf. SM:SYS:MCU_FW_CHECK in Safety Manual) Also, in order to
 * clear the SMU alarms ALM7[1] and ALM7[0], it is necessary to clear the alarms within this MC40.
 * */
void clearFaultStatusAndECCDetectionFSIRAM(void)
{
    uint16 password = IfxScuWdt_getSafetyWatchdogPassword();
    uint16 *ptrEccd = (uint16 *)(0xF0063810);      /* MCi_ECCD and i = 40 */
    *ptrEccd = 0;

    IfxScuWdt_clearSafetyEndinit(password);
    uint16 *ptrFaultsts = (uint16 *)(0xF00638F0);  /* MCi_FAULTSTS and i = 40*/
    *ptrFaultsts = 0;
    IfxScuWdt_setSafetyEndinit(password);
}

/*
 * Check if Smu alarms were cleared
 * */
void slkFwCheckClearSmuAlarms(const FwCheckStruct *fwCheckTable, const int tableSize)
{
    uint16 passwd = IfxScuWdt_getSafetyWatchdogPassword();
    clearFaultStatusAndECCDetectionFSIRAM();
    IfxScuWdt_clearSafetyEndinit(passwd);

    /* Iterate through all SMU alarm status registers listed in the fwCheckTable and clear them */
    for(uint8 i = 0; i < tableSize; i++)
    {
        MODULE_SMU.CMD.U = IfxSmu_Command_alarmStatusClear;
        *(volatile uint32 *)fwCheckTable[i].regUnderTest = (uint32) 0xFFFFFFFF;
    }

    IfxScuWdt_setSafetyEndinit(passwd);
}

/*
 * Clear SSH and trigger a specific reset depending on incoming reset
 * */
void slkFwCheckRetriggerCheck(const SlkResetType resetType)
{
    /* Initiate the specific reset to trigger an FW check. Note: the reset should be the same type as it was before this FW check execution */
    switch(resetType)
    {
        case SlkResetType_coldpoweron:
        case SlkResetType_lbist:
            /* Trigger Cold PORST */
            /* No TLF for this board to trigger cold PORST */
            break;
        case SlkResetType_warmpoweron:
            slkFwCheckClearSSH(resetType);
            /* Trigger Warm PORST */
            slkTriggerWarmPorst();
            break;
        case SlkResetType_system:
            slkFwCheckClearSSH(resetType);
            slkTriggerSwReset(SlkResetType_system);
            break;
        case SlkResetType_application:
            slkFwCheckClearSSH(resetType);
            slkTriggerSwReset(SlkResetType_application);
            break;
        default:
            __debug();
            break;
    }
}

/*
 * This function is comparing the SSH registers of all RAMs with their expected values
 * */
IfxMtu_MbistSel slkFwCheckCheckSshRegisters(const MemoryTestedStruct* sshTable, int tableSize)
{
    IfxMtu_MbistSel mbistSel;
    Ifx_MTU_MC *mc;
    int a;
    volatile uint16 FAULTSTS_expected_value, ECCD_expected_value, ERRINFO_expected_value;

    /* Iterate through all entries in the sshTable */
    for (a = 0; a < tableSize ; a++ )
    {
        /* Get SSH which is tested during this iteration */
        mbistSel = sshTable[a].sshUnderTest;
        /* Get pointer to MC object of tested SSH */
        mc = &MODULE_MTU.MC[mbistSel];

        /* Evaluate which register values are expected for FAULTSTS, ECCD and ERRINFO registers. */
        /* Check which memory type it is and evaluate if the RAM is initialized or not. Depending
         * on this different FAULTSTS values are expected for CPU and LMU memories. */
        if  ( CPU_MEM_TYPE == sshTable[a].memoryType )
        {
            if (TRUE == slkFwCheckEvaluateRamInit(sshTable[a].InSelMask) )
            {
                FAULTSTS_expected_value = 0x9;
            }
            else
            {
                FAULTSTS_expected_value = 0x1;
            }
        }
        else if ( LMU_MEM_TYPE == sshTable[a].memoryType )
        {
            if (TRUE == slkFwCheckEvaluateLmuInit(sshTable[a].InSelMask) )
            {
                FAULTSTS_expected_value = 0x9;
            }
            else
            {
                FAULTSTS_expected_value = 0x1;
            }
        }
        else
        {
           FAULTSTS_expected_value = sshTable[a].sshRegistersDef.faultstsVal;
        }

           ECCD_expected_value     = sshTable[a].sshRegistersDef.eccdVal;
           ERRINFO_expected_value  = sshTable[a].sshRegistersDef.errinfoVal;

           /* Exception: If AURIX woke up from standby, overwrite expected values with the dedicated standby values. */
           if(g_SlkStatus.wakeupFromStandby)
           {
               FAULTSTS_expected_value = sshTable[a].sshRegistersStb.faultstsVal;
               ECCD_expected_value     = sshTable[a].sshRegistersStb.eccdVal;
               ERRINFO_expected_value  = sshTable[a].sshRegistersStb.errinfoVal;
           }
        {

            /* Finally compare register values of selected memory with the expected ones, if any mismatch is
             * detected return the name of the selected memory and stop the function execution. */
            if (    mc->ECCD.U          != ECCD_expected_value         ||
                    mc->FAULTSTS.U      != FAULTSTS_expected_value     ||
                    mc->ERRINFO[0].U    != ERRINFO_expected_value )
            {
               return (mbistSel);
            }
        }
        /* EXCEPTION TEST */
    }
    return (IfxMtu_MbistSel_none);
}

/*
 * Verify if CPU memory is initialized
 * */
boolean slkFwCheckEvaluateRamInit(uint16 memoryMask)
{
    if ( RAM_INIT_AT_COLD_WARM == DMU_HF_PROCONRAM.B.RAMIN ||
         RAM_INIT_AT_COLD_ONLY == DMU_HF_PROCONRAM.B.RAMIN)
    {
        if (((DMU_HF_PROCONRAM.B.RAMINSEL) & (uint16) (memoryMask)) == 0)
        {
            return (TRUE);
        }
        else
        {
            return (FALSE);
        }
    }
    else
    {
        return (FALSE);
    }
}

/*
 * Verify if LMU memory is initialized
 * */
boolean slkFwCheckEvaluateLmuInit(uint16 memoryMask)
{
    if ( RAM_INIT_AT_COLD_WARM == DMU_HF_PROCONRAM.B.RAMIN ||
         RAM_INIT_AT_COLD_ONLY == DMU_HF_PROCONRAM.B.RAMIN)
    {
        if (((DMU_HF_PROCONRAM.B.LMUINSEL) & (uint16) (memoryMask)) == 0)
        {
            return (TRUE);
        }
        else
        {
            return (FALSE);
        }
    }
    else
    {
        return (FALSE);
    }
}

/*
 * This function is to clear all SMU alarms
 * */
void slkClearAllSmuAlarms(void)
{
    slkFwCheckClearSmuAlarms(fwCheckSMUTC37A, fwCheckSMUTC37A_size);
}
