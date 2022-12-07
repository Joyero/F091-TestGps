/*******************************************************************************
* Filename: checkPosition.h
* Developer: Jorge Yesid Rios Ortiz
*******************************************************************************/

#ifndef __CHECK_POSITION_H
#define __CHECK_POSITION_H

#ifdef __cplusplus
extern "C" {
#endif


#include "stdint.h"
#include "stdbool.h"
#include "math.h"

#define CHECK_POS_DISTANCE             100
#define CHECK_POS_EARTH_RAD_DEFAULT   6378.1


typedef union
{
  uint8_t Register;
  struct
  {
    bool Lat   :1;
    bool Lon   :1;
    bool GpsOk :1;
  } cfg;
} sCheckPosDataValidation;


typedef struct
{
  bool bTimer;
  bool bHealthRequest;
} sCheckPosWakeupReason;

typedef struct
{
  double dLatitude;
  double dLongitude;
  double dDistance;
  double dEarthRad;
  sCheckPosWakeupReason sWakeupReason;
  sCheckPosDataValidation sDoCheck;
} sCheckPosApp;


#define CHECK_POS_PROMPT   "CHECK POS"

void checkPos_InitFw(void);

bool   checkPos_SetLatitude(double dLat);
double checkPos_GetLatitude(void);
bool   checkPos_SetLongitude(double dLon);
double checkPos_GetLongitude(void);
bool   checkPos_SetEarthRadius(double dLat);
double checkPos_GetEarthRadius(void);

void checkPos_HealthRequest(void);


#ifdef __cplusplus
}
#endif

#endif /* __CHECK_POSITION_H */

