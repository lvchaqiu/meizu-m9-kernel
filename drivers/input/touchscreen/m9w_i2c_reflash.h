
#ifndef _SYNAPTICS_I2C_RMI_H_
#define  _SYNAPTICS_I2C_RMI_H_

#include <linux/m9w_i2c_rmi.h>

const char* SynaFirmware = NULL; 

typedef enum
{
  ESuccess,                   //! No Error
  ENoUsb,                     //! Cannot establish connection over USB with the controller
  ENotEnoughMem,              //! Memory allocation failed
  EUsbReadFailed,             //! Read from the USB bulk endpoint failed
  EUsbWriteFailed,            //! Write on the USB bulk endpoint failed
  EUsbCtrlEndPointFailed,     //! An I/O operation on the control endpoint failed
  EReadTimeout,               //! Waiting for response to a command timed out
  EWriteTimeout,              //! Trying to write a command to the usb pipe timed out 
  EErrorController,           //! The controller return returned err_msg=".." in reply to a cdci cmd
  EErrorResponsePacket,       //! The response to a command is not an expected one (parsing error,
                              //  not right response, etc). 
  EErrorPacket,               //! The controller returned an <error err.../> response
  EErrorNoParser,             //! The crocodile parser couldn't be created
  EErrorNullData,             //! The user is trying to send a NULL data to the sensor
  EErrorNoElements,           //! The controller replied with and xml command with no elements
                              //  inside (eg: <reg> </reg>
  EErrorNotEnoughReplies,     //! The controller didn't reply to all our requests
  EErrorWriteFailedDueToSixtyRegistersBug,
  EInterruptEndpointConnectFailed,
  EInterruptEndpointDisconnectFailed,
  EInterruptEndpointReadFailed,
  EInterruptEndpointNotConnected,
  EErrorUsbFailure,
  EErrorAddressNotDefined,
  EErrorInvalidERmiAddressValue,
  EErrorInvalidEPullupsValue,
  EErrorInvalidESlaveSelectValue,
  EErrorDriver,
  EErrorBootID,
  EErrorOpenFile,
  EErrorFileNoHead,
  EErrorFirmwareSize,
  EErrorConfigSize,
  EErrorFunctionNotSupported,
  EErrorFlashNotDisabled,
  EErrorNoPDT,
  EErrorConfigCheckSum,
  EErrorFirmwareCheckSum,

} EError;

EError          m_ret;

typedef struct _RMI4FunctionDescriptor
{
  unsigned char m_QueryBase;
  unsigned char m_CommandBase;
  unsigned char m_ControlBase;
  unsigned char m_DataBase;
  unsigned char m_IntSourceCount;
  unsigned char m_ID;
} RMI4FunctionDescriptor;

typedef struct _ConfigDataBlock
{
  unsigned short m_Address;
  unsigned char m_Data;
} ConfigDataBlock;

RMI4FunctionDescriptor m_PdtF34Flash;
RMI4FunctionDescriptor m_PdtF01Common;
RMI4FunctionDescriptor m_BaseAddresses;
  
unsigned char m_bFlashProgOnStartup;
unsigned char m_bUnconfigured;

unsigned short m_uQuery_Base;

unsigned int m_lengthWritten, m_lengthRead;

ConfigDataBlock g_ConfigDataList[0x2d - 4];

unsigned int g_ConfigDataCount;

unsigned long   g_uTimeout;
unsigned short  m_uTarget;

// Control bridge data
unsigned short m_BootloadID;
unsigned short m_BootID_Addr;

// Image file 
unsigned long m_checkSumImg;

// buffer for flash images ... tomv
//unsigned char FirmwareImage[16000];  // make smaller and dynamic
//unsigned char ConfigImage[16000];  // make smaller and dynamic
  
unsigned short m_bootloadImgID;
unsigned char m_firmwareImgVersion;
unsigned char *m_firmwareImgData;
unsigned char *m_configImgData;
unsigned short m_firmwareBlockSize;
unsigned short m_firmwareBlockCount;
unsigned short m_configBlockSize;
unsigned short m_configBlockCount;

unsigned int m_totalBlockCount;
unsigned int m_currentBlockCount;

// Registers for configuration flash
unsigned char m_uPageData[0x200];
unsigned char m_uStatus;
unsigned long m_firmwareImgSize;
unsigned long m_configImgSize;
unsigned long m_fileSize;

unsigned short m_uF01RMI_CommandBase;
unsigned short m_uF01RMI_DataBase;
unsigned short m_uF01RMI_QueryBase;
unsigned short m_uF01RMI_IntStatus;

unsigned short m_uF34Reflash_DataReg;
unsigned short m_uF34Reflash_BlockNum;
unsigned short m_uF34Reflash_BlockData;
unsigned short m_uF34Reflash_FlashControl;
unsigned short m_uF34ReflashQuery_BootID;
unsigned short m_uF34ReflashQuery_FirmwareBlockSize;
unsigned short m_uF34ReflashQuery_FirmwareBlockCount;
unsigned short m_uF34ReflashQuery_ConfigBlockSize;
unsigned short m_uF34ReflashQuery_ConfigBlockCount;
unsigned short m_uF34ReflashQuery_FlashPropertyQuery;

unsigned long m_FirmwareImgFile_checkSum;
  
//  Constants
static const unsigned char s_uF34ReflashCmd_FirmwareCrc   = 0x01;
static const unsigned char s_uF34ReflashCmd_FirmwareWrite = 0x02;
static const unsigned char s_uF34ReflashCmd_EraseAll      = 0x03;
static const unsigned char s_uF34ReflashCmd_ConfigRead    = 0x05;
static const unsigned char s_uF34ReflashCmd_ConfigWrite   = 0x06;
static const unsigned char s_uF34ReflashCmd_ConfigErase   = 0x07;
static const unsigned char s_uF34ReflashCmd_Enable        = 0x0f;
static const unsigned char s_uF34ReflashCmd_NormalResult  = 0x80; 

int synaptic_read_blockdata(unsigned char addr, unsigned char *data, unsigned int length);

int synaptic_write_blockdata(unsigned char addr, unsigned char *data, unsigned int length);

#endif

