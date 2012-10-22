
/*#include "meizum9_i2c_reflash.h"*/
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
//#include <plat/gpio-cfg.h>

#include "m9w_i2c_reflash.h"

#define RMI4CheckIfFatalError(...)  (0);
static spinlock_t rw_lock;
static spinlock_t intr_lock;
unsigned char is_synaptic_reflash_rw   = 0;
static struct synaptics_rmi4 *g_ts;
DECLARE_COMPLETION(reflash_intr_completion);

void set_reflash_rw_flag(unsigned char value)
{
    spin_lock(&rw_lock);
    is_synaptic_reflash_rw = value;
    spin_unlock(&rw_lock);
}

unsigned char get_reflash_rw_flag(void)
{
    unsigned char value;
    spin_lock(&rw_lock);
    value = is_synaptic_reflash_rw;
    spin_unlock(&rw_lock);
    return value;
}

int synaptic_read_blockdata(unsigned char addr, unsigned char *data, unsigned int length)
{
    int ret = 0;
    struct i2c_msg read_i2c_msg[2];
    
    while (1) {
        if(spin_trylock(&rw_lock)) {
            if (is_synaptic_reflash_rw) {
                spin_unlock(&rw_lock);
                msleep(1);
            } else {
                is_synaptic_reflash_rw = 1;
                spin_unlock(&rw_lock);
                break;
            }
        } else {
            msleep(1);
        }
    }

    /*
     *while (1) {
     *    if( get_reflash_rw_flag()) {
     *      schedule();
     *    } else {
     *        set_reflash_rw_flag(1);
     *        break;
     *    }
     *}
     */

    read_i2c_msg[0].addr = g_ts->client->addr;
    read_i2c_msg[0].flags = 0;
    read_i2c_msg[0].buf = (unsigned char*)&addr;
    read_i2c_msg[0].len = 1;

    read_i2c_msg[1].addr = g_ts->client->addr;
    read_i2c_msg[1].flags = I2C_M_RD;
    read_i2c_msg[1].buf = (__u8 *)(data);
    read_i2c_msg[1].len = length;

    ret = i2c_transfer(g_ts->client->adapter, read_i2c_msg, 2);
    if (ret < 0) {
        printk(KERN_ERR "I2C read failed at addr:%d, len:%d\n", addr, length);
    }
    
    while (1) {
        if(spin_trylock(&rw_lock)) {
            is_synaptic_reflash_rw = 0;
            spin_unlock(&rw_lock);
            break;
        } else {
            msleep(1);
        }
    }

    /*set_reflash_rw_flag(0);*/
    return ret;
}


int synaptic_write_blockdata(unsigned char addr, unsigned char *data, unsigned int length)
{
    int ret;
    struct i2c_msg write_i2c_msg[1];
    unsigned char *wrt_data;
    
    while (1) {
        if(spin_trylock(&rw_lock)) {
            if (is_synaptic_reflash_rw) {
                spin_unlock(&rw_lock);
                msleep(1);
            } else {
                is_synaptic_reflash_rw = 1;
                spin_unlock(&rw_lock);
                break;
            }
        } else {
            msleep(1);
        }
    }

    wrt_data  = kmalloc(length + 1, GFP_KERNEL);
    if (wrt_data == NULL) {
        printk("%s alloc mem faild!!\n", __func__);
        set_reflash_rw_flag(0);
        return -1;
    }
    wrt_data[0] = addr;
    memcpy(wrt_data + 1, data, length);

    write_i2c_msg[0].addr = g_ts->client->addr;
    write_i2c_msg[0].flags = 0;
    write_i2c_msg[0].buf = (__u8 *)(wrt_data);
    write_i2c_msg[0].len = length + 1;

    ret = i2c_transfer(g_ts->client->adapter, write_i2c_msg, 1);
    if (ret < 0) {
        printk(KERN_ERR "I2C write failed at addr:%d, len:%d\n", addr, length);
    }

    if (wrt_data) kfree(wrt_data);
    
    while (1) {
        if(spin_trylock(&rw_lock)) {
            is_synaptic_reflash_rw = 0;
            spin_unlock(&rw_lock);
            break;
        } else {
            msleep(1);
        }
    }

    /*set_reflash_rw_flag(0);*/

    return ret;
}

