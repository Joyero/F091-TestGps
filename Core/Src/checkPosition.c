/*******************************************************************************
* Filename: checkPosition.c
* Developer: Jorge Yesid Rios Ortiz
*******************************************************************************/


#include <stdlib.h>
#include "checkPosition.h"
#include "cmsis_os.h"
#include "usart.h"
#include "stm32f0xx_it.h"
#include "WDT_Check.h"
#include "string.h"
#include "gps.h"

#define CHECK_POS_CHECK_DISTANCE       5000


#define CHECK_POS_BLINK_NO_CFG   1500
#define CHECK_POS_BLINK_20_MTS   WDT_CHECK_PERIOD_20
#define CHECK_POS_BLINK_50_MTS   WDT_CHECK_PERIOD_50
#define CHECK_POS_BLINK_100_MTS  WDT_CHECK_PERIOD_100
#define CHECK_POS_BLINK_250_MTS  WDT_CHECK_PERIOD_250
#define CHECK_POS_BLINK_500_MTS  WDT_CHECK_PERIOD_500
#define CHECK_POS_BLINK_FAR_AWAY WDT_CHECK_PERIOD


TaskHandle_t checkPosHandleTask = NULL;      //Task Handle
SemaphoreHandle_t checkPosSemaphore = NULL;  //task's Semaphore Handle
TimerHandle_t checkPosTimer = NULL;          //Timer Tx Handle

sCheckPosApp sCheckPos;


void checkPos_Task(void *pvParameters);
void checkPos_ResetVariables(void);
void checkPos_PrintHelp(void);
void checkPos_CheckDistance(void);
void checkPos_TimerCallback(TimerHandle_t xTimer);


void checkPos_InitFw(void)
{
  //BaseType_t xReturned;
  if(NULL == checkPosHandleTask)
  {
    xTaskCreate(checkPos_Task, // Function that implements the task
    "checkPos",                // Text name for the task
    250,                       // Stack size in words, not bytes
    (void*) 1,                 // Parameter passed into the task
    osPriorityNormal,          // Priority at which the task is created
    &checkPosHandleTask);      // Used to pass out the created task's handle
  }
}


void checkPos_Task(void *pvParameters)
{
  checkPos_ResetVariables();
  if(NULL == checkPosSemaphore)
  {
    checkPosSemaphore = xSemaphoreCreateBinary();
  }

  if(NULL == checkPosTimer)
  {
    checkPosTimer = xTimerCreate("TimerRxFromBle", CHECK_POS_CHECK_DISTANCE, pdTRUE,
        (void*) 0, checkPos_TimerCallback);
  }

  vTaskDelay(400);
  checkPos_ResetVariables();
  printf("check pos task ok\r\n");
  checkPos_PrintHelp();
  vTaskDelay(2000);
  if(checkPosTimer!=NULL)
  {
    xTimerStart(checkPosTimer, 5);
  }
  for (;;)
  {
    if(xSemaphoreTake(checkPosSemaphore, portMAX_DELAY) == pdTRUE)
    {
      if(true == sCheckPos.sWakeupReason.bTimer)
      {
        sCheckPos.sWakeupReason.bTimer = false;
        checkPos_CheckDistance();
      }
      if(true == sCheckPos.sWakeupReason.bHealthRequest)
      {
        sCheckPos.sWakeupReason.bHealthRequest = false;
        WDTCheck_HealthResponse(WDT_CHECK_TASK_CHECK_POS_CODE);
      }
    }
  }
}


void checkPos_ResetVariables(void)
{
  sCheckPos.dLatitude = 0;
  sCheckPos.dLongitude = 0;
  sCheckPos.dDistance = 0;
  sCheckPos.dEarthRad = CHECK_POS_EARTH_RAD_DEFAULT;
  sCheckPos.sWakeupReason.bTimer = false;
  sCheckPos.sDoCheck.Register = 0;
  sCheckPos.sWakeupReason.bHealthRequest = false;
  WDTCheck_Period(true, CHECK_POS_BLINK_NO_CFG, 0);
}


void checkPos_PrintHelp(void)
{
  printf("%s> How to config parameters:\r\n", CHECK_POS_PROMPT);
  printf("config latitude:\r\n");
  printf("   lat=<value>\r\n");
  printf("config longitude:\r\n");
  printf("   lon=<value>\r\n");
  printf("config earth radius[kms]: (default: %f)\r\n", CHECK_POS_EARTH_RAD_DEFAULT);
  printf("   rad=<value>\r\n");
  printf("get parameters\r\n");
  printf("   data\r\n");
  printf("  parameters: Point:[lat,log] Radius:<radius> Kms  <==Response\r\n");
}


