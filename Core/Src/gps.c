/*******************************************************************************
* Filename: gps.c
* Developer(s): Jorge Yesid Rios Ortiz
*******************************************************************************/

#include "gps.h"
#include "main.h"
#include "cmsis_os.h"
#include "gpio.h"
#include "usart.h"
#include "WDT_Check.h"

extern TIM_HandleTypeDef htim3;
#define TIMER &htim3
#define TIMER_INSTANCE   htim3.Instance

TaskHandle_t gpsTaskHandle = NULL;           // freeRTOS task handle
QueueHandle_t gpsRxQueue = NULL;             // freeRTOS handle for GPS Rx Queue
SemaphoreHandle_t gpsSemaphoreHandle = NULL; // freeRTOS handle for GPS Rx Semaphore

sGpsData GpsData;             // gps data with validation
sGpsDataFromGps  GpsDataRaw;  // Temporal gps data without validation
sGpsValidationParameters  GpsValidationParameters; // Parameters to gps validation

void gps_InitHw(void);
void gps_InitValidationParameters(void);

void gps_FrameDecoder(void);
uint8_t *gps_GetNextToken(uint8_t * pData);
uint8_t gps_FrameOfInterest(uint8_t *pData);
bool gps_VerifyChecksumNmea(unsigned char * pData);

void gps_ExtractDataRMC(uint8_t *pframe);
bool gps_ValidationNMEAData(void);
void gps_UpdateGpsData(void);

void gps_ExtractTime(uint8_t *pData);
void gps_ExtractDate(uint8_t *pData);
sGpsCoordinate gps_ExtractCoordinate(uint8_t *pData);

uint8_t gps_HexaCharToAscii(uint8_t uHexa);


void gps_InitFw(void)
{
  if (NULL==gpsTaskHandle)
  {
    xTaskCreate(gps_Task,         // Function that implements the task
                "Gps",            // Text name for the task
                200,              // Stack size in words, not bytes
                (void *) 1,       // Parameter passed into the task
                osPriorityNormal, // Priority at which the task is created
                &gpsTaskHandle);  // Used to pass out the created task's handle
  }
}


void gps_InitHw(void)
{
  MX_USART1_UART_Init(); //uart GPS
  HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);    // USART1_IRQn interrupt configuration
  HAL_NVIC_EnableIRQ(USART1_IRQn);            // USART1_IRQn interrupt configuration
  __HAL_UART_ENABLE_IT(&huart1,UART_IT_RXNE); // Enable Receive Interrupt
  printf("GPS Hw Ok\r\n");
}

void gps_Task(void * argument)
{
  uint8_t gpsFirstByte = 0;
  GpsData.bHealthRequest = false;

  printf("Init GPS\r\n");


  if (NULL==gpsSemaphoreHandle)
  {
    gpsSemaphoreHandle = xSemaphoreCreateBinary();
  }

  if (NULL==gpsRxQueue)
  {
    gpsRxQueue = xQueueCreate(GPS_RX_QUEUE_SIZE, sizeof(uint8_t));
  }

  gps_InitValidationParameters(); // validation parameters setup
  gps_InitHw(); // Init Hardware

  printf("GPS Task Ok\r\n");
  for (;;)
  {
    if( xSemaphoreTake( gpsSemaphoreHandle, portMAX_DELAY ) == pdTRUE) // wait until \n arrives from UART
    {
      if( xQueueReceive( gpsRxQueue, &gpsFirstByte, portMAX_DELAY ) == pdTRUE )
      {
        if(gpsFirstByte == '$')  // Verifies the head of Frame
        {
          gps_FrameDecoder();
        }
      }
      if (false!=GpsData.bHealthRequest)  //Watchdog timer
      {
        WDTCheck_HealthResponse(WDT_CHECK_TASK_GPS_CODE);
        GpsData.bHealthRequest = false;
      }
      xQueueReset(gpsRxQueue);
    }
  }
}


