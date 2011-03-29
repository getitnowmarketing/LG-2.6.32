//--------------------------------------------------------
//
//	MELFAS Firmware download basic code v5 2008.05.23
//
//  Modified by dangwoo.choi (Elini)
//--------------------------------------------------------

#include <linux/module.h>
#include <linux/delay.h>

//#define LGE_MELFAS_UPDATE_ENABLE 

#ifdef LGE_MELFAS_UPDATE_ENABLE

#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__
#define __MELFAS_FIRMWARE_DOWNLOAD_H__

//============================================================
//
//	Porting section 1. Type define
//
//============================================================

typedef char				INT8;
typedef unsigned char		UINT8;
typedef short				INT16;
typedef unsigned short		UINT16;
typedef int					INT32;
typedef unsigned int		UINT32;
typedef unsigned char		BOOLEAN;


#ifndef TRUE
#define TRUE 				(1==1)
#endif

#ifndef FALSE
#define FALSE 				(1==0)
#endif

#ifndef NULL
#define NULL 				0
#endif

//============================================================
//
//	Porting section 2. Melfas download Options
//
//============================================================


// Disable downlaoding, if module version does not match.
#define MELFAS_DISABLE_DOWNLOAD_IF_MODULE_VERSION_DOES_NOT_MATCH		0

// Enable download enable command with protocol		// Protocol dependent option
//#define MELFAS_ENABLE_DOWNLOAD_ENABLE_COMMAND

// For printing debug information.
#define MELFAS_ENABLE_DBG_PRINT											1
#define MELFAS_ENABLE_DBG_PROGRESS_PRINT								1

// For delay function test. ( Disable after Porting finished )
#define MELFAS_ENABLE_DELAY_TEST										1

//============================================================
//
//	Port setting. ( Melfas preset this value. )
//
//============================================================

#if 1 											// Using Pin
#define MCSDL_USE_VDD_CONTROL
#define MCSDL_USE_INTR_CONTROL
#else											// Don't care.
#define MCSDL_USE_CE_CONTROL
#define MCSDL_USE_RESETB_CONTROL
#endif

//============================================================
//
//	Porting section 3. IO Control poting.
//
//	Fill up 'I2C IO'
//	Fill up 'USE_CONTROL' only on upper setting.
//
//============================================================

//----------------
// VDD
//----------------
#ifdef MCSDL_USE_VDD_CONTROL
#define TKEY_VDD_SET								lge_melfas_vdd
#define TKEY_VDD_SET_LOW()              			TKEY_VDD_SET(0)
#define TKEY_VDD_SET_HIGH()             			TKEY_VDD_SET(1)					//
#else
#define TKEY_VDD_SET_HIGH()             											// Nothing
#define TKEY_VDD_SET_LOW()              											// Nothing
#endif

//----------------
// CE
//----------------
#ifdef MCSDL_USE_CE_CONTROL
#define TKEY_CE_SET_HIGH()   	          			____HERE!_____					//
#define TKEY_CE_SET_LOW()      	        			____HERE!_____					//
#define TKEY_CE_SET_OUTPUT()   	        			____HERE!_____					//
#else
#define TKEY_CE_SET_HIGH()              											// Nothing
#define TKEY_CE_SET_LOW()              												// Nothing
#define TKEY_CE_SET_OUTPUT()              											// Nothing
#endif

#define TKEY_SET_LOW							0
#define TKEY_SET_HIGH							1
#define TKEY_SET_OUTPUT							2
#define TKEY_SET_INPUT							3

//----------------
// INTR
//----------------
#ifdef MCSDL_USE_INTR_CONTROL

#define TKEY_INTR_SET		             			lge_melfas_intr
#define TKEY_INTR_SET_LOW()              			TKEY_INTR_SET(TKEY_SET_LOW)
#define TKEY_INTR_SET_HIGH()             			TKEY_INTR_SET(TKEY_SET_HIGH)
#define TKEY_INTR_SET_OUTPUT()             			TKEY_INTR_SET(TKEY_SET_OUTPUT)
#define TKEY_INTR_SET_INPUT()             			TKEY_INTR_SET(TKEY_SET_INPUT)
#else
#define TKEY_INTR_SET_HIGH()              											// Nothing
#define TKEY_INTR_SET_LOW()            												// Nothing
#define TKEY_TINR_SET_OUTPUT()             											// Nothing
#define TKEY_INTR_SET_INPUT()              											// Nothing
#endif

