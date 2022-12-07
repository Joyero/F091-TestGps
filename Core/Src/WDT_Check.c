/*******************************************************************************
* Filename: WDT_Check.c
* Developer(s): Jorge Yesid Rios Ortiz
*******************************************************************************/

#include "iwdg.h"
#include "WDT_Check.h"
#include "gps.h"
#include "shell.h"
#include "checkPosition.h"


IWDG_HandleTypeDef hiwdg;

/*local variables*/
TaskHandle_t WDTCheck_TaskHandle = NULL;
WDTCheck * pWdtCheck = NULL;

extern __IO uint32_t uwTick;

void WDTCheck_Task(void *pvParameters);


void WDTCheck_InitFW(void)
{
  xTaskCreate(WDTCheck_Task,         // Function that implements the task.
              "wdt",                 // Text name for the task.
              250,                   // Stack size in words, not bytes.
              (void*) 1,             // Parameter passed into the task.
              osPriorityNormal,      // Priority at which the task is created.
              &WDTCheck_TaskHandle); // Used to pass out the created task's handle.
}


void WDTCheck_Task(void *pvParameters)
{
  WDTCheck WdtCheck;
  pWdtCheck = &WdtCheck;

  WdtCheck.bUsePermanentValue = true;
  WdtCheck.u8TimesBeforeReturn = 0;
  WdtCheck.u16PeriodCurrent = WDT_CHECK_PERIOD;
  WdtCheck.u16PeriodTemp = 0;

  WdtCheck.sHealth.u16CounterProcess = 0;
  WdtCheck.sHealth.bCheckProcess = false;
  WdtCheck.sHealth.bLaunchProcess = false;
  WdtCheck.sHealth.sTask.Register = WDT_CHECK_WATCHED_TASKS;

  vTaskDelay(300);
  printf("WDT task Ok\r\n");
  #if 1==DEBUG_WDT_CHECK_ACTIVE
    uint8_t u8Taskfields = 0;
    uint16_t u16counterWdt = 0;
    MX_IWDG_Init();
  #endif
  for (;;)
  {
    if ( true==WdtCheck.bUsePermanentValue)
    {
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
      vTaskDelay(WdtCheck.u16PeriodCurrent);
      #if 1==DEBUG_WDT_CHECK_ACTIVE
        HAL_IWDG_Refresh(&hiwdg);
      #endif
    }
    else
    {
      do
      {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        vTaskDelay(WdtCheck.u16PeriodTemp);
        WdtCheck.u8TimesBeforeReturn--;
        #if 1==DEBUG_WDT_CHECK_ACTIVE
          HAL_IWDG_Refresh(&hiwdg);
        #endif
      }
      while (0<WdtCheck.u8TimesBeforeReturn);
      WdtCheck.bUsePermanentValue = true;
    }

    #if 1==DEBUG_WDT_CHECK_ACTIVE
      HAL_IWDG_Refresh(&hiwdg);
      WdtCheck.sHealth.u16CounterProcess++;
      if(WDT_CHECK_COUNTER == WdtCheck.sHealth.u16CounterProcess)
      {
        u8Taskfields = WDT_CHECK_WATCHED_TASKS;
        WdtCheck.sHealth.u8TaskToCheck = u8Taskfields;
        #if (1==WDT_CHECK_DEBUG)
          printf("%s> fields to check: %d <%04d>\r\n", WDT_CHECK_PROMPT, u8Taskfields, u16counterWdt);
        #endif
        for (char u8CntFields = 0; u8CntFields < 32; u8CntFields++)
        {
        if((u8Taskfields & 0x01) == 0x01)
        {
            WDTCheck_HealthRequest(u8CntFields);
        }
          u8Taskfields = u8Taskfields >> 1;
          vTaskDelay(50);
          HAL_IWDG_Refresh(&hiwdg);
        }
      }
      if(WDT_REQUEST_COUNTER == WdtCheck.sHealth.u16CounterProcess)
      {
        #if (1==WDT_CHECK_DEBUG)
          printf("%s> checked-responsed Tasks : %d-%d [%lu] <%04d>\r\n", WDT_CHECK_PROMPT, WdtCheck.sHealth.u8TaskToCheck, WdtCheck.sHealth.sTask.Register, uwTick, u16counterWdt++);
        #endif
        if(WdtCheck.sHealth.u8TaskToCheck != WdtCheck.sHealth.sTask.Register)
        {
          while(1)
          {
              vTaskDelay(50);
          }
        }
        WdtCheck.sHealth.sTask.Register = 0;
      }
      if(WDT_MAX_COUNTER <= WdtCheck.sHealth.u16CounterProcess)
      {
        WdtCheck.sHealth.u16CounterProcess = 0;
      }
    #endif
  }
}


void WDTCheck_Period(bool bIsPermanent, uint16_t u16Period, uint8_t u8Times)
{
  if (NULL==WDTCheck_TaskHandle)
  {
    return;
  }

  pWdtCheck->bUsePermanentValue = bIsPermanent;
  if (true == bIsPermanent)
  {
    pWdtCheck->u16PeriodCurrent = u16Period;
    pWdtCheck->u16PeriodTemp = 0;
    pWdtCheck->u8TimesBeforeReturn = 0;
  }
  else
  {
    pWdtCheck->u16PeriodTemp = u16Period;
    pWdtCheck->u8TimesBeforeReturn = u8Times;
  }
}


void WDTCheck_HealthRequest(char code)
{
  switch (code)
  {
    case WDT_CHECK_TASK_GPS_CODE:
    {
      gps_HealthRequest();
      break;
    }
    case WDT_CHECK_TASK_SHELL_CODE:
    {
      shell_HealthRequest();
      break;
    }
    case WDT_CHECK_TASK_CHECK_POS_CODE:
    {
      checkPos_HealthRequest();
      break;
    }

    default:
    {
      while(1);
    }
  }

}


void WDTCheck_HealthResponse(char code)
{
  HAL_IWDG_Refresh(&hiwdg);
  switch (code)
  {
    case WDT_CHECK_TASK_GPS_CODE:
    {
      pWdtCheck->sHealth.sTask.Task.t0 = 1;
      break;
    }
    case WDT_CHECK_TASK_SHELL_CODE:
    {
      pWdtCheck->sHealth.sTask.Task.t1 = 1;
      break;
    }
    case WDT_CHECK_TASK_CHECK_POS_CODE:
    {
      pWdtCheck->sHealth.sTask.Task.t2 = 1;
      break;
    }
    default:
    {
      while(1);
    }
  }
}


void WDTCheck_RestartCounter(void)
{
  pWdtCheck->sHealth.bCheckProcess = false;
  pWdtCheck->sHealth.bLaunchProcess = false;
  pWdtCheck->sHealth.u8TaskToCheck = 0;
  pWdtCheck->sHealth.sTask.Register = 0;
  pWdtCheck->sHealth.u16CounterProcess = 0;
}

void WDTCheck_RestartProcess(bool bIsFotActive)
{
  pWdtCheck->sHealth.bCheckProcess = false;
  pWdtCheck->sHealth.bLaunchProcess = false;
  pWdtCheck->sHealth.u8TaskToCheck = 0;
  pWdtCheck->sHealth.sTask.Register = 0;
  pWdtCheck->sHealth.u16CounterProcess = 0;
}
