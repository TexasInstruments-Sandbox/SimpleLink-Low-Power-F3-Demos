

#include "ti/ble/stack_util/bcomdef.h"
#include "ti/ble/host/gap/gapgattserver.h"
#include "ti/ble/host/gap/gap_advertiser.h"
#include <ble_app/app_main.h>
#include "ti/ble/host/gapbondmgr/gapbondmgr.h"
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"

// The GAP profile role
uint8_t profileRole = GAP_PROFILE_PERIPHERAL;
// GAP GATT Service (GGS) parameters
uint8_t attDeviceName[GAP_DEVICE_NAME_LEN]= "Onn-Remote";

uint8_t pRandomAddress[B_ADDR_LEN] = {0};

// Initiate selected profiles
void init_profiles(){
 }
/*********************************************************************
 * Bond Manager Configuration
 */

gapBondParams_t gapBondParams = {
    .pairMode             = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ,
    .mitm                 = true,
    .ioCap                = GAPBOND_IO_CAP_KEYBOARD_DISPLAY,
    .bonding              = true,
    .secureConnection     = GAPBOND_SECURE_CONNECTION_ALLOW,
    .authenPairingOnly    = true,
    .autoSyncAL           = false,
    .KeySize              = 16,
    .removeLRUBond        = false,
    .KeyDistList          = GAPBOND_KEYDIST_CENCKEY | GAPBOND_KEYDIST_CIDKEY | GAPBOND_KEYDIST_CSIGN | GAPBOND_KEYDIST_PENCKEY | GAPBOND_KEYDIST_PIDKEY | GAPBOND_KEYDIST_PSIGN,
    .eccDebugKeys         = true,
    .eraseBondWhileInConn = false,
    .sameIrkAction        = GAPBOND_SAME_IRK_UPDATE_BOND_REC
};

uint8_t pairMode                =    GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
uint8_t mitm                    =    true;
uint8_t ioCap                   =    GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
uint8_t bonding                 =    true;
uint8_t secureConnection        =    GAPBOND_SECURE_CONNECTION_ALLOW;
uint8_t authenPairingOnly       =    false;
uint8_t autoSyncAL              =    false;
uint8_t KeySize                 =    16;
uint8_t removeLRUBond           =    false;
uint8_t KeyDistList             =    GAPBOND_KEYDIST_CENCKEY | GAPBOND_KEYDIST_CIDKEY | GAPBOND_KEYDIST_CSIGN | GAPBOND_KEYDIST_PENCKEY | GAPBOND_KEYDIST_PIDKEY | GAPBOND_KEYDIST_PSIGN;
uint8_t eccDebugKeys            =    true;
uint8_t allowDebugKeys          =    true;
uint8_t eraseBondWhileInConn    =    false;
uint8_t sameIrkAction           =    GAPBOND_SAME_IRK_UPDATE_BOND_REC;

void setBondManagerParameters()
{
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(uint8_t), &pairMode);
    GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(uint8_t), &mitm);
    GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
    GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    GAPBondMgr_SetParameter(GAPBOND_SECURE_CONNECTION, sizeof(uint8_t), &secureConnection);
    // Set Authenticated Pairing Only mode
    GAPBondMgr_SetParameter(GAPBOND_AUTHEN_PAIRING_ONLY, sizeof(uint8_t), &authenPairingOnly);
    // Set Auto Acceptlist Sync
    GAPBondMgr_SetParameter(GAP_ADV_AL_POLICY_ANY_REQ, sizeof(uint8_t), &autoSyncAL);
    GAPBondMgr_SetParameter(GAPBOND_KEYSIZE, sizeof(uint8_t), &KeySize);
    GAPBondMgr_SetParameter(GAPBOND_LRU_BOND_REPLACEMENT, sizeof(uint8_t), &removeLRUBond);
    GAPBondMgr_SetParameter(GAPBOND_KEY_DIST_LIST, sizeof(uint8_t), &KeyDistList);
    // Set Secure Connection Debug Keys
    GAPBondMgr_SetParameter(GAPBOND_SC_HOST_DEBUG, sizeof(uint8_t), &eccDebugKeys);
    // Set Allow Debug Keys
    GAPBondMgr_SetParameter(GAPBOND_ALLOW_DEBUG_KEYS, sizeof(uint8_t), &allowDebugKeys);
    // Set the Erase bond While in Active Connection Flag
    GAPBondMgr_SetParameter(GAPBOND_ERASE_BOND_IN_CONN, sizeof(uint8_t), &eraseBondWhileInConn);
    GAPBondMgr_SetParameter(GAPBOND_SAME_IRK_OPTION, sizeof(uint8_t), &sameIrkAction);
}


// Advertising params (dynamically switched between undirected and directed)
// eventProps is modified at runtime: undirected (SCANNABLE) or directed (DIRECTED)
// peerAddr and peerAddrType are set dynamically for directed advertising
GapAdv_params_t advParams1 = {
  .eventProps =   GAP_ADV_PROP_CONNECTABLE | GAP_ADV_PROP_LEGACY | GAP_ADV_PROP_SCANNABLE,
  .primIntMin =   32,
  .primIntMax =   32,
  .primChanMap =  GAP_ADV_CHAN_ALL,
  .peerAddrType = PEER_ADDRTYPE_PUBLIC_OR_PUBLIC_ID,
  .peerAddr =     { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa },
  .filterPolicy = GAP_ADV_AL_POLICY_ANY_REQ,
  .txPower =      0,
  .primPhy =      GAP_ADV_PRIM_PHY_1_MBPS,
  .secPhy =       GAP_ADV_SEC_PHY_1_MBPS,
  .sid =          0
};

uint8_t advData1[] =
{
  0x0B,
  GAP_ADTYPE_LOCAL_NAME_COMPLETE,
  'O',
  'n',
  'n',
  '-',
  'R',
  'e',
  'm',
  'o',
  't',
  'e',

  0x02,
  GAP_ADTYPE_FLAGS,
  GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED | GAP_ADTYPE_FLAGS_GENERAL,

  0x03,
  GAP_ADTYPE_APPEARANCE,
  LO_UINT16(0x0180),
  HI_UINT16(0x0180),

  0x05,
  GAP_ADTYPE_16BIT_MORE,
  LO_UINT16(0x1812),
  HI_UINT16(0x1812),
  LO_UINT16(0x180f),
  HI_UINT16(0x180f),

};

uint8_t scanResData1[] =
{
  0x0B,
  GAP_ADTYPE_LOCAL_NAME_COMPLETE,
  'O',
  'n',
  'n',
  '-',
  'R',
  'e',
  'm',
  'o',
  't',
  'e',
};
