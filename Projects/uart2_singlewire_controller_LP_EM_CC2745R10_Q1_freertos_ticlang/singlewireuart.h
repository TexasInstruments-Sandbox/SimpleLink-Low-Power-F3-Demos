#define CMD_BOARDCAST_BTYE_2        0xAA
#define ID_REQUEST_SEQUENCE         0xAABBCCDD

#define MAXIMUM_ID              2
#define MAX_TIMEOUT             2000    // 2 ms
#define UART_BUFFER_MAX_SIZE    20
#define COMMAND_SIZE            9

#define BUILD_UINT16(loByte, hiByte)    ((loByte & 0xFF) | ((hiByte & 0xFF) << 8))
#define BUILD_UINT32(byte0, byte1, byte2, byte3)    (BUILD_UINT16(byte0, byte1) | (BUILD_UINT16(byte2, byte3) << 16))
#define GET_BYTE_0(data)            (data & 0xFF)
#define GET_BYTE_1(data)            ((uint8_t)((data >> 8) & 0xFF))
#define GET_BYTE_2(data)            ((uint8_t)((data >> 16) & 0xFF))
#define GET_BYTE_3(data)            ((uint8_t)((data >> 24) & 0xFF))

//index for data frame
#define SOF_IDX                     0x0
#define PAYLOAD_LEN_IDX             0x1
#define PAYLOAD_IDX                 0x3
#define CS_IDX(padloadLen)          (padloadLen+PAYLOAD_IDX)
#define EOF_IDX(padloadLen)         (padloadLen+PAYLOAD_IDX+2)

#define PAYLOAD_BUFFER(buf)         (buf+PAYLOAD_IDX)
#define GET_DATA_FRAME_SIZE(padloadLen)     (padloadLen+6)

#define TX_MAX_SIZE         1500
#define RX_MAX_SIZE         1200
#define TX_PAYLOAD_SIZE     1000
#define RX_PAYLOAD_SIZE     1000

uint8_t txBuffer[TX_MAX_SIZE];
uint8_t rxBuffer[RX_MAX_SIZE];

uint8_t txPayLoad[TX_PAYLOAD_SIZE];
uint8_t rxPayLoad[RX_PAYLOAD_SIZE];

uint16_t broadcastSize = 4;
uint16_t unicastSize = 4;

/*! @enum STATE_MODES */
typedef enum {
    /*! Waiting to assign ID to peripherals */
    MODE_ASSIGN_ID = 0x01,
    /*! Repeat unicast to each peripheral */
    MODE_REPEAT_UNICAST = 0x02,
    /*! Broadcasting to all peripherals */
    MODE_BROADCAST = 0x03,
    /*! Unicasting to one peripheral */
    MODE_UNICAST = 0x04,
    /*! Waiting for active send from peripheral  */
    MODE_ACTIVE_SEND = 0x05,
} STATE_MODES;

/*! @enum SEM_EVENTS */
typedef enum {
    /*! UART received successfully */
    UART_RECEIVED = 0x01,
    /*! UART transmitted successfully */
    UART_TRANSMITTED = 0x02,
    /*! Button pressed on LP */
    LPBUTTON_PRESSED = 0x03,
    /*! Clock timeout expired */
    CLOCK_TIMEOUT = 0x04,
    /*! Peripheral pulling the line low */
    PERIPHERAL_ACTIVITY = 0x05,
} SEM_EVENTS;

/*! @enum S_UART_STATE */
typedef enum {
    /*! Single-wire UART state of IDLE, MCU is in low power mode in this state */
    S_UART_STATE_IDLE = 0,
    /*! Single-wire UART state of receiving command */
    S_UART_STATE_RX_CMD = 1,
    /*! Single-wire UART state of receiving data */
    S_UART_STATE_RX_DATA = 2,
    /*! Single-wire UART state of transmitting command */
    S_UART_STATE_TX_CMD = 3,
    /*! Single-wire UART state of transmitting data */
    S_UART_STATE_TX_DATA = 4,
    /*! Single-wire UART state of waiting for IDLE */
    S_UART_STATE_WAIT_IDLE = 5
} S_UART_STATE;

/*! @enum UART_COMMAND */
typedef enum {
    /*! No command */
    CMD_NOCOMMAND = 0xFF,
    /*! Controller broadcast command, the second byte should be 0xAA for broadcast command */
    CMD_BROADCAST = 0x00,
    /*! Controller unicasting command, the second byte is ID */
    CMD_UNICASTING = 0x3A,
    /*! Controller assign ID for peripheral command, the second byte is ID  */
    CMD_ASSIGN_ID = 0x5A,
    /*! Peripheral actively send data to controller, the second byte is ID  */
    CMD_ACTIVE_SEND = 0x35,
    /*! Plug-in Notification Command */
    CMD_PLUGIN_NOTIFY = 0xAA,
    /*! Peripheral acknowledge to controller unicasting command, the second byte is ID  */
    CMD_ACK = 0x55,
} UART_COMMAND;

/* COMMAND frame */
typedef struct _COMMAND_PACKET
{
    uint8_t cmdCode;
    uint8_t reserved;//4 bits
    uint8_t cmdSeq;//4 bits
    uint32_t TagID;
    uint16_t dataLength;
    uint8_t checksum;
} COMMAND_PACKET;

/* Broadcast command data frame */
typedef struct _BROADCAST_DATA_FRAME
{
    //keep the same data type for struct component
    uint32_t begin;

    //payload
    /*
     * broadcast command brief example
     * bdcCmd:
     *  1 -> set PA0 LED
     *  2 -> clear PA0 LED
     *  3 -> toggle PA0 LED in 0.5Hz
     */
    uint32_t bdcCmd;

    uint32_t checksum;
    uint32_t end;
} BROADCAST_DATA;

/* Unicasting command data frame */
typedef struct _UNICASTING_DATA_FRAME
{
    uint32_t begin;

    //payload
    /* unicasting command brief example */

    /*
     * Command type:
     *  1 -> set LED color
     *  2 -> get ADC data
     */
    uint32_t cmdType;

    /* ledColor:
     *  1 -> red
     *  2 -> green
     */
    uint32_t ledColor;

    /* toggleFreq:
     *  0 -> channel 0 (PA27)
     *  1 -> channel 1 (PA26)
     *  2 -> channel 2 (PA25)
     *  3 -> channel 3 (PA24)
     */
    uint32_t adcChannel;

    uint32_t payloadData[248];

    uint32_t checksum;
    uint32_t end;
} UNICASTING_DATA;

/* Unicasting command acknowledge data frame */
typedef struct _UNICASTING_ACK_DATA_FRAME
{
    uint32_t begin;

    //payload
    /*
     * Peripheral actively send command brief example */

    /*
     * Send requested ADC channel voltage result to controller in Unicasting command (in Volt),
     * ignored if the unicasting command is for Led toggling
     */
    float adcResult;//V

    uint32_t checksum;
    uint32_t end;
} UNICASTING_ACK_DATA;


/* Peripheral actively send data command data frame */
typedef struct _ACTIVE_SEND_DATA_FRAME
{
    uint32_t begin;

    //payload
    /* Peripheral actively send command brief example */

    /* send four ADC channel voltage to controller (in Volt):
     *  channel 0 (PA27)
     *  channel 1 (PA26)
     *  channel 2 (PA25)
     *  channel 3 (PA24)
     */
    float adcChannel0;//V
    float adcChannel1;
    float adcChannel2;
    float adcChannel3;

    uint32_t checksum;
    uint32_t end;
} ACTIVE_SEND_DATA;