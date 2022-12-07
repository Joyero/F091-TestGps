/*******************************************************************************
* Filename: WDT_Check.c
* Developer(s): Jorge Yesid Rios Ortiz
*******************************************************************************/

#ifndef __WDT_CHECK_H
#define __WDT_CHECK_H


#include "cmsis_os.h"
#include "stdio.h"
#include "stdbool.h"
#include "main.h"


#define DEBUG_WDT_CHECK_ACTIVE  0  //0:Disable WDT,  1:Enable WDT
#define WDT_CHECK_DEBUG         1  //0:Disable WDT,  1:Enable WDT
#define WDT_CHECK_PROMPT    "WDT"

#define WDT_CHECK_PERIOD_CMD_OK                   50
#define WDT_CHECK_PERIOD_CMD_ERROR              2000

#define WDT_CHECK_PERIOD_20                       20
#define WDT_CHECK_PERIOD_50                       50
#define WDT_CHECK_PERIOD_100                     100
#define WDT_CHECK_PERIOD_250                     250
#define WDT_CHECK_PERIOD_500                     500
#define WDT_CHECK_PERIOD                        1000


#define WDT_CHECK_DEFAULT_TIMES                    0
#define WDT_CHECK_BLINK_TIMES_OK                  10
#define WDT_CHECK_BLINK_TIMES_ERROR                1

#define WDT_CHECK_ACTIVE_TASK                      7
#define WDT_CHECK_COUNTER                          2
#define WDT_REQUEST_COUNTER                        8
#define WDT_MAX_COUNTER                            WDT_REQUEST_COUNTER + WDT_CHECK_COUNTER

#define WDT_CHECK_TASK_GPS_CODE                   0
#define WDT_CHECK_TASK_SHELL_CODE                 1
#define WDT_CHECK_TASK_CHECK_POS_CODE             2

#define WDT_CHECK_TASK_GPS_VALUE                 0x1
#define WDT_CHECK_TASK_SHELL_VALUE               0x2
#define WDT_CHECK_TASK_CHECK_POS_VALUE           0x4


#define WDT_CHECK_WATCHED_TASKS  WDT_CHECK_TASK_GPS_VALUE + \
                                 WDT_CHECK_TASK_SHELL_VALUE + \
                                 WDT_CHECK_TASK_CHECK_POS_VALUE

typedef union
{
  uint8_t Register;
  struct
  {
    char t0  :1;
    char t1  :1;
    char t2  :1;
  } Task;
} sWDTCheck_HealthTask;

typedef struct
{
  bool bCheckProcess;
  bool bLaunchProcess;
  uint16_t u16CounterProcess;
  uint8_t  u8TaskToCheck;
  sWDTCheck_HealthTask sTask;
} sWDTCheck_Health;


typedef struct
{
  bool bUsePermanentValue;
  uint8_t u8TimesBeforeReturn;
  uint16_t u16PeriodCurrent;
  uint16_t u16PeriodTemp;
  sWDTCheck_Health sHealth;
}WDTCheck;


void WDTCheck_InitFW(void);
void WDTCheck_Period(bool bIsPermanent, uint16_t u16Period, uint8_t u8Times);
void WDTCheck_HealthRequest(char code);
void WDTCheck_HealthResponse(char code);
void WDTCheck_RestartCounter(void);
void WDTCheck_RestartProcess(bool bIsFotActive);

#endif /* __WDT_CHECK_H */