// This function gets the firmware block size and block count
void RMI4ReadFirmwareInfo(void)
{
  unsigned char uData[2];

  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_FirmwareBlockSize, &uData[0], 2);
  RMI4CheckIfFatalError(m_ret);

  m_firmwareBlockSize = uData[0] | (uData[1] << 8);

  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_FirmwareBlockCount, &uData[0], 2);
  RMI4CheckIfFatalError(m_ret);

  m_firmwareBlockCount = uData[0] | (uData[1] << 8);
  m_firmwareImgSize = m_firmwareBlockCount*m_firmwareBlockSize;

  printk("m_firmwareBlockSize, m_firmwareBlockCount: %d, %d\n", m_firmwareBlockSize, m_firmwareBlockCount);
}

int RMI4isExpectedRegFormat(void)
{
  // Flash Properties query 1: registration map format version 1
  //  0: registration map format version 0
  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_FlashPropertyQuery, &m_uPageData[0], 1);
  RMI4CheckIfFatalError(m_ret);
  /*printk("FlashPropertyQuery = 0x%x\n", m_uPageData[0]);*/
  return ((m_uPageData[0] & 0x01) == 0x01);
}

void RMI4setFlashAddrForDifFormat(void)
{
  if (RMI4isExpectedRegFormat())
  {
    printk("Image format 1\n");
    m_uF34Reflash_FlashControl = m_PdtF34Flash.m_DataBase + m_firmwareBlockSize + 2;
    m_uF34Reflash_BlockNum = m_PdtF34Flash.m_DataBase;
    m_uF34Reflash_BlockData = m_PdtF34Flash.m_DataBase + 2;
  }
  else {
    printk("Image format 0\n");
    m_uF34Reflash_FlashControl = m_PdtF34Flash.m_DataBase;
    m_uF34Reflash_BlockNum = m_PdtF34Flash.m_DataBase + 1;
    m_uF34Reflash_BlockData = m_PdtF34Flash.m_DataBase + 3;
  }
}

void RMI4PrintF01QueryInfo(void)
{
    unsigned char f01_query_info[20];
    int cnt;
    m_ret = synaptic_read_blockdata(m_uF01RMI_QueryBase, f01_query_info, sizeof(f01_query_info));
    if (m_ret >= 0) {
        for (cnt=0; cnt<sizeof(f01_query_info); cnt++) {
            printk("%s f01_query_info %02d:%02x\n", __func__, cnt, f01_query_info[cnt]);
        }
    } else {
        printk("Read f01_query_info error!\n");
    }
}

void RMI4ReadPageDescriptionTable(void)
{
  // Read config data
  unsigned short uAddress;
  RMI4FunctionDescriptor Buffer;

  m_PdtF01Common.m_ID = 0;
  m_PdtF34Flash.m_ID = 0;
  m_BaseAddresses.m_ID = 0xff;

  for(uAddress = 0xe9; uAddress > 10; uAddress -= sizeof(RMI4FunctionDescriptor))
  {
    m_ret = synaptic_read_blockdata(uAddress, (unsigned char*)&Buffer, sizeof(Buffer));
    RMI4CheckIfFatalError(m_ret);

    if(m_BaseAddresses.m_ID == 0xff)
    {
      m_BaseAddresses = Buffer;
    }

    switch(Buffer.m_ID)
    {
      case 0x34:
        m_PdtF34Flash = Buffer;
        break;
      case 0x01:
        m_PdtF01Common = Buffer;
        break;
    }

    if(Buffer.m_ID == 0)
    {
      break;
    }
    else
    {
      printk("Function $%02x found.\n", Buffer.m_ID);
    }
  }

  // Initialize device related data members
  m_uF01RMI_DataBase = m_PdtF01Common.m_DataBase;
  m_uF01RMI_IntStatus = m_PdtF01Common.m_DataBase + 1;
  m_uF01RMI_CommandBase = m_PdtF01Common.m_CommandBase;
  m_uF01RMI_QueryBase = m_PdtF01Common.m_QueryBase;

  m_uF34Reflash_DataReg = m_PdtF34Flash.m_DataBase;
  m_uF34Reflash_BlockNum = m_PdtF34Flash.m_DataBase;
  m_uF34Reflash_BlockData = m_PdtF34Flash.m_DataBase + 2;
  m_uF34ReflashQuery_BootID = m_PdtF34Flash.m_QueryBase;

  m_uF34ReflashQuery_FlashPropertyQuery = m_PdtF34Flash.m_QueryBase + 2;
  m_uF34ReflashQuery_FirmwareBlockSize = m_PdtF34Flash.m_QueryBase + 3;
  m_uF34ReflashQuery_FirmwareBlockCount = m_PdtF34Flash.m_QueryBase + 5;
  m_uF34ReflashQuery_ConfigBlockSize = m_PdtF34Flash.m_QueryBase + 3;
  m_uF34ReflashQuery_ConfigBlockCount = m_PdtF34Flash.m_QueryBase + 7;

  RMI4PrintF01QueryInfo();

  RMI4setFlashAddrForDifFormat();
}

void RMI4WritePage(void)
{
  // Write page
  unsigned char uPage = 0x00;
  unsigned char uF01_RMI_Data[2];
  unsigned char m_uStatus;

  m_ret = synaptic_write_blockdata(0xff, &uPage, 1);
  RMI4CheckIfFatalError(m_ret);

  do
  {
    m_ret = synaptic_read_blockdata(0, &m_uStatus, 1);
    RMI4CheckIfFatalError(m_ret);

    if(m_uStatus & 0x40)
    {
      m_bFlashProgOnStartup = true;
    }

    if(m_uStatus & 0x80)
    {
      m_bUnconfigured = true;
      break;
    }

    printk("m_uStatus is 0x%x\n", m_uStatus);
  } while(m_uStatus & 0x40);

  if(m_bFlashProgOnStartup && ! m_bUnconfigured) {
    printk("Bootloader running\n");
  } else if(m_bUnconfigured) {
    printk("UI running\n");
  }

  RMI4ReadPageDescriptionTable();

  if(m_PdtF34Flash.m_ID == 0) {
    printk("Function $34 is not supported\n");
    RMI4CheckIfFatalError( EErrorFunctionNotSupported );
  } else {
      printk("Function $34 addresses Control base:$%02x Query base: $%02x.\n", m_PdtF34Flash.m_ControlBase, m_PdtF34Flash.m_QueryBase);
  }

  if(m_PdtF01Common.m_ID == 0) {
    printk("Function $01 is not supported\n");
    m_PdtF01Common.m_ID = 0x01;
    m_PdtF01Common.m_DataBase = 0;
    RMI4CheckIfFatalError( EErrorFunctionNotSupported );
  } else {
      printk("Function $01 addresses Control base:$%02x Query base: $%02x.\n", m_PdtF01Common.m_ControlBase, m_PdtF01Common.m_QueryBase);
  }

  // Get device status
  m_ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase, &uF01_RMI_Data[0], sizeof(uF01_RMI_Data));
  RMI4CheckIfFatalError(m_ret);

  // Check Device Status
  printk("Configured: %s\n", uF01_RMI_Data[0] & 0x80 ? "false" : "true");
  printk("FlashProg:  %s\n", uF01_RMI_Data[0] & 0x40 ? "true" : "false");
  printk("StatusCode: 0x%x \n", uF01_RMI_Data[0] & 0x0f );
}

// This function gets config block count and config block size
void RMI4ReadConfigInfo(void)
{
  unsigned char uData[2];

  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_ConfigBlockSize, &uData[0], 2);

  RMI4CheckIfFatalError(m_ret);

  m_configBlockSize = uData[0] | (uData[1] << 8);

  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_ConfigBlockCount, &uData[0], 2);

  RMI4CheckIfFatalError(m_ret);

  m_configBlockCount = uData[0] | (uData[1] << 8);
  m_configImgSize = m_configBlockSize*m_configBlockCount;
  
  printk("m_configBlockSize, m_configBlockCount: %d, %d\n", m_configBlockSize, m_configBlockCount);
}

