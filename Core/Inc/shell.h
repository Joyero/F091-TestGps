/*******************************************************************************
* Filename: shell.h
* Developer: Jorge Yesid Rios Ortiz
*******************************************************************************/

#ifndef __SHELL_H
#define __SHELL_H

#ifdef __cplusplus
extern "C" {
#endif




#include "stdint.h"
#include "stdbool.h"


#define SHELL_PROMPT   "SHELL"
#define SHELL_TIME_RX_FROM_BLE_DEFAULT    80

#define SHELL_RX_BUFFER_SIZE 150
#define SHELL_TX_BUFFER_SIZE 400

//commads and its size
#define SHELL_CMD_LAT      "lat="
#define SHELL_CMD_LAT_SIZE 4
#define SHELL_CMD_LON      "lon="
#define SHELL_CMD_LON_SIZE 4
#define SHELL_CMD_RAD      "rad="
#define SHELL_CMD_RAD_SIZE 4
#define SHELL_CMD_GET_DATA      "data"
#define SHELL_CMD_GET_DATA_SIZE 4


typedef struct
{
  bool bRxFromBle;
  bool bHealthRequest;
} sShellWakeupReason;


typedef struct
{
  uint16_t u16Pos;
  char Buffer[SHELL_RX_BUFFER_SIZE];
} sShellBufferRx;


typedef struct
{
  uint8_t u8Pos;
  uint8_t Buffer[4];
} sShellBufferTemp;


typedef struct
{
  sShellBufferRx sRx;
  sShellBufferTemp sRxTemp;
  bool bReceivingFrame;
} sShellSerialPort;


typedef struct
{
  sShellSerialPort  sUart;
  sShellWakeupReason sWakeupReason;
} sShellApp;


void shell_InitFw(void);

void shell_ReceivedChar(uint8_t RxChar);

void shell_HealthRequest(void);


#ifdef __cplusplus
}
#endif

#endif /* __SHELL_H */
