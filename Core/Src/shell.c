/*******************************************************************************
* Filename: shell.c
* Developer: Jorge Yesid Rios Ortiz
*******************************************************************************/


#include <stdlib.h>
#include "shell.h"
#include "cmsis_os.h"
#include "usart.h"
#include "stm32f0xx_it.h"
#include "WDT_Check.h"
#include "string.h"
#include "checkPosition.h"

extern UART_HandleTypeDef huart3;
#define SHELL_UART        huart3

TaskHandle_t shellHandleTask = NULL;     //Task Handle
SemaphoreHandle_t shellSemaphore = NULL; //task's Semaphore Handle
QueueHandle_t shellRxQueue = NULL;       //Queue Rx Handle
TimerHandle_t shellRxTimer = NULL;       //Timer Rx Handle

sShellApp *sShell = NULL;   // Variable control data

void shell_InitHw(void);
void shell_Task(void *pvParameters);
void shell_ResetVariables(void);
void shell_ResetUart(bool bResetQueue);

void shell_TimerCallbackRxfromShell(TimerHandle_t xTimer);
void shell_ExtractLineFromQueue(void);
void shell_DecodeDataFromShell(void);

void shell_InitFw(void)
{
  if(NULL == shellHandleTask)
  {
    xTaskCreate(shell_Task, // Function that implements the task.
    "ble",                  // Text name for the task.
    500,                    // Stack size in words, not bytes.
    (void*) 1,              // Parameter passed into the task.
    osPriorityNormal,       // Priority at which the task is created.
    &shellHandleTask);      // Used to pass out the created task's handle.
  }
}