int RMI4IssueEnableFlashCommand(void)
{
  return synaptic_write_blockdata(m_uF34Reflash_FlashControl, (unsigned char *)&s_uF34ReflashCmd_Enable, 1);
}

EError RMI4ReadBootloadID(void)
{
  char uData[2];
  m_ret = synaptic_read_blockdata(m_uF34ReflashQuery_BootID, (unsigned char *)uData, 2);

  m_BootloadID = (unsigned int)uData[0] + (unsigned int)uData[1]*0x100;

  return m_ret;
}

unsigned short GetConfigSize(void)
{
  return m_configBlockSize*m_configBlockCount;
}

unsigned short GetFirmwareSize(void)
{
  return m_firmwareBlockSize*m_firmwareBlockCount;
}


void SpecialCopyEndianAgnostic(unsigned char *dest, unsigned short src)
{
  dest[0] = src%0x100;  //Endian agnostic method
  dest[1] = src/0x100;
}

int RMI4WriteBootloadID(void)
{
  unsigned char uData[2];

  SpecialCopyEndianAgnostic(uData, m_BootloadID);
  return synaptic_write_blockdata(m_uF34Reflash_BlockData, uData, 2);
}

int SynaWaitForATTN(int msec)
{
    int is_timeout = 1;
    unsigned long timeout = msecs_to_jiffies(msec);
  
    while (time_before(jiffies, timeout)) {
        if (!gpio_get_value(S5PV210_GPH0(7))) { 
            is_timeout = 0;
            synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);
            break;
        } else {
            /*msleep(1);*/
            schedule();
        }
    }
    
    if (!gpio_get_value(S5PV210_GPH0(7))) { 
        is_timeout = 0;
        synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);
    }

    if (is_timeout) {
        printk("%s timeout for wait %d msec\n",  __func__,msec);
    }

    return is_timeout;
}

void SynaWaitForATTNPoll(int errorCount)
{
  int uErrorCount = 0;
  int ret;
  unsigned char uStatus;
  while (uErrorCount < errorCount) {
    mdelay(5);
    ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &uStatus, 1);
    if (ret < 0) {
        printk("%s read int_Status error:%d\n", __func__, ret);
    }
    if (uStatus & 0x3 || uErrorCount > errorCount) {
        break;
    } else {
        printk("%s int_Status:0x%x\n", __func__, uStatus);
        ++ uErrorCount;
    }
  }
  return;
}

// Wait for ATTN assertion and see if it's idle and flash enabled
int RMI4WaitATTN(int msec)
{
  int uErrorCount = 0;
  int errorCount = 300;
  int ret = 0;

  if (SynaWaitForATTN(msec)) {
      ret = 1;
  }

  mdelay(2);

  do {
    m_ret = synaptic_read_blockdata(m_uF34Reflash_FlashControl, &m_uPageData[0], 1);

    // To work around the physical address error from control bridge
    // The default check count value is 3. But the value is larger for erase condition
    if((m_ret < 0) && uErrorCount < errorCount) {
      mdelay(100);
      uErrorCount++;
      m_uPageData[0] = 0;
      continue;
    } else {
      uErrorCount++;
    }

    RMI4CheckIfFatalError(m_ret);

    // Clear the attention assertion by reading the interrupt status register
    m_ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);
    RMI4CheckIfFatalError(m_ret);

    if ( m_uPageData[0] != s_uF34ReflashCmd_NormalResult ) {
        WARN(1, "flash status:0x%x", m_uPageData[0]);
        printk("%s  m_uPageData[0]:0x%x\n", __func__, m_uPageData[0]);
        mdelay(1);
        ret = 2;
    }

  } while( m_uPageData[0] != s_uF34ReflashCmd_NormalResult && uErrorCount < errorCount);

  return ret;

}