//----------------
// RESETB
//----------------
#ifdef MCSDL_USE_RESETB_CONTROL
#define TKEY_RESETB_SET_HIGH()             			____HERE!_____					//UHI2C_IRQ_EN(TRUE)
#define TKEY_RESETB_SET_LOW()              			____HERE!_____					//
#define TKEY_RESETB_SET_OUTPUT()           			____HERE!_____					//
#define TKEY_RESETB_SET_INPUT()           			____HERE!_____					//
#else
#define TKEY_RESETB_SET_HIGH()            											// Nothing
#define TKEY_RESETB_SET_LOW()          												// Nothing
#define TKEY_RESETB_SET_OUTPUT()           											// Nothing
#define TKEY_RESETB_SET_INPUT()            											// Nothing
#endif


//------------------
// I2C SCL & SDA
//------------------

#define TKEY_I2C_SCL							0
#define TKEY_I2C_SDA							1

#define TKEY_I2C_SET							lge_melfas_i2c_set	
#define TKEY_I2C_SCL_SET_LOW()					TKEY_I2C_SET(TKEY_I2C_SCL, TKEY_SET_LOW)
#define TKEY_I2C_SCL_SET_HIGH()					TKEY_I2C_SET(TKEY_I2C_SCL, TKEY_SET_HIGH)	
#define TKEY_I2C_SCL_SET_OUTPUT()				TKEY_I2C_SET(TKEY_I2C_SCL, TKEY_SET_OUTPUT)
#define TKEY_I2C_SCL_SET_INPUT()				TKEY_I2C_SET(TKEY_I2C_SCL, TKEY_SET_INPUT)

#define TKEY_I2C_SDA_SET_LOW()					TKEY_I2C_SET(TKEY_I2C_SDA, TKEY_SET_LOW)
#define TKEY_I2C_SDA_SET_HIGH()					TKEY_I2C_SET(TKEY_I2C_SDA, TKEY_SET_HIGH)
#define TKEY_I2C_SDA_SET_OUTPUT()				TKEY_I2C_SET(TKEY_I2C_SDA, TKEY_SET_OUTPUT)
#define TKEY_I2C_SDA_SET_INPUT()				TKEY_I2C_SET(TKEY_I2C_SDA, TKEY_SET_INPUT)

#define TKEY_I2C_SET_HIGH()                  	TKEY_I2C_SCL_SET_HIGH(); 		\
                                    			TKEY_I2C_SDA_SET_HIGH()

#define TKEY_I2C_SET_LOW()						TKEY_I2C_SCL_SET_LOW();  		\
												TKEY_I2C_SDA_SET_LOW()


#define TKEY_I2C_SET_OUTPUT()   	            TKEY_I2C_SCL_SET_OUTPUT();		\
                                    			TKEY_I2C_SDA_SET_OUTPUT()

#define TKEY_I2C_INIT()                     	TKEY_I2C_SET_HIGH();			\
                                    			TKEY_I2C_SET_OUTPUT()

#define TKEY_I2C_CLOSE()                     	TKEY_I2C_SET_LOW();				\
                                    			TKEY_I2C_SET_OUTPUT()





//============================================================
//
//	Porting section 4-2. Delay parameter setting
//
//============================================================
#define MCSDL_DELAY_1US							1
#define MCSDL_DELAY_10US						10
#define MCSDL_DELAY_15US						15
#define MCSDL_DELAY_100US						100
#define MCSDL_DELAY_150US						150
#define MCSDL_DELAY_500US             			500
#define MCSDL_DELAY_1MS							1000
#define MCSDL_DELAY_5MS							5000
#define MCSDL_DELAY_10MS						10000
#define MCSDL_DELAY_15MS						15000
#define MCSDL_DELAY_25MS						25000
#define MCSDL_DELAY_45MS						45000
#define MCSDL_DELAY_500MS						500000


//============================================================
//
//	Porting section 5. Defence External Effect
//
//============================================================
#if 1

#define MELFAS_BASEBAND_ISR							lge_melfas_enable_isr	
#define MELFAS_WATCHDOG_RESET						lge_melfas_watchdog_reset	
#define MELFAS_DISABLE_BASEBAND_ISR()				MELFAS_BASEBAND_ISR(0)
#define MELFAS_DISABLE_WATCHDOG_TIMER_RESET()		MELFAS_WATCHDOG_RESET(0)

#define MELFAS_ROLLBACK_BASEBAND_ISR()				MELFAS_BASEBAND_ISR(1)
#define MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET()		MELFAS_WATCHDOG_RESET(1)

#else

#define MELFAS_DISABLE_BASEBAND_ISR()											//Nothing
#defien MELFAS_DISABLE_WATCHDOG_TIMER_RESET()									//Nothing

#define MELFAS_ROLLBACK_BASEBAND_ISR()											//Nothing
#defien MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET()									//Nothing

#endif


//=====================================================================
//
//   MELFAS Firmware download
//
//=====================================================================