void gps_ReceiveData(uint8_t pRxData)
{
  if (NULL==gpsTaskHandle || NULL == gpsSemaphoreHandle )
  {
    return;
  }
  static portBASE_TYPE xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(gpsRxQueue, &pRxData, NULL);
  if( (pRxData == '\n') || (xQueueIsQueueFullFromISR(gpsRxQueue) != pdFALSE))
  {
    xSemaphoreGiveFromISR( gpsSemaphoreHandle, &xHigherPriorityTaskWoken );  //Semaphore signal
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
}


sGpsPosition gps_GetPosition(void)
{
  return GpsData.sPos;
}


sGpsCoordinate gps_GetStrLatitude(void)
{
    return GpsData.sPos.sLatitude;
}


uint8_t gps_GetLatitudeDegrees(void)
{
  return GpsData.sPos.sLatitude.i16Degrees;
}


uint8_t gps_GetLatitudeMinutes(void)
{
  return GpsData.sPos.sLatitude.u8Minutes;
}


uint8_t gps_GetLatitudeOrientation(void)
{
    return GpsData.sPos.sLatitude.cOrientation;
}


double gps_GetLatitudeDegreeDecimals(void)
{
  return GpsData.sPos.sLatitude.dValueDD;
}


sGpsCoordinate gps_GetStrLongitude(void)
{
    return GpsData.sPos.sLongitude;
}


uint8_t gps_GetLongitudeDegrees(void)
{
    return GpsData.sPos.sLongitude.i16Degrees;
}


uint8_t gps_GetLongitudeMinutes(void)
{
    return GpsData.sPos.sLongitude.u8Minutes;
}


uint8_t gps_GetLongitudeOrientation(void)
{
    return GpsData.sPos.sLongitude.cOrientation;
}


double gps_GetLongitudeDegreeDecimals(void)
{
  return GpsData.sPos.sLongitude.dValueDD;
}


bool gps_IsValidFrame(void)
{
  if (GpsData.u32ValidDataAge <= 1)
  {
    return true;
  }
  else
  {
    return false;
  }
}


uint32_t gps_GetValidDataAge(void)
{
  return GpsData.u32ValidDataAge;
}


void gps_InitValidationParameters(void)
{
  GpsValidationParameters.Hour.min = 0;
  GpsValidationParameters.Hour.max = 23;
  GpsValidationParameters.Minute.min = 0;
  GpsValidationParameters.Minute.max = 59;
  GpsValidationParameters.Second.min = 0;
  GpsValidationParameters.Second.max = 60;
  GpsValidationParameters.Status.min = 'A';
  GpsValidationParameters.Status.max = 'A';
  GpsValidationParameters.Latitude.min = -90; // Convert to miliminutes
  GpsValidationParameters.Latitude.max =  90; // Convert to miliminutes
  GpsValidationParameters.LatitudeOrientation.min = 'N';  // Must to be compare, doesn't to search into a range
  GpsValidationParameters.LatitudeOrientation.max = 'S';  // Must to be compare, doesn't to search into a range
  GpsValidationParameters.Longitude.min = -180; // Convert to miliminutes
  GpsValidationParameters.Longitude.max =  180; // Convert to miliminutes
  GpsValidationParameters.LongitudeOrientation.min = 'E'; // Must to be compare, doesn't to search into a range
  GpsValidationParameters.LongitudeOrientation.max = 'O'; // Must to be compare, doesn't to search into a range
  GpsValidationParameters.Day.min = 1;
  GpsValidationParameters.Day.max = 31;
  GpsValidationParameters.Month.min = 1;
  GpsValidationParameters.Month.max = 12;
  GpsValidationParameters.Year.min = 1970;
  GpsValidationParameters.Year.max = 2038;
}


void gps_FrameDecoder(void)
{
  uint8_t gpsRxBuff[GPS_RX_QUEUE_SIZE];
  uint8_t gpsRxSize = 0;
  uint8_t gpsRxIndex = 0;
  uint8_t gpsRxCurrentChar = 0;
  gpsRxSize = uxQueueMessagesWaiting(gpsRxQueue);
  if( gpsRxSize >= 3)
  {
    do
    {
      if( xQueueReceive( gpsRxQueue, &gpsRxCurrentChar, portMAX_DELAY == pdTRUE ) )
      {
        gpsRxBuff[gpsRxIndex] = gpsRxCurrentChar;
      }
      gpsRxIndex++;
    }while(gpsRxIndex <= (gpsRxSize-2) );  // Copy from queue to local buffer

    if( gps_VerifyChecksumNmea(gpsRxBuff) == true )
    {
      if(gps_FrameOfInterest(gpsRxBuff) == GPS_RMC_NMEA_FRAME_FOUND)
      {
        gps_ExtractDataRMC(gpsRxBuff);
        if ( gps_ValidationNMEAData() == true )
        {
          gps_UpdateGpsData();
        }
      }
      else
      {
      }
    }
  }
}


uint8_t *gps_GetNextToken(uint8_t * pData)
{
  uint8_t ucFieldLenCtrl = 0;
  for(;( (ucFieldLenCtrl < GPS_MAX_FIELD_LEN) && ( *(pData + ucFieldLenCtrl) != GPS_FRAME_TOKEN)&& ( *(pData + ucFieldLenCtrl) != GPS_FRAME_END) ) ; ucFieldLenCtrl++ );
  ucFieldLenCtrl++;
  return(pData + ucFieldLenCtrl);
}


uint8_t gps_FrameOfInterest(uint8_t *pData)
{
  uint8_t ucReturn = GPS_UNKNOWN_NMEA_FRAME;
  if(  *(pData+2) && (*(pData+3) == 'M') && (*(pData+4) == 'C') )
  {
    ucReturn = GPS_RMC_NMEA_FRAME_FOUND;  // RMC frame found
  }
  return(ucReturn);
}


bool gps_VerifyChecksumNmea(unsigned char * pData)
{
  static uint8_t ucIndex;
  static uint8_t ucCalculatedChecksum;
  static bool ucVerificationResult;
  static uint8_t ucCurrentValue;
  static uint8_t u8TempResult;

  ucIndex = 0;
  ucCalculatedChecksum = 0;
  ucVerificationResult = false;
  u8TempResult = 0;
  do
  {
    ucCurrentValue = *(pData + ucIndex);
    ucIndex++;
    if( ucCurrentValue == GPS_FRAME_END )
    {
      ucVerificationResult = true;
    }
    else
    {
      ucCalculatedChecksum ^= ucCurrentValue;
    }
  }
  while( ucCurrentValue != GPS_FRAME_END &&  ucIndex < GPS_RX_QUEUE_SIZE );

  if( ucVerificationResult == true )
  {
     ucVerificationResult = false;
     ucCurrentValue = ( ucCalculatedChecksum >> 4 );
     u8TempResult = gps_HexaCharToAscii(ucCurrentValue);
     ucCurrentValue = *(pData + ucIndex);
     if( ucCurrentValue == u8TempResult )
     {
       ucCurrentValue = ( ucCalculatedChecksum & 0x0F );
       u8TempResult = gps_HexaCharToAscii(ucCurrentValue);
       ucIndex++;
       ucCurrentValue = *(pData + ucIndex);
       if( ucCurrentValue == u8TempResult )
       {
         ucVerificationResult = true;
       }
       else
       {
         ucVerificationResult = false;
       }
     }
  }
  return(ucVerificationResult);
}


void gps_ExtractDataRMC(uint8_t *pframe)
{
  uint8_t *pData = pframe;

  pData = gps_GetNextToken(pData);
  gps_ExtractTime(pData);  // Extract HH:MM:SS

  pData = gps_GetNextToken(pData);
  GpsDataRaw.cStatus = *pData;
  if(GpsDataRaw.cStatus != 'A')
  {
    return; // invalid frame, it doesn't continue analyzing
  }

  pData = gps_GetNextToken(pData);
  GpsDataRaw.sLatitude = gps_ExtractCoordinate(pData);  // Extract latitude coordinate

  pData = gps_GetNextToken(pData);
  if(*pData == 'N')  // Extract latitude orientation
  {
    GpsDataRaw.sLatitude.cOrientation = 'N';
  }
  else
  {
    GpsDataRaw.sLatitude.cOrientation = 'S';
    GpsDataRaw.sLatitude.dValueDD *= -1;
    GpsDataRaw.sLatitude.i16Degrees *= -1;
  }

  pData = gps_GetNextToken(pData);
  GpsDataRaw.sLongitude = gps_ExtractCoordinate(pData);  // Extract longitude coordinate
  pData = gps_GetNextToken(pData);
  if(*pData == 'E')  // Extract longitude orientation
  {
    GpsDataRaw.sLongitude.cOrientation = 'E';
  }
  else
  {
    GpsDataRaw.sLongitude.cOrientation = 'W';
    GpsDataRaw.sLongitude.dValueDD *= -1;
    GpsDataRaw.sLongitude.i16Degrees *= -1;
  }

  pData = gps_GetNextToken(pData);  // Speed en Knots. dont use it
  pData = gps_GetNextToken(pData);  // true track. dont use it
  pData = gps_GetNextToken(pData);
  gps_ExtractDate(pData);  // Extract DD/MM/YYYY
}


bool gps_ValidationNMEAData(void)
{
  if( (GpsDataRaw.cStatus != GpsValidationParameters.Status.min) )
  {
    return false;
  }
  if( ((GpsDataRaw.u8Hour   >= GpsValidationParameters.Hour.min)   &&
       (GpsDataRaw.u8Hour   <= GpsValidationParameters.Hour.max)   &&
       (GpsDataRaw.u8Minute >= GpsValidationParameters.Minute.min) &&
       (GpsDataRaw.u8Minute <= GpsValidationParameters.Minute.max) &&
       (GpsDataRaw.u8Second >= GpsValidationParameters.Second.min) &&
       (GpsDataRaw.u8Second <= GpsValidationParameters.Second.max)) == false )
  {
    return false;
  }

  if( ((GpsDataRaw.u16Year >= GpsValidationParameters.Year.min)  &&
       (GpsDataRaw.u16Year <= GpsValidationParameters.Year.max)  &&
       (GpsDataRaw.u8Month >= GpsValidationParameters.Month.min) &&
       (GpsDataRaw.u8Month <= GpsValidationParameters.Month.max) &&
       (GpsDataRaw.u8Day   >= GpsValidationParameters.Day.min)   &&
       (GpsDataRaw.u8Day   <= GpsValidationParameters.Day.max) ) == false )
  {
    return false;
  }
  if( ((GpsDataRaw.sLatitude.i16Degrees >= GpsValidationParameters.Latitude.min ) &&
       (GpsDataRaw.sLatitude.i16Degrees <= GpsValidationParameters.Latitude.max) &&
       (GpsDataRaw.sLatitude.cOrientation == 'N' || GpsDataRaw.sLatitude.cOrientation == 'S') ) == false )
  {
    return false;
  }
  if( ((GpsDataRaw.sLongitude.i16Degrees >= GpsValidationParameters.Longitude.min ) &&
       (GpsDataRaw.sLongitude.i16Degrees <= GpsValidationParameters.Longitude.max) &&
       (GpsDataRaw.sLongitude.cOrientation == 'E' || GpsDataRaw.sLongitude.cOrientation == 'W') ) == false )
  {
    return false;
  }
  return true;
}


void gps_UpdateGpsData(void)
{
  // Valid frame
  GpsData.bValidFrame = true;
  GpsData.u32ValidDataAge = 0;

  // Satellite status
  GpsData.sSatInfo.cStatus  = GpsDataRaw.cStatus;

   // Date time
  GpsData.sDateTime.sDate.u8Day = GpsDataRaw.u8Day;
  GpsData.sDateTime.sDate.u8Month = GpsDataRaw.u8Month;
  GpsData.sDateTime.sDate.u16Year = GpsDataRaw.u16Year;
  GpsData.sDateTime.sTime.u8Hour = GpsDataRaw.u8Hour;
  GpsData.sDateTime.sTime.u8Min = GpsDataRaw.u8Minute;
  GpsData.sDateTime.sTime.u8Sec = GpsDataRaw.u8Second;

  // position
  GpsData.sPos.sLatitude = GpsDataRaw.sLatitude;
  GpsData.sPos.sLongitude = GpsDataRaw.sLongitude;

}


void gps_PPSReceived(void)
{
  GpsData.u32ValidDataAge +=1;
  if (GpsData.u32ValidDataAge > 1)
  {
    GpsData.bValidFrame = false;
  }
}


void gps_NoPPSReceived(void)
{
  GpsData.u32ValidDataAge +=1;
  GpsData.bValidFrame = false;
  if (NULL!=TIMER_INSTANCE)
  {
    __HAL_TIM_SET_COUNTER(TIMER, 0);
    __HAL_TIM_SET_AUTORELOAD(TIMER, GPS_TIMER_1_SEG);
  }

}


void gps_HealthRequest(void)
{
  GpsData.bHealthRequest = true;
}


void gps_ExtractTime(uint8_t *pData)
{
  // Hour
  GpsDataRaw.u8Hour = (*pData - 0x30) * 10;
  GpsDataRaw.u8Hour += (*(pData+1) - 0x30);
  // Minutes
  GpsDataRaw.u8Minute = (*(pData + 2) - 0x30) * 10;
  GpsDataRaw.u8Minute += (*(pData + 3) - 0x30);
  // Seconds
  GpsDataRaw.u8Second = (*(pData + 4) - 0x30) * 10;
  GpsDataRaw.u8Second += (*(pData + 5) - 0x30);
}


void gps_ExtractDate(uint8_t *pData)
{
  //Yearh
  GpsDataRaw.u16Year = (*(pData+4) -0x30) *10;
  GpsDataRaw.u16Year += (*(pData+5) -0x30) + 2000;
  //Month
  GpsDataRaw.u8Month = (*(pData+2) -0x30) *10;
  GpsDataRaw.u8Month += (*(pData+3) -0x30);
  //Day
  GpsDataRaw.u8Day = (*(pData) -0x30) *10;
  GpsDataRaw.u8Day += (*(pData+1) -0x30);
}



sGpsCoordinate gps_ExtractCoordinate(uint8_t *pData)
{
  uint8_t u8DataSize = 0;
  uint8_t *pNextData;
  uint32_t u32Multiply = 1;
  uint8_t u8PointPos = 0;
  double decimals = 0;
  sGpsCoordinate sPos = {0, 0, 0, 0};
  pNextData = gps_GetNextToken(pData);
  // Get Field size
  u8DataSize = (uint8_t)(pNextData - pData)-1;
  if (*(pData+5)=='.')
  {
    sPos.i16Degrees = (*pData - 0x30) * 100;
    sPos.i16Degrees += (*(pData + 1) - 0x30) * 10;
    sPos.i16Degrees += (*(pData + 2) - 0x30);
    sPos.u8Minutes = (*(pData+3) - 0x30) * 10;
    sPos.u8Minutes += (*(pData+4) - 0x30);
    u8PointPos = 6;
  }
  else
  {
    sPos.i16Degrees = (*pData - 0x30) * 10;
    sPos.i16Degrees += (*(pData+1) - 0x30);
    sPos.u8Minutes = (*(pData+2) - 0x30) * 10;
    sPos.u8Minutes += (*(pData+3) - 0x30);
    u8PointPos = 5;
  }
  while(u8DataSize>u8PointPos)
  {
    u8DataSize--;
    decimals += (*(pData+u8DataSize) - 0x30) * u32Multiply;
    u32Multiply *=10;
  }

  decimals += (sPos.u8Minutes *u32Multiply);
  decimals /= (u32Multiply*100);
  sPos.dValueDD += sPos.i16Degrees + (decimals*10/6);
  return(sPos);
}


uint8_t gps_HexaCharToAscii(uint8_t uHexa)
{
  uint8_t ucResult = 0xFF;
  if( uHexa < 0x10 )
  {
    if( uHexa < 0x0A )
    {
      ucResult = uHexa + 0x30;
    }
    else
    {
      ucResult = uHexa + 0x37;
    }
  }
  return(ucResult);
}