// Enable Flashing programming
int RMI4EnableFlashing(void)
{
  int uErrorCount = 0;
  int ret = 0;

  // Read bootload ID
  m_ret = RMI4ReadBootloadID();
  RMI4CheckIfFatalError(m_ret);

  // Write bootID to block data registers
  if (RMI4WriteBootloadID() < 0) {
     return -1; 
  }

  do {
    m_ret = synaptic_read_blockdata(m_uF34Reflash_FlashControl, &m_uPageData[0], 1);

    // To deal with ASIC physic address error from cdciapi lib when device is busy and not available for read
    if((m_ret < 0) && uErrorCount < 300) {
      uErrorCount++;
      m_uPageData[0] = 0;
      continue;
    }

    RMI4CheckIfFatalError(m_ret);

    // Clear the attention assertion by reading the interrupt status register
    m_ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);

    RMI4CheckIfFatalError(m_ret);
    if (m_uPageData[0] & 0x0f) {
        printk("%s %d, m_uPageData:0x%x\n", __func__, __LINE__, m_uPageData[0]);
        uErrorCount++;
        mdelay(1);
    }
  } while(((m_uPageData[0] & 0x0f) != 0x00) && (uErrorCount <= 300));

  if (((m_uPageData[0] & 0x0f) != 0x00) && (uErrorCount == 300)) {
      return -2;
  }

  // Issue Enable flash command
  if ( RMI4IssueEnableFlashCommand() < 0) {
      return -3;
  };
  if (RMI4WaitATTN(8000)) {
      return -4;
  }

  RMI4ReadPageDescriptionTable();

  return ret;
}

unsigned long ExtractLongFromHeader(const unsigned char* SynaImage)  // Endian agnostic
{
  return((unsigned long)SynaImage[0] +
         (unsigned long)SynaImage[1]*0x100 +
         (unsigned long)SynaImage[2]*0x10000 +
         (unsigned long)SynaImage[3]*0x1000000);
}

void RMI4CalculateChecksum(unsigned short * data, unsigned short len, unsigned long * dataBlock)
{
  unsigned long temp = *data++;
  unsigned long sum1;
  unsigned long sum2;
  
  *dataBlock = 0xffffffff;

  sum1 = *dataBlock & 0xFFFF;
  sum2 = *dataBlock >> 16;

  while (len--)
  {
    sum1 += temp;
    sum2 += sum1;
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);

    temp = *data ++;
  }

  *dataBlock = sum2 << 16 | sum1;
}

int RMI4ReadFirmwareHeader(void)
{
  unsigned long checkSumCode;
  int ret = 0;

  /*m_fileSize = sizeof(SynaFirmware) -1;*/

  printk("\nScanning SynaFirmware[], the auto-generated C Header File - len = %ld \n\n", m_fileSize);

  checkSumCode         = ExtractLongFromHeader(&(SynaFirmware[0]));
  m_bootloadImgID      = (unsigned int)SynaFirmware[4] + (unsigned int)SynaFirmware[5]*0x100;
  m_firmwareImgVersion = SynaFirmware[7];
  m_firmwareImgSize    = ExtractLongFromHeader(&(SynaFirmware[8]));
  m_configImgSize      = ExtractLongFromHeader(&(SynaFirmware[12]));

  printk("Target = %s, ",&SynaFirmware[16]);
  printk("Cksum = 0x%lX, Id = 0x%X, Ver = %d, FwSize = 0x%lX, ConfigSize = 0x%lX \n", checkSumCode, m_bootloadImgID, m_firmwareImgVersion, m_firmwareImgSize, m_configImgSize);

  RMI4ReadFirmwareInfo();   // Determine firmware organization - read firmware block size and firmware size

  RMI4CalculateChecksum((unsigned short*)&(SynaFirmware[4]), (unsigned short)(m_fileSize-4)>>1, &m_FirmwareImgFile_checkSum);

  printk("SynaFirmware[] fw chksum:0x%lX, computed chksum: 0x%lX\n", checkSumCode, m_FirmwareImgFile_checkSum);
  if (m_FirmwareImgFile_checkSum != checkSumCode) {
      printk("Error:  SynaFirmware[] invalid checksum, computed: 0x%lX\n", m_FirmwareImgFile_checkSum);
      ret = -1;
  }

    printk("SynaFirmware[] size = 0x%lX, expected 0x%lX\n", m_fileSize, (0x100+m_firmwareImgSize+m_configImgSize));
  if (m_fileSize != (0x100+m_firmwareImgSize+m_configImgSize))
  {
    printk("Error: SynaFirmware[] size = 0x%lX, expected 0x%lX\n", m_fileSize, (0x100+m_firmwareImgSize+m_configImgSize));
    ret = -2;
  }

  printk("Firmware image size: size in image 0x%lX, in device size 0x%X\n", m_firmwareImgSize, GetFirmwareSize());
  if (m_firmwareImgSize != GetFirmwareSize())
  {
    printk("Firmware image size verfication failed, size in image 0x%lX did not match device size 0x%X\n", m_firmwareImgSize, GetFirmwareSize());
    ret = -3;
  }

    printk("Configuration size: size in image 0x%lX , in device size 0x%X\n", m_configImgSize, GetConfigSize());
  if (m_configImgSize != GetConfigSize())
  {
    printk("Configuration size verfication failed, size in image 0x%lX did not match device size 0x%X\n", m_configImgSize, GetConfigSize());
    ret = -4;
  }

  m_firmwareImgData=(unsigned char *)((&SynaFirmware[0])+0x100);

  // memcpy(m_firmwareImgData, (&SynaFirmware[0])+0x100, m_firmwareImgSize);
  /*memcpy(m_configImgData,   (&SynaFirmware[0])+0x100+m_firmwareImgSize, m_configImgSize);*/
  m_configImgData = (unsigned char*) ( (&SynaFirmware[0]) + 0x100 + m_firmwareImgSize);

  synaptic_read_blockdata(m_uF34Reflash_FlashControl, &m_uPageData[0], 1);

  return ret;
}

