/******************************************************************************

@file  app_dev_info.c

@brief This file contains the device info application functionality

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

Copyright (c) 2022-2026, Texas Instruments Incorporated
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

Copyright (c) 2022-2026, Texas Instruments Incorporated
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************
*****************************************************************************/

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <stdio.h>
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include <ble_app/Profiles/dev_info/dev_info_services.h>
#include <nvs_flash/dev_info_flash.h>

//*****************************************************************************
//! Defines
//*****************************************************************************

//*****************************************************************************
//! Globals
//*****************************************************************************

//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      DevInfo_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the Device Info service.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t DevInfo_start(void)
{
  bStatus_t status = SUCCESS;
  uint8_t  *devAddr = NULL;
  uint8_t systemId[DEVINFO_SYSTEM_ID_LEN] = {0};

  // Initialize flash reading module
  DevInfoFlash_init();

  // Device Information Service
  status = DevInfo_addService();
  if ( status != SUCCESS )
  {
    // Return status value
    return( status );
  }

  devAddr = GAP_GetDevAddress( TRUE );

  memcpy( &systemId[0], &devAddr[0], 3 );   // use 6 bytes of device address for 8 bytes of system ID value
  memset( &systemId[3], 0xFFFE, 2 );        // set middle bytes to 0xFFFE
  memcpy( &systemId[5], &devAddr[3], 3 );   // shift three bytes up

  // Set Device Info Service System ID Parameter
  status = DevInfo_setParameter( DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId );
  if ( status != SUCCESS )
  {
    // Return status value
    return ( status );
  }

  // Set Device Info Service Manufacturer Name Parameter
  status = DevInfo_setParameter( DEVINFO_MANUFACTURER_NAME, DEVINFO_STR_ATTR_LEN, "Texas Instruments" );

  // === Read device identification from flash ===
  DeviceIdData_t deviceId;
  if (DevInfoFlash_readDeviceId(&deviceId))
  {
    // Update PnP ID with flash values
    uint8_t pnpId[DEVINFO_PNP_ID_LEN];
    pnpId[0] = deviceId.vendorSource;
    pnpId[1] = LO_UINT16(deviceId.vendorId);
    pnpId[2] = HI_UINT16(deviceId.vendorId);
    pnpId[3] = LO_UINT16(deviceId.productId);
    pnpId[4] = HI_UINT16(deviceId.productId);
    pnpId[5] = LO_UINT16(deviceId.productVersion);
    pnpId[6] = HI_UINT16(deviceId.productVersion);
    DevInfo_setParameter(DEVINFO_PNP_ID, DEVINFO_PNP_ID_LEN, pnpId);

    // Set Model Number (from device type)
    char modelStr[20];
    DevInfoFlash_getModelNumber(deviceId.deviceType, modelStr, sizeof(modelStr));
    DevInfo_setParameter(DEVINFO_MODEL_NUMBER, strlen(modelStr), modelStr);

    // Set Serial Number (from MAC address)
    char serialStr[20];
    DevInfoFlash_getSerialNumber(devAddr, serialStr, sizeof(serialStr));
    DevInfo_setParameter(DEVINFO_SERIAL_NUMBER, strlen(serialStr), serialStr);
  }

  // === Read firmware version from NVS ===
  FirmwareVersion_t fwVersion;
  DevInfoFlash_readFirmwareVersion(&fwVersion);

  // Software Revision
  DevInfo_setParameter(DEVINFO_SOFTWARE_REV, strlen(fwVersion.versionStr), fwVersion.versionStr);

  // Firmware Revision (same value)
  DevInfo_setParameter(DEVINFO_FIRMWARE_REV, strlen(fwVersion.versionStr), fwVersion.versionStr);

  // Hardware Revision (board version)
  const char *hwRev = "CC2340R53_1.0";
  DevInfo_setParameter(DEVINFO_HARDWARE_REV, strlen(hwRev), hwRev);

  // Return status value
  return ( status );
}
