/*******************************************************************************
* Filename: gps.h
* Developer: Jorge Yesid Rios Ortiz
*******************************************************************************/

#ifndef __GPS_H
#define __GPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx_hal.h"
#include "stdbool.h"
#include "stdlib.h"


#define GPS_RMC_NMEA_FRAME_FOUND  0x00
#define GPS_UNKNOWN_NMEA_FRAME    0xFF


#define GPS_MAX_FIELD_LEN   13
#define GPS_RX_QUEUE_SIZE   80
#define GPS_FRAME_TOKEN     ','
#define GPS_FRAME_END       '*'

#define GPS_TIMER_1_SEG   1099


typedef struct
{
  int8_t  min;
  int8_t  max;
} sMinMaxi8;

typedef struct
{
  uint8_t  min;
  uint8_t  max;
} sMinMaxu8;

typedef struct
{
  uint16_t  min;
  uint16_t  max;
} sMinMaxu16;

typedef struct
{
  int32_t  min;
  int32_t  max;
} sMinMaxi32;

typedef struct
{
  uint32_t  min;
  uint32_t  max;
} sMinMaxu32;


typedef struct
{
  sMinMaxi8  Status;
  sMinMaxu8  Minute;
  sMinMaxu8  Second;
  sMinMaxu8  Day;
  sMinMaxu8  Hour;
  sMinMaxu8  Month;
  sMinMaxu8  LatitudeOrientation;
  sMinMaxu8  LongitudeOrientation;
  sMinMaxu16 Year;
  sMinMaxi32 Latitude;
  sMinMaxi32 Longitude;
} sGpsValidationParameters;


typedef struct
{
  uint8_t  u8Hour;
  uint8_t  u8Min;
  uint8_t  u8Sec;
} sGpsTime;


typedef struct
{
  uint8_t  u8Day;
  uint8_t  u8Month;
  uint16_t u16Year;
} sGpsDate;


typedef struct
{
  sGpsDate sDate;
  sGpsTime sTime;
} sGpsDateTime;


typedef struct
{
  int8_t  cStatus;
} sGpsSateliteInfo;


typedef struct
{
  char    cOrientation;
  uint8_t u8Minutes;
  int16_t i16Degrees;
  double dValueDD;
} sGpsCoordinate;


typedef struct
{
  sGpsCoordinate sLatitude;
  sGpsCoordinate sLongitude;
} sGpsPosition;


typedef struct
{
  volatile bool             bValidFrame;
  volatile bool             bHealthRequest;
  volatile uint32_t         u32ValidDataAge;
  volatile sGpsPosition     sPos;
  sGpsDateTime              sDateTime;
  volatile sGpsSateliteInfo sSatInfo;
} sGpsData;


typedef struct
{ 
  volatile int8_t   cStatus;
  volatile uint8_t  u8Hour;
  volatile uint8_t  u8Minute;
  volatile uint8_t  u8Second;
  volatile uint8_t  u8Month;
  volatile uint8_t  u8Day;
  volatile uint16_t u16Year;
  volatile sGpsCoordinate sLatitude;
  volatile sGpsCoordinate sLongitude;
} sGpsDataFromGps;


void gps_InitFw(void);
void gps_Task(void * argument);
void gps_ReceiveData(uint8_t);

sGpsPosition gps_GetPosition(void);

sGpsCoordinate gps_GetStrLatitude(void);
uint8_t gps_GetLatitudeDegrees(void);
uint8_t gps_GetLatitudeMinutes(void);
uint8_t gps_GetLatitudeOrientation(void);
double gps_GetLatitudeDegreeDecimals(void);

sGpsCoordinate gps_GetStrLongitude(void);
uint8_t gps_GetLongitudeDegrees(void);
uint8_t gps_GetLongitudeMinutes(void);
int16_t gps_GetLongitudeMinutesFrac(void);
double gps_GetLongitudeDegreeDecimals(void);

bool gps_IsValidFrame(void);
uint32_t gps_GetValidDataAge(void);

void gps_PPSReceived(void);
void gps_NoPPSReceived(void);

void gps_HealthRequest(void);


#ifdef __cplusplus
}
#endif

#endif /* __GPS_H */
