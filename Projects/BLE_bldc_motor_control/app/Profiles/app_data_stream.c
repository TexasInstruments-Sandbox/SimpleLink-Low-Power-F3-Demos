/******************************************************************************

@file  app_data.c

@brief This file contains the Data Stream application functionality.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2024, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************


*****************************************************************************/

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <time.h>
#include <ti/drivers/GPIO.h>
#include <ti/bleapp/profiles/data_stream/data_stream_profile.h>
#include <ti/bleapp/ble_app_util/inc/bleapputil_api.h>
#include <ti/bleapp/menu_module/menu_module.h>
#include <app_main.h>
#include <app_bldc_motor_control.h>
#include <common/Profiles/simple_gatt/simple_gatt_profile.h>

//*****************************************************************************
//! Defines
//*****************************************************************************
#define DS_CCC_UPDATE_NOTIFICATION_ENABLED  1

//*****************************************************************************
//! Globals
//*****************************************************************************
char buffer[MAX_STRING];

static void SimpleGatt_changeCB( uint8_t paramId );

// Simple GATT Profile Callbacks
static SimpleGattProfile_CBs_t simpleGatt_profileCBs =
{
  SimpleGatt_changeCB // Simple GATT Characteristic value change callback
};

//*****************************************************************************
//!LOCAL FUNCTIONS
//*****************************************************************************

static void DS_onCccUpdateCB( uint16 connHandle, uint16 pValue );
static void DS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len );

//*****************************************************************************
//!APPLICATION CALLBACK
//*****************************************************************************
// Data Stream application callback function for incoming data
static DSP_cb_t ds_profileCB =
{
  DS_onCccUpdateCB,
  DS_incomingDataCB
};

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      SimpleGatt_ChangeCB
 *
 * @brief   Callback from Simple Profile indicating a characteristic
 *          value change.
 *
 * @param   paramId - parameter Id of the value that was changed.
 *
 * @return  None.
 */
static void SimpleGatt_changeCB( uint8_t paramId )
{
  uint8_t newValue = 0;

  switch( paramId )
  {
    case SIMPLEGATTPROFILE_CHAR1:
      {
        SimpleGattProfile_getParameter( SIMPLEGATTPROFILE_CHAR1, &newValue );
        charEvents = CHAR1_INPUT;
        char1 = newValue;
        BLECallback();
      }
      break;

    case SIMPLEGATTPROFILE_CHAR3:
      {
        SimpleGattProfile_getParameter(SIMPLEGATTPROFILE_CHAR3, &newValue);
        charEvents = CHAR3_INPUT;
        char3 = newValue;
        BLECallback();
      }
      break;
    case SIMPLEGATTPROFILE_CHAR4:
      {
          SimpleGattProfile_getParameter(SIMPLEGATTPROFILE_CHAR4, &newValue);
          charEvents = CHAR4_INPUT;
          char4 = newValue;
          BLECallback();
          break;
      }
    default:
      // should not reach here!
      break;
  }
}


/*********************************************************************
 * @fn      DS_onCccUpdateCB
 *
 * @brief   Callback from Data_Stream_Profile indicating ccc update
 *
 * @param   cccUpdate - pointer to data structure used to store ccc update
 *
 * @return  SUCCESS or stack call status
 */
static void DS_onCccUpdateCB( uint16 connHandle, uint16 pValue )
{
  if ( pValue == DS_CCC_UPDATE_NOTIFICATION_ENABLED)
  {
  }
  else
  {
  }
}

/*********************************************************************
 * @fn      DS_incomingDataCB
 *
 * @brief   Callback from Data_Stream_Profile indicating incoming data
 *
 * @param   dataIn - pointer to data structure used to store incoming data
 *
 * @return  SUCCESS or stack call status
 */
static void DS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len )
{
  bStatus_t status = SUCCESS;
  char dataOut[] = "Data size is too long";
  uint16 i = 0;
  char buffer[MAX_STRING];

  // The incoming data length was too large
  if ( len == 0 )
  {
    // Send error message over GATT notification
    status = DSP_sendData( (uint8 *)dataOut, sizeof( dataOut ) );
  }

  // New data received from peer device
  else
  {

    }

    // Echo the incoming data over GATT notification
    status = DSP_sendData( (uint8 *)buffer, strlen(buffer) );
  
}

/*********************************************************************
 * @fn      DataStream_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the Data Stream profile.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t DataStream_start( void )
{
  bStatus_t status = SUCCESS;

  //status = DSP_start( &ds_profileCB );
  if( status != SUCCESS )
  {
    // Return status value
    return status;
  }

  // ADD BLDC Characteristics Service
  status = SimpleGattProfile_addService();
  if( status != SUCCESS )
  {
    // Return status value
    return status;
  }

  // Set BLDC Characteristics Parameters
    uint8_t charValue1 = 0;
    uint8_t charValue2[SIMPLEGATTPROFILE_CHAR2_LEN] = { 1, 2};
    uint8_t charValue3 = 3;
    uint8_t charValue4 = 4;
    uint8_t charValue5 = 5;

    SimpleGattProfile_setParameter( SIMPLEGATTPROFILE_CHAR1, sizeof(uint8_t),
                                    &charValue1 );
    SimpleGattProfile_setParameter( SIMPLEGATTPROFILE_CHAR2, SIMPLEGATTPROFILE_CHAR2_LEN,
                                    charValue2 );
    SimpleGattProfile_setParameter( SIMPLEGATTPROFILE_CHAR3, sizeof(uint8_t),
                                    &charValue3 );
    SimpleGattProfile_setParameter( SIMPLEGATTPROFILE_CHAR4, sizeof(uint8_t),
                                    &charValue4 );

  // Register BLDC CHaracteristics Callbacks
  status = SimpleGattProfile_registerAppCBs( &simpleGatt_profileCBs );

  return ( SUCCESS );
}