void shell_InitHw(void)
{
  //Enable IRQ
  HAL_NVIC_SetPriority(USART3_8_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(USART3_8_IRQn);
  __HAL_UART_ENABLE_IT(&SHELL_UART, UART_IT_RXNE);

}

void shell_Task(void *pvParameters)
{

  sShellApp sShellapp;   // Variable control data
  sShell = &sShellapp;   // Variable control data

  //Create Rx Gsm queue
  if(NULL == shellRxQueue)
  {
    shellRxQueue = xQueueCreate(SHELL_RX_BUFFER_SIZE, sizeof(uint8_t));
  }

  shell_ResetVariables();

  if(NULL == shellSemaphore)
  {
    shellSemaphore = xSemaphoreCreateBinary();
  }

  if(NULL == shellRxTimer)
  {
    shellRxTimer = xTimerCreate("TimerRxFromBle", SHELL_TIME_RX_FROM_BLE_DEFAULT, pdFALSE,
        (void*) 0, shell_TimerCallbackRxfromShell);
  }

  shell_InitHw();
  vTaskDelay(200);
  printf("SHELL task ok\r\n");

  for (;;)
  {
    if(xSemaphoreTake(shellSemaphore, portMAX_DELAY) == pdTRUE)
    {
      if(true == sShell->sWakeupReason.bRxFromBle)
      {
        sShell->sWakeupReason.bRxFromBle = false;
        shell_ExtractLineFromQueue();
        shell_DecodeDataFromShell();
      }
      if(true == sShell->sWakeupReason.bHealthRequest)
      {
        sShell->sWakeupReason.bHealthRequest = false;
        WDTCheck_HealthResponse(WDT_CHECK_TASK_SHELL_CODE);
      }
    }
  }
}

void shell_ResetVariables(void)
{
  sShell->sUart.bReceivingFrame = false;
  shell_ResetUart(true);
  sShell->sWakeupReason.bRxFromBle = false;
  sShell->sWakeupReason.bHealthRequest = false;
}

void shell_ResetUart(bool bResetQueue)
{
  uint16_t u16Size = SHELL_RX_BUFFER_SIZE;
  char *buffer = (char*) sShell->sUart.sRx.Buffer;
  sShell->sUart.bReceivingFrame = false;
  sShell->sUart.sRx.u16Pos = 0;

  if(NULL != buffer)
  {
    uint16_t i = 0;
    while ((0x00 != buffer[i]) || (u16Size > i))
    {
      buffer[i] = 0x00;
      i++;
    }
  }
  if(true == bResetQueue && shellRxQueue != NULL )
  {
    xQueueReset(shellRxQueue);
  }
}

void shell_ReceivedChar(uint8_t RxChar)
{
  if(NULL == shellHandleTask || NULL == shellSemaphore || NULL == shellRxTimer)
  {
    return;
  }

  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
  if(false == sShell->sUart.bReceivingFrame && (char) 0x00 != (char) RxChar)
  {
    sShell->sUart.bReceivingFrame = true;
  }

  if(true == sShell->sUart.bReceivingFrame)
  {

    if(pdTRUE == xQueueIsQueueFullFromISR(shellRxQueue))
    {
      sShell->sWakeupReason.bRxFromBle = true;
      sShell->sUart.bReceivingFrame = false;
      xSemaphoreGiveFromISR(shellSemaphore, &xHigherPriorityTaskWoken);
      xTimerStopFromISR(shellRxTimer, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
      xQueueSendFromISR(shellRxQueue, &RxChar, NULL);
      xTimerStartFromISR(shellRxTimer, &xHigherPriorityTaskWoken);
    }
  }
}


void shell_HealthRequest(void)
{
  if(NULL == shellHandleTask || NULL == shellSemaphore)
  {
    return;
  }
  sShell->sWakeupReason.bHealthRequest = true;
  xSemaphoreGive(shellSemaphore);
}


void shell_TimerCallbackRxfromShell(TimerHandle_t xTimer)
{
  if(NULL == shellHandleTask || NULL == shellSemaphore)
  {
    return;
  }
  sShell->sUart.bReceivingFrame = false;
  sShell->sWakeupReason.bRxFromBle = true;
  xSemaphoreGive(shellSemaphore);
}

void shell_ExtractLineFromQueue(void)
{
  uint8_t u8RxData = 0x00;
  bool bExitWhile = false;

  shell_ResetUart(false);
  if(0 < sShell->sUart.sRxTemp.u8Pos)
  {
    for (uint8_t i = 0; i < 4; i++)
    {
      if(0 != sShell->sUart.sRxTemp.Buffer[i])
      {
        sShell->sUart.sRx.Buffer[i] = sShell->sUart.sRxTemp.Buffer[i];   //Copy temp data into buffer
        sShell->sUart.sRx.u16Pos++;
        sShell->sUart.sRxTemp.Buffer[i] = 0x00;   //Reset temporal buffer
      }
    }
    sShell->sUart.sRxTemp.u8Pos = 0;
  }
    do
    {
      if( pdTRUE == xQueueReceive(shellRxQueue, &u8RxData, 20))
      {
        if ( '\r'!=u8RxData && '\n'!=u8RxData )
        {
          sShell->sUart.sRx.Buffer[sShell->sUart.sRx.u16Pos++] = u8RxData;
        }
        else if ( 0!=sShell->sUart.sRx.u16Pos )
        {
          bExitWhile = true;
        }
      }
      else
      {
        bExitWhile = true;
      }
      if (SHELL_RX_BUFFER_SIZE < sShell->sUart.sRx.u16Pos)
      {
        bExitWhile = true;
      }
    }
    while ( false == bExitWhile);
  if(2 >= uxQueueMessagesWaiting(shellRxQueue))
  {
    bExitWhile = false;
    do
    {
      if( pdTRUE == xQueueReceive(shellRxQueue, &u8RxData, 20))
      {
        if('\r' != u8RxData && '\n' != u8RxData)
        {
          sShell->sUart.sRxTemp.Buffer[sShell->sUart.sRxTemp.u8Pos++] = u8RxData;
        }
      }
      else
      {
        bExitWhile = true;
      }
      if(3 < sShell->sUart.sRxTemp.u8Pos)
      {
        bExitWhile = true;
      }
    }
    while ( false == bExitWhile);
  }
}


void shell_DecodeDataFromShell(void)
{
  double value = 0;
  bool bsetCmd = false;
  if (strncmp(sShell->sUart.sRx.Buffer, SHELL_CMD_LAT, SHELL_CMD_LAT_SIZE) == 0)
  {
    if ((sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] >=0x30 &&
         sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] <=0x39) ||
        (sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] == '-')   )
    {
      value = strtod((const char *)&sShell->sUart.sRx.Buffer[SHELL_CMD_LAT_SIZE], NULL);
      bsetCmd = checkPos_SetLatitude(value);
    }
    printf("%s> new Latitude: %f. %s\r\n",SHELL_PROMPT, value,
           (bsetCmd==true? "set ok":"ERROR!!") );
  }
  else if (strncmp(sShell->sUart.sRx.Buffer, SHELL_CMD_LON, SHELL_CMD_LON_SIZE) == 0)
  {
    if ((sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] >=0x30 &&
         sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] <=0x39) ||
        (sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] == '-')   )
    {
      value = strtod((const char *)&sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE], NULL);
      bsetCmd = checkPos_SetLongitude(value);
    }
    printf("%s> new Longitude: %f. %s\r\n",SHELL_PROMPT, value,
           (bsetCmd==true? "set ok":"ERROR!!") );
  }
  else if (strncmp(sShell->sUart.sRx.Buffer, SHELL_CMD_RAD, SHELL_CMD_RAD_SIZE) == 0)
  {
    if ((sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] >=0x30 &&
         sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE] <=0x39) )
    {
      value = strtod((const char *)&sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE], NULL);
      bsetCmd = checkPos_SetEarthRadius(value);
    }
    printf("%s> new radius: %f[kms]. set %s\r\n",SHELL_PROMPT, value,
           (bsetCmd==true? "set ok":"ERROR!!") );
  }
  else if (strncmp(sShell->sUart.sRx.Buffer, SHELL_CMD_GET_DATA, SHELL_CMD_GET_DATA_SIZE) == 0)
  {
    value = strtod((const char *)&sShell->sUart.sRx.Buffer[SHELL_CMD_LON_SIZE], NULL);
    bsetCmd = checkPos_SetEarthRadius(value);
    printf("%s> Point:[%f %f] Radius:%f Kms\r\n",SHELL_PROMPT, checkPos_GetLatitude(),
                                 checkPos_GetLongitude(), checkPos_GetEarthRadius());
  }
  else
  {
    printf("%s> ERROR\r\n",SHELL_PROMPT);
  }
}