void checkPos_CheckDistance(void)
{
  double dLatitudeDD  = gps_GetLatitudeDegreeDecimals();
  double dLongitudeDD = gps_GetLongitudeDegreeDecimals();
  sCheckPos.sDoCheck.cfg.GpsOk = gps_IsValidFrame();
  if (sCheckPos.sDoCheck.cfg.GpsOk == 1)
  {
  }
  if (sCheckPos.sDoCheck.cfg.GpsOk == 0)
  {
    printf("%s> GPS [no valid data from gps]\r\n", CHECK_POS_PROMPT);
    WDTCheck_Period(true, CHECK_POS_BLINK_FAR_AWAY, 0);
    return;
  }
  if (sCheckPos.sDoCheck.Register < 0x07)
  {
    printf("%s> GPS [%f,%f] configure parameters\r\n", CHECK_POS_PROMPT, dLatitudeDD, dLongitudeDD);
    return;
  }
  printf("%s> GPS:[%f,%f]", CHECK_POS_PROMPT, dLatitudeDD, dLongitudeDD);
  double difLatitude =  (dLatitudeDD  - sCheckPos.dLatitude)   *M_PI/180;
  double difLongitude = (dLongitudeDD - sCheckPos.dLongitude)  *M_PI/180;
  dLatitudeDD  *= M_PI/180;
  dLongitudeDD *= M_PI/180;
  sCheckPos.dDistance = pow(sin(difLatitude / 2), 2) +
                        pow(sin(difLongitude / 2), 2) * cos(dLatitudeDD*M_PI/180) * cos(sCheckPos.dLatitude*M_PI/180);
  sCheckPos.dDistance = 2 * sCheckPos.dEarthRad * asin(sqrt(sCheckPos.dDistance))*1000;
  printf(" TARGET:[%f,%f]  Distance:%.2f m %s\r\n",
									 sCheckPos.dLatitude, sCheckPos.dLongitude,
									 sCheckPos.dDistance,
									 (sCheckPos.dDistance<=100?" close to target!":""));
  if (sCheckPos.dDistance<=20)
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_20_MTS, 0);
  }
  else if (sCheckPos.dDistance<=50)
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_50_MTS, 0);
  }
  else if (sCheckPos.dDistance<=100)
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_100_MTS, 0);
  }
  else if (sCheckPos.dDistance<=250)
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_250_MTS, 0);
  }
  else if (sCheckPos.dDistance<=500)
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_500_MTS, 0);
  }
  else
  {
    WDTCheck_Period(true, CHECK_POS_BLINK_FAR_AWAY, 0);
  }
}


bool checkPos_SetLatitude(double dLat)
{
  if( (dLat >= -90) && (dLat <=  90) )
  {
    sCheckPos.sDoCheck.cfg.Lat = true;
    sCheckPos.dLatitude = dLat;
    return true;
  }
  return false;
}


double checkPos_GetLatitude(void)
{
  return sCheckPos.dLatitude;

}


bool checkPos_SetLongitude(double dLon)
{
  if( (dLon >= -180) && (dLon <= 180) )
  {
    sCheckPos.sDoCheck.cfg.Lon = true;
      sCheckPos.dLongitude = dLon;
      return true;
  }
  return false;
}


double checkPos_GetLongitude(void)
{
  return sCheckPos.dLongitude;
}


bool checkPos_SetEarthRadius(double dRad)
{
  if( (dRad >= 6356.8) && (dRad <=6378.1) )
  {
    sCheckPos.dEarthRad = dRad;
    return true;
  }
  return false;
}


double checkPos_GetEarthRadius(void)
{
  return sCheckPos.dEarthRad;
}


void checkPos_HealthRequest(void)
{
  if(NULL == checkPosHandleTask || NULL == checkPosSemaphore)
  {
    return;
  }
  sCheckPos.sWakeupReason.bHealthRequest = true;
  xSemaphoreGive(checkPosSemaphore);
}


void checkPos_TimerCallback(TimerHandle_t xTimer)
{
  if(NULL == checkPosHandleTask || NULL == checkPosSemaphore)
  {
    return;
  }
  sCheckPos.sWakeupReason.bTimer = true;
  xSemaphoreGive(checkPosSemaphore);
}