void RMI4ConfigBlockCount(void)
{
    m_totalBlockCount = m_firmwareBlockCount + m_configBlockCount;
    m_currentBlockCount = 0;
}

void RMI4ReflashPercent(void)
{
    int percent = (100*m_currentBlockCount/m_totalBlockCount + 100)%100;
    printk("percent:%3d\r", percent);
}


int RMI4FlashFirmwareWrite(void)
{
  unsigned char *puFirmwareData = m_firmwareImgData;
  unsigned char uData[2];
  unsigned short uBlockNum;
  int ret = 0;

  printk("Flash Firmware starts\n");

  for(uBlockNum = 0; uBlockNum < m_firmwareBlockCount; ++uBlockNum)
  {
    /*printk("%s will reflash firmware at block:%d\n", __func__, uBlockNum);*/
    uData[0] = uBlockNum & 0xff;
    uData[1] = (uBlockNum & 0xff00) >> 8;
    
    /*mdelay(20);*/
    // Write Block Number
    m_ret = synaptic_write_blockdata(m_uF34Reflash_BlockNum, &uData[0], 2);
    RMI4CheckIfFatalError(m_ret);

    // Write Data Block
    m_ret = synaptic_write_blockdata(m_uF34Reflash_BlockData, puFirmwareData, m_firmwareBlockSize);
    RMI4CheckIfFatalError(m_ret);

    // Issue Write Firmware Block command
    uData[0] = 2;
    m_ret = synaptic_write_blockdata(m_uF34Reflash_FlashControl, &uData[0], 1);
    RMI4CheckIfFatalError(m_ret);
    mdelay(1);

    // ret = Wait ATTN. Read Flash Command register and check error
    ret = RMI4WaitATTN(5000);
    if (ret) {  // wait intr timeout
        printk("%s uBlockNum:%d wait intr timeout! ret:%d\n", __func__,  uBlockNum, ret);
        -- uBlockNum;
        /*return ret;   // return when error return;*/
    } else {
        // Move to next data block
        puFirmwareData += m_firmwareBlockSize;
        RMI4ReflashPercent();
        ++ m_currentBlockCount;
    }
  }

  printk("Flash Firmware done\n");

  return ret;
}

int RMI4ValidateBootloadID(unsigned short bootloadID)
{
  printk("In RMI4ValidateBootloadID\n");
  m_ret = RMI4ReadBootloadID();
  RMI4CheckIfFatalError(m_ret);
  printk("Bootload ID of device: %X, input bootID: %X\n", m_BootloadID, bootloadID);

  // check bootload ID against the value found in firmware--but only for image file format version 0
  return m_firmwareImgVersion != 0 || bootloadID == m_BootloadID;
}