//----------------------------------------------------
//   Return values of download function
//----------------------------------------------------
#define MCSDL_RET_SUCCESS						0x00
#define MCSDL_RET_ENTER_DOWNLOAD_MODE_FAILED	0x01
#define MCSDL_RET_ERASE_FLASH_FAILED			0x02
#define MCSDL_RET_PREPARE_ERASE_FLASH_FAILED		0x0B
#define MCSDL_RET_ERASE_VERIFY_FAILED			0x03
#define MCSDL_RET_READ_FLASH_FAILED				0x04
#define MCSDL_RET_READ_EEPROM_FAILED			0x05
#define MCSDL_RET_READ_INFORMAION_FAILED		0x06
#define MCSDL_RET_PROGRAM_FLASH_FAILED			0x07
#define MCSDL_RET_PROGRAM_EEPROM_FAILED			0x08
#define MCSDL_RET_PREPARE_PROGRAM_FAILED			0x09
#define MCSDL_RET_PROGRAM_VERIFY_FAILED			0x0A

#define MCSDL_RET_WRONG_MODE_ERROR				0xF0
#define MCSDL_RET_WRONG_SLAVE_SELECTION_ERROR	0xF1
#define MCSDL_RET_WRONG_PARAMETER				0xF2
#define MCSDL_RET_COMMUNICATION_FAILED			0xF3
#define MCSDL_RET_READING_HEXFILE_FAILED		0xF4
#define MCSDL_RET_FILE_ACCESS_FAILED			0xF5
#define MCSDL_RET_MELLOC_FAILED					0xF6
#define MCSDL_RET_WRONG_MODULE_REVISION			0xF7


//----------------------------------------------------
// ISP commands
//----------------------------------------------------
#define MCSDL_ISP_CMD_ERASE							0x02
#define MCSDL_ISP_CMD_ERASE_TIMING					0x0F
#define MCSDL_ISP_CMD_PROGRAM_FLASH					0x03
#define MCSDL_ISP_CMD_READ_FLASH					0x04
#define MCSDL_ISP_CMD_PROGRAM_TIMING				0x0F
#define MCSDL_ISP_CMD_READ_INFORMATION				0x06
#define MCSDL_ISP_CMD_RESET							0x07

//------------------------------
// MCS5000's responses
//------------------------------
#define MCSDL_ISP_ACK_ERASE_DONE					0x82
#define MCSDL_ISP_ACK_PREPARE_ERASE_DONE			0x8F
#define MCSDL_I2C_ACK_PREPARE_PROGRAM				0x8F
#define MCSDL_MDS_ACK_PROGRAM_FLASH					0x83
#define MCSDL_MDS_ACK_READ_FLASH					0x84
#define MCSDL_MDS_ACK_PROGRAM_INFORMATION			0x88
#define MCSDL_MDS_ACK_PROGRAM_LOCKED				0xFE
#define MCSDL_MDS_ACK_READ_LOCKED					0xFE
#define MCSDL_MDS_ACK_FAIL							0xFE

#define MCSDL_ISP_PROGRAM_TIMING_VALUE_0			0x00
#define MCSDL_ISP_PROGRAM_TIMING_VALUE_1			0x00
#define MCSDL_ISP_PROGRAM_TIMING_VALUE_2			0x78

#define MCSDL_ISP_ERASE_TIMING_VALUE_0				0x01
#define MCSDL_ISP_ERASE_TIMING_VALUE_1				0xD4
#define MCSDL_ISP_ERASE_TIMING_VALUE_2				0xC0
//------------------------------
//	I2C ISP
//------------------------------

#define MCSDL_I2C_SLAVE_ADDR_ORG				0x7D 							// Original Address

#define MCSDL_I2C_SLAVE_ADDR_SHIFTED			(MCSDL_I2C_SLAVE_ADDR_ORG<<1)	// Adress after sifting.

#define MCSDL_I2C_SLAVE_READY_STATUS			0x55

#define USE_BASEBAND_I2C_FUNCTION					1								// Selection of i2c function ( This must be 1 )

#define MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD 	0								// If 'enable download command' is needed ( Pinmap dependent option ).
//----------------------------------------------------
//	MELFAS Version information address
//----------------------------------------------------
#define MCSDL_ADDR_MODULE_REVISION					0x98
#define MCSDL_ADDR_FIRMWARE_VERSION					0x9C

#define MELFAS_TRANSFER_LENGTH						64								// Program & Read flash block size


//----------------------------------------------------
//	Functions
//----------------------------------------------------

int mcsdl_download_binary_data(void);			// with binary type .c   file.
int mcsdl_download_binary_file(void);			// with binary type .bin file.

#ifdef MELFAS_ENABLE_DELAY_TEST					// For initial porting test.
void mcsdl_delay_test(INT32 nCount);
#endif


#endif		//#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__
#endif 		//#if LGE_MELFAS_UPDATE_ENABLE