int RMI4IssueEraseCommand(unsigned char *command)
{
  // command = 3 - erase all; command = 7 - erase config
  int ret = synaptic_write_blockdata(m_uF34Reflash_FlashControl, command, 1);

  return ret;
}

int RMI4ProgramFirmware(void)
{
  int ret = 0;
  unsigned char uData[1];

  if ( !RMI4ValidateBootloadID(m_bootloadImgID) ) {
      printk("%s: bootloadID Valify failed!\n", __func__);
      RMI4CheckIfFatalError( EErrorBootID );
      return -1;
  }

  // Write bootID to data block register
  if (RMI4WriteBootloadID() < 0) {
      return -2;
  }

  // Issue the firmware and configuration erase command
  uData[0]=3;
  ret = RMI4IssueEraseCommand(&uData[0]);
  RMI4CheckIfFatalError(m_ret);
  RMI4WaitATTN(5000);

  // Write firmware image
  ret = RMI4FlashFirmwareWrite();
  RMI4CheckIfFatalError(ret);
  return ret;
}

EError RMI4IssueFlashControlCommand( unsigned char *command)
{
  m_ret = synaptic_write_blockdata(m_uF34Reflash_FlashControl, command, 1);
  return m_ret;
}

//**************************************************************
// This function write config data to device one block at a time
//**************************************************************
void RMI4ProgramConfiguration(void)
{
  unsigned char uData[2];
  unsigned char *puData = m_configImgData;
  unsigned short blockNum;
  int ret = 0;

  printk("%s will reflash firmware configuration.\n", __func__);

  for(blockNum = 0; blockNum < m_configBlockCount; blockNum++)
  {
    /*printk("%s will reflash firmware at block:%d\n", __func__, blockNum);*/
    SpecialCopyEndianAgnostic(&uData[0], blockNum);

    /*mdelay(20);*/
    // Write Configuration Block Number
    m_ret = synaptic_write_blockdata(m_uF34Reflash_BlockNum, &uData[0], 2);
    RMI4CheckIfFatalError(m_ret);

    // Write Data Block
    m_ret = synaptic_write_blockdata(m_uF34Reflash_BlockData, puData, m_configBlockSize);
    RMI4CheckIfFatalError(m_ret);

    // Issue Write Configuration Block command to flash command register
    uData[0] = s_uF34ReflashCmd_ConfigWrite;
    m_ret = RMI4IssueFlashControlCommand(&uData[0]);
    RMI4CheckIfFatalError(m_ret);

    // Wait for ATTN
    ret = RMI4WaitATTN(5000);
    if (ret) {  // wait intr timeout
        printk("%s uBlockNum:%d wait intr timeout! ret:%d\n", __func__,  blockNum, ret);
        -- blockNum;
    } else {
        puData += m_configBlockSize;
        RMI4ReflashPercent();
        ++ m_currentBlockCount;
    }
  }
  
  printk("%s reflash firmware configuration finished!\n", __func__);

}

//**************************************************************
// Issue a reset ($01) command to the $F01 RMI command register
// This tests firmware image and executes it if it's valid
//**************************************************************
int RMI4ResetDevice(void)
{
  unsigned short m_uF01DeviceControl_CommandReg = m_PdtF01Common.m_CommandBase;
  unsigned char uData[1];

  uData[0] = 1;
  return synaptic_write_blockdata(m_uF01DeviceControl_CommandReg, &uData[0], 1);
}

int RMI4DisableFlash(int msec)
{
  unsigned char uData[2];
  unsigned int uErrorCount = 0;
  int ret = 0;
  int is_timeout = 1;
	unsigned long timeout;

  ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);
  
  if (ret < 0) {
      printk("%s clear intr bit error!\n", __func__);
  }
 
  mdelay(500);
  do {
      // Issue a reset command
      ret = RMI4ResetDevice();
      if (ret < 0) {
          printk("RMI4ResetDevice error!\n");
          mdelay(100);
      }
  } while (0);
  // Wait for ATTN to be asserted to see if device is in idle state
  SynaWaitForATTN(2000);
  SynaWaitForATTN(2000);

  do {
    ret = synaptic_read_blockdata(m_uF34Reflash_FlashControl, &m_uPageData[0], 1);

    // To work around the physical address error from control bridge
    if((ret < 0) && uErrorCount < 300) {
      uErrorCount++;
      m_uPageData[0] = 0;
      msleep(10);
      continue;
    } else if (m_uPageData[0] & 0x0f) {
        printk("RMI4WaitATTN after errorCount loop, uErrorCount=%d, m_uPageData:0x%2x\n", uErrorCount, m_uPageData[0]);
        msleep(10);
    }

  /*} while(((m_uPageData[0] & 0x0f) != 0x00) && (uErrorCount <= 300));*/
  } while(((m_uPageData[0] & 0x0f) != 0x00) );

  // Clear the attention assertion by reading the interrupt status register
  ret = synaptic_read_blockdata(m_PdtF01Common.m_DataBase + 1, &m_uStatus, 1);

  mdelay(10);
  // Read F01 Status flash prog, ensure the 6th bit is '0'
  printk("Checking if reflash finished!\n");
	
  timeout = jiffies + msecs_to_jiffies(msec);
  
  while (time_before(jiffies, timeout)) {
      synaptic_read_blockdata(m_uF01RMI_DataBase, &uData[0], 1);
      if (uData[0] & 0x40) {
          msleep(10);
      } else {
          is_timeout = 0;
          break;
      }
  }
  if (is_timeout) {
      printk("%s can't exit reflash mode!!\n", __func__);
      return -6;
  }
     
  printk("Reflash finished!\n");

  // With a new flash image the page description table could change
  RMI4ReadPageDescriptionTable();

  return 0;
}

int RMI4VerifyFirmwareVer(unsigned char* img_ver)
{
    unsigned char product_id_ver[15] = {0,};
    unsigned char ver;

    synaptic_read_blockdata(m_uF01RMI_QueryBase+11, product_id_ver, 10);
    synaptic_read_blockdata(m_uF01RMI_QueryBase+3, &ver, 1);
    sprintf(&(product_id_ver[10]), "%02x", ver);
    printk("img_ver:%s, product_id_ver:%s\n", img_ver, product_id_ver);
    return strncmp(img_ver, product_id_ver, strlen(product_id_ver));
}

int RMI4Reflash(struct synaptics_rmi4* ts, const unsigned char* img_buf, int img_len)
{
    unsigned char* buf = NULL;
    int try_count = 0;
    int ret = 0;

    struct wake_lock reflash_wakelock;
    
    g_ts = ts;

    wake_lock_init(&reflash_wakelock, WAKE_LOCK_SUSPEND, "synaptic_reflash_wakelock");
    
    is_synaptic_reflash_rw = 0;
    
    spin_lock_init(&rw_lock);
    
    RMI4WritePage();
    
    buf = kmalloc(80*1024, GFP_KERNEL);
    if (buf == NULL) {
        printk("alloc 80k byte buf error!\n");
        ret = -2;
        goto error1;
    }
    
    memcpy(buf, img_buf, img_len);

    buf[img_len] = 0xff;

    SynaFirmware = buf;

    m_fileSize = img_len;

    spin_lock_init(&intr_lock);

    RMI4ReadConfigInfo();

    RMI4ReadFirmwareInfo();

    RMI4ConfigBlockCount();

    RMI4setFlashAddrForDifFormat();

    if ((ret = RMI4ReadFirmwareHeader()) < 0) {
        printk("%s Not a valid img file!! ret code:%d\n", __func__, ret);
        ret = -4;
        goto error2;
    }

try_enter_reflash_again:    
    if (RMI4EnableFlashing() < 0) {
        printk("%s can't enable reflash!!\n", __func__);
        if (try_count < 3) {
            ++ try_count;
            goto try_enter_reflash_again;
        }
        ret = -6;
        goto error2;
    }

    RMI4ProgramFirmware();

    RMI4ProgramConfiguration();

    if (RMI4DisableFlash(10000) < 0) {
        ret = -6;
    } else {
        ret = 0;
    };

    goto out;

error2:
    /*RMI4DisableFlash(1000);*/
error1:
out:
    if (buf != NULL) {
        kfree(buf);
    }
    wake_lock_destroy(&reflash_wakelock);
    return ret;
}
