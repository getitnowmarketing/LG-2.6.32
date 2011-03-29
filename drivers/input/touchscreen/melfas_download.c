//--------------------------------------------------------
//
//	MELFAS Firmware download base code for MCS6000
//	Version : v01
//	Date	: 2009.01.20
//
//  Modified by dangwoo.choi (Elini)
//--------------------------------------------------------

#include "melfas_download.h"
#ifdef LGE_MELFAS_UPDATE_ENABLE

//============================================================
//
//	Static variables & functions
//
//============================================================

//---------------------------------
//	Downloading functions
//---------------------------------
static int  mcsdl_download(const UINT8 *pData, const UINT16 nLength);

static int  mcsdl_enter_download_mode(void);
static void mcsdl_write_download_mode_signal(void);

static int  mcsdl_i2c_erase_flash(void);
static int  mcsdl_i2c_prepare_erase_flash(void);
static int  mcsdl_i2c_read_flash( UINT8 *pBuffer, UINT16 nAddr_start, UINT8 cLength);
static int  mcsdl_i2c_program_info(void);
static int  mcsdl_i2c_program_flash( UINT8 *pData, UINT16 nAddr_start, UINT8 cLength );

//---------------------------------
//	Download enable command
//---------------------------------
#ifdef MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
void melfas_send_download_enable_command(void);
#endif

//---------------------------------
//	I2C Functions
//---------------------------------
BOOLEAN _i2c_read_( UINT8 slave_addr, UINT8 *pData, UINT8 cLength);
BOOLEAN _i2c_write_(UINT8 slave_addr, UINT8 *pData, UINT8 cLength);

//---------------------------------
//	Delay functions
//---------------------------------
static void mcsdl_delay(unsigned long nCount);

//---------------------------------
//	For debugging display
//---------------------------------
#if MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet);
#endif


//----------------------------------
// Download enable command
//----------------------------------
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD

void melfas_send_download_enable_command(void)
{
	// TO DO : Fill this up

}

#endif


//============================================================
//
//	Include MELFAS Binary source code File ( ex> MELFAS_FIRM_bin.c)
//
//	Warning!!!!
//		.c file included at next must not be inserted to Souce project.
//		Just #include One file. !!
//
//	'7000' is a sample of Model name.
//
//============================================================

//#include "MELFAS_7000_R01_V02_bin.c"
//#include "MELFAS_7000_R01_V03_bin.c"
//#include "MELFAS_7000_R02_V04_bin.c"
//#include "MELFAS_7000_R02_V05_bin.c"
//#include "MTH_LGW535_RAF2_VAA2_bin.c"



//============================================================
//
//	<ain Download furnction
//
//============================================================

extern void MELFAS_BASEBAND_ISR(UINT8 enable);
extern void MELFAS_WATCHDOG_RESET(UINT8 enable);		
extern void TKEY_VDD_SET(UINT8 set);
extern void TKEY_INTR_SET(UINT8 set);
extern void TKEY_I2C_SET(UINT8 dest, UINT8 set);
extern BOOLEAN lge_melfas_i2c_read(UINT8, UINT8*, UINT8);
extern BOOLEAN lge_melfas_i2c_write(UINT8, UINT8*, UINT8);

extern const unsigned short MELFAS_binary_nLength;
extern const unsigned char MELFAS_binary[];

int mcsdl_download_binary_data(void)
{
	int ret;

	#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD

	melfas_send_download_enable_command();

	mcsdl_delay(MCSDL_DELAY_100US);

	#endif

	MELFAS_DISABLE_BASEBAND_ISR();					// Disable Baseband touch interrupt ISR.
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();			// Disable Baseband watchdog timer

	//------------------------
	// Run Download
	//------------------------
	ret = mcsdl_download( (const UINT8*) MELFAS_binary, (const UINT16)MELFAS_binary_nLength );

	MELFAS_ROLLBACK_BASEBAND_ISR();					// Roll-back Baseband touch interrupt ISR.
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();			// Roll-back Baseband watchdog timer

	#if MELFAS_ENABLE_DBG_PRINT

		mcsdl_print_result( ret );

	#endif


	return ( ret == MCSDL_RET_SUCCESS );
}



int mcsdl_download_binary_file(void)
{
	int ret;

	UINT8  *pData = NULL;
	UINT16 nBinary_length =0;


	//==================================================
	//
	//	Porting section 7. File process
	//
	//	1. Read '.bin file'
	//	2. Run mcsdl_download_binary_data();
	//
	//==================================================

	#if 0

		// TO DO : File Process & Get file Size(== Binary size)
		//			This is just a simple sample

		FILE *fp;
		INT  nRead;

		//------------------------------
		// Open a file
		//------------------------------

		if( fopen( fp, "MELFAS_FIRM.bin", "rb" ) == NULL ){
			return MCSDL_RET_FILE_ACCESS_FAILED;
		}

		//------------------------------
		// Get Binary Size
		//------------------------------

		fseek( fp, 0, SEEK_END );

		nBinary_length = (UINT16)ftell(fp);

		//------------------------------
		// Memory allocation
		//------------------------------

		pData = (UINT8*)malloc( (INT)nBinary_length + (nBinary_length%2) );

		if( pData == NULL ){

			return MCSDL_RET_FILE_ACCESS_FAILED;
		}

		//------------------------------
		// Read binary file
		//------------------------------

		fseek( fp, 0, SEEK_SET );

		nRead = fread( pData, 1, (INT)nBinary_length, fp );		// Read binary file

		if( nRead != (INT)nBinary_length ){

			fclose(fp);												// Close file

			if( pData != NULL )										// free memory alloced.
				free(pData);

			return MCSDL_RET_FILE_ACCESS_FAILED;
		}

		//------------------------------
		// Close file
		//------------------------------

		fclose(fp);

	#endif

	if( pData != NULL && nBinary_length > 0 && nBinary_length < 62*1024 ){

		MELFAS_DISABLE_BASEBAND_ISR();					// Disable Baseband touch interrupt ISR.
		MELFAS_DISABLE_WATCHDOG_TIMER_RESET();			// Disable Baseband watchdog timer

		ret = mcsdl_download( (const UINT8 *)pData, (const UINT16)nBinary_length );

		MELFAS_ROLLBACK_BASEBAND_ISR();					// Roll-back Baseband touch interrupt ISR.
		MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();			// Roll-back Baseband watchdog timer

	}else{

		ret = MCSDL_RET_WRONG_PARAMETER;
	}

	#if MELFAS_ENABLE_DBG_PRINT

	mcsdl_print_result( ret );

	#endif

	#if 0
		if( pData != NULL )										// free memory alloced.
			free(pData);
	#endif

	return ( ret == MCSDL_RET_SUCCESS );

}

//------------------------------------------------------------------
//
//	Download function
//
//------------------------------------------------------------------

static int mcsdl_download(const UINT8 *pData, const UINT16 nLength )
{
	int		i;
	int		nRet;

	UINT8   cLength;
	UINT16  nStart_address=0;

	UINT8	buffer[MELFAS_TRANSFER_LENGTH];

	UINT8	*pOriginal_data;

	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "Starting download...\n");
	#endif

	//--------------------------------------------------------------
	//
	// Enter Download mode
	//
	//--------------------------------------------------------------
	nRet = mcsdl_enter_download_mode();
	if( nRet != MCSDL_RET_SUCCESS ) 
		goto MCSDL_DOWNLOAD_FINISH;


	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'




	//--------------------------------------------------------------
	//
	// Check H/W Revision
	//
	// Don't download firmware, if Module H/W revision does not match.
	//
	//--------------------------------------------------------------
	#if MELFAS_DISABLE_DOWNLOAD_IF_MODULE_VERSION_DOES_NOT_MATCH

		#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		uart_printf("Checking module revision...\n");
		#endif

		pOriginal_data  = (UINT8 *)pData;

		nRet = mcsdl_i2c_read_flash( buffer, MCSDL_ADDR_MODULE_REVISION, 4 );

		if( nRet != MCSDL_RET_SUCCESS )
			goto MCSDL_DOWNLOAD_FINISH;

		if( 	(pOriginal_data[MCSDL_ADDR_MODULE_REVISION+1] != buffer[1])
			||	(pOriginal_data[MCSDL_ADDR_MODULE_REVISION+2] != buffer[2]) ){

			nRet = MCSDL_RET_WRONG_MODULE_REVISION;
			goto MCSDL_DOWNLOAD_FINISH;
		}

		mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'

	#endif



	//--------------------------------------------------------------
	//
	// Erase Flash
	//
	//--------------------------------------------------------------
	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "Erasing...\n");
	#endif

	nRet = mcsdl_i2c_prepare_erase_flash();

	if( nRet != MCSDL_RET_SUCCESS ){
		goto MCSDL_DOWNLOAD_FINISH;
	}

	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'

	nRet = mcsdl_i2c_erase_flash();

	if( nRet != MCSDL_RET_SUCCESS ){
		goto MCSDL_DOWNLOAD_FINISH;
	}

	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'


	//---------------------------
	//
	// Verify erase
	//
	//---------------------------
	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "Verify Erasing...\n");
	#endif

	nRet = mcsdl_i2c_read_flash( buffer, 0x00, 16 );

	if( nRet != MCSDL_RET_SUCCESS )
		goto MCSDL_DOWNLOAD_FINISH;

	for(i=0; i<16; i++){

		if( buffer[i] != 0xFF ){

			nRet = MCSDL_RET_ERASE_VERIFY_FAILED;
			goto MCSDL_DOWNLOAD_FINISH;
		}
	}

	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'


	//-------------------------------
	//
	// Program flash information
	//
	//-------------------------------
	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "Program information...\n");
	#endif

	nRet = mcsdl_i2c_program_info();

	if( nRet != MCSDL_RET_SUCCESS )
		goto MCSDL_DOWNLOAD_FINISH;


	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'


   //-------------------------------
   // Program flash
   //-------------------------------

	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "Program flash...  ");
	#endif

	pOriginal_data  = (UINT8 *)pData;

	nStart_address = 0;
	cLength  = MELFAS_TRANSFER_LENGTH;

	for( nStart_address = 0; nStart_address < nLength; nStart_address+=cLength ){

		#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk("#");
		#endif

		if( ( nLength - nStart_address ) < MELFAS_TRANSFER_LENGTH ){
			cLength  = (UINT8)(nLength - nStart_address);

			cLength += (cLength%2);									// For odd length.
		}

		nRet = mcsdl_i2c_program_flash( pOriginal_data, nStart_address, cLength );

        if( nRet != MCSDL_RET_SUCCESS ){

			#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
			printk(KERN_INFO "Program flash failed position : 0x%x / nRet : 0x%x ", nStart_address, nRet);
			#endif

            goto MCSDL_DOWNLOAD_FINISH;
		}

		pOriginal_data  += cLength;

		mcsdl_delay(MCSDL_DELAY_500US);					// Delay '500 usec'

	}


	//-------------------------------
	//
	// Verify flash
	//
	//-------------------------------

	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n");
	printk(KERN_INFO "Verify flash...   ");
	#endif

	pOriginal_data  = (UINT8 *) pData;

	nStart_address = 0;

	cLength  = MELFAS_TRANSFER_LENGTH;

	for( nStart_address = 0; nStart_address < nLength; nStart_address+=cLength ){

		#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk("#");
		#endif

		if( ( nLength - nStart_address ) < MELFAS_TRANSFER_LENGTH ){
			cLength = (UINT8)(nLength - nStart_address);

			cLength += (cLength%2);									// For odd length.
		}

		//--------------------
		// Read flash
		//--------------------
		nRet = mcsdl_i2c_read_flash( buffer, nStart_address, cLength );

		//--------------------
		// Comparing
		//--------------------
		for(i=0; i<(int)cLength; i++){

			if( buffer[i] != pOriginal_data[i] ){

				#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				printk(KERN_INFO "\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", nStart_address, pOriginal_data[i], buffer[i] );

                #endif

				nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
				goto MCSDL_DOWNLOAD_FINISH;

			}
		}

		pOriginal_data += cLength;

		mcsdl_delay(MCSDL_DELAY_500US);					// Delay '500 usec'
	}

	#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(KERN_INFO "\n");
	#endif

	nRet = MCSDL_RET_SUCCESS;


MCSDL_DOWNLOAD_FINISH :

	mcsdl_delay(MCSDL_DELAY_1MS);					// Delay '1 msec'

	//---------------------------
	//	Reset command
	//---------------------------
	buffer[0] = MCSDL_ISP_CMD_RESET;

	_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, buffer, 1 );

    TKEY_INTR_SET_INPUT();

    mcsdl_delay(MCSDL_DELAY_45MS);							// Delay about '200 msec'
    mcsdl_delay(MCSDL_DELAY_45MS);
    mcsdl_delay(MCSDL_DELAY_45MS);
    mcsdl_delay(MCSDL_DELAY_45MS);

	return nRet;

}


//------------------------------------------------------------------
//
//   Enter Download mode ( MDS ISP or I2C ISP )
//
//------------------------------------------------------------------
static int mcsdl_enter_download_mode(void)
{
	BOOLEAN	bRet;
	int    	nRet = MCSDL_RET_ENTER_DOWNLOAD_MODE_FAILED;

	UINT8 cData=0;

	//--------------------------------------------
	// Tkey module reset
	//--------------------------------------------

	TKEY_VDD_SET_LOW();

	TKEY_CE_SET_LOW();
	TKEY_CE_SET_OUTPUT();

	TKEY_I2C_CLOSE();

	TKEY_INTR_SET_LOW();
	TKEY_INTR_SET_OUTPUT();

	TKEY_RESETB_SET_LOW();
	TKEY_RESETB_SET_OUTPUT();

	mcsdl_delay(MCSDL_DELAY_45MS);					// Delay about '100 msec'
	mcsdl_delay(MCSDL_DELAY_45MS);

	TKEY_VDD_SET_HIGH();

	TKEY_CE_SET_HIGH();

	TKEY_I2C_SDA_SET_HIGH();

	mcsdl_delay(MCSDL_DELAY_25MS); 						// Delay '25 msec'

	//-------------------------------
	// Write 1st signal
	//-------------------------------
	mcsdl_write_download_mode_signal();

	mcsdl_delay(MCSDL_DELAY_1MS); 						// Delay '2 msec'
	mcsdl_delay(MCSDL_DELAY_1MS);

	//-------------------------------
	// Check response
	//-------------------------------

	bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &cData, 1 );

	if( bRet != TRUE || cData != MCSDL_I2C_SLAVE_READY_STATUS ){

		printk(KERN_INFO "mcsdl_enter_download_mode() returns - ret : 0x%x & cData : 0x%x\n", nRet, cData);
		goto MCSDL_ENTER_DOWNLOAD_MODE_FINISH;
	}

	nRet = MCSDL_RET_SUCCESS;

	//-----------------------------------
	// Entering MDS ISP mode finished.
	//-----------------------------------

MCSDL_ENTER_DOWNLOAD_MODE_FINISH:

   return nRet;
}

//--------------------------------------------
//
//   Write ISP Mode entering signal
//
//--------------------------------------------
static void mcsdl_write_download_mode_signal(void)
{
	int    i;

	UINT8 enter_code[14] = { 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1 };

	//---------------------------
	// ISP mode signal 0
	//---------------------------

	for(i=0; i<14; i++){

		if( enter_code[i] )	{

			TKEY_RESETB_SET_HIGH();
			TKEY_INTR_SET_HIGH();

		}else{

			TKEY_RESETB_SET_LOW();
			TKEY_INTR_SET_LOW();
		}

		TKEY_I2C_SCL_SET_HIGH();	mcsdl_delay(MCSDL_DELAY_15US);
		TKEY_I2C_SCL_SET_LOW();

		TKEY_RESETB_SET_LOW();
		TKEY_INTR_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_100US);

   }

	TKEY_I2C_SCL_SET_HIGH();

	mcsdl_delay(MCSDL_DELAY_100US);

	TKEY_INTR_SET_HIGH();
	TKEY_RESETB_SET_HIGH();
}


//--------------------------------------------
//
//   Prepare Erase flash
//
//--------------------------------------------
static int mcsdl_i2c_prepare_erase_flash(void)
{
	int   nRet = MCSDL_RET_PREPARE_ERASE_FLASH_FAILED;

	int i;
	BOOLEAN   bRet;

	UINT8 i2c_buffer[4] = {	MCSDL_ISP_CMD_ERASE_TIMING,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_0,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_1,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_2   };
	UINT8	ucTemp;

   //-----------------------------
   // Send Erase Setting code
   //-----------------------------

   for(i=0; i<4; i++){

		bRet = _i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );

		if( !bRet ){

			goto MCSDL_I2C_PREPARE_ERASE_FLASH_FINISH;
		}

		mcsdl_delay(MCSDL_DELAY_15US);
   }

   //-----------------------------
   // Read Result
   //-----------------------------

	mcsdl_delay(MCSDL_DELAY_500US);                  		// Delay 500usec

	bRet = _i2c_read_(MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if( bRet && ucTemp == MCSDL_ISP_ACK_PREPARE_ERASE_DONE ){

		nRet = MCSDL_RET_SUCCESS;

	}


MCSDL_I2C_PREPARE_ERASE_FLASH_FINISH :

   return nRet;

}

//--------------------------------------------
//
//   Erase flash
//
//--------------------------------------------
static int mcsdl_i2c_erase_flash(void)
{
	int   nRet = MCSDL_RET_ERASE_FLASH_FAILED;

	UINT8 	i;
	BOOLEAN	bRet;

	UINT8 	i2c_buffer[1] = {	MCSDL_ISP_CMD_ERASE};
	UINT8 	ucTemp;

   //-----------------------------
   // Send Erase code
   //-----------------------------

   for(i=0; i<1; i++){

		bRet = _i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );

		if( !bRet )
			goto MCSDL_I2C_ERASE_FLASH_FINISH;

		mcsdl_delay(MCSDL_DELAY_15US);
   }

   //-----------------------------
   // Read Result
   //-----------------------------

	mcsdl_delay(MCSDL_DELAY_45MS);                  			// Delay 45ms


	bRet = _i2c_read_(MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if( bRet && ucTemp == MCSDL_ISP_ACK_ERASE_DONE ){

		nRet = MCSDL_RET_SUCCESS;

	}


MCSDL_I2C_ERASE_FLASH_FINISH :

   return nRet;

}

//--------------------------------------------
//
//   Read flash
//
//--------------------------------------------
static int mcsdl_i2c_read_flash( UINT8 *pBuffer, UINT16 nAddr_start, UINT8 cLength)
{
	int nRet = MCSDL_RET_READ_FLASH_FAILED;

	int     i;
	BOOLEAN   bRet;
	UINT8   cmd[4];
	UINT8   ucTemp;

	//-----------------------------------------------------------------------------
	// Send Read Flash command   [ Read code - address high - address low - size ]
	//-----------------------------------------------------------------------------

	cmd[0] = MCSDL_ISP_CMD_READ_FLASH;
	cmd[1] = (UINT8)((nAddr_start >> 8 ) & 0xFF);
	cmd[2] = (UINT8)((nAddr_start      ) & 0xFF);
	cmd[3] = cLength;

	for(i=0; i<4; i++){

		bRet = _i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &cmd[i], 1 );

		mcsdl_delay(MCSDL_DELAY_15US);

		if( bRet == FALSE )
			goto MCSDL_I2C_READ_FLASH_FINISH;

   }

	//----------------------------------
	// Read 'Result of command'
	//----------------------------------
	bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if( !bRet || ucTemp != MCSDL_MDS_ACK_READ_FLASH){

		goto MCSDL_I2C_READ_FLASH_FINISH;
	}

	//----------------------------------
	// Read Data  [ pCmd[3] == Size ]
	//----------------------------------
	for(i=0; i<(int)cmd[3]; i++){

		mcsdl_delay(MCSDL_DELAY_100US);                  // Delay about 100us

		bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, pBuffer++, 1 );
             //LGE_UPDATE_S/sunkyo.park/kick dog fatal/ALESSI
              if((i%0x4000)==0) msleep(1);
		//LGE_UPDATE_E
		if( bRet == FALSE && i!=(int)(cmd[3]-1) )
			goto MCSDL_I2C_READ_FLASH_FINISH;
	}

	nRet = MCSDL_RET_SUCCESS;


MCSDL_I2C_READ_FLASH_FINISH :

	return nRet;
}


//--------------------------------------------
//
//   Program information
//
//--------------------------------------------
static int mcsdl_i2c_program_info(void)
{

	int nRet = MCSDL_RET_PREPARE_PROGRAM_FAILED;

	int i;
	BOOLEAN bRet;

	UINT8 i2c_buffer[4] = { MCSDL_ISP_CMD_PROGRAM_TIMING,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_0,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_1,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_2};

	//------------------------------------------------------
	//   Write Program timing information
	//------------------------------------------------------
	for(i=0; i<4; i++){

			bRet = _i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );

			if( bRet == FALSE )
				goto MCSDL_I2C_PREPARE_PROGRAM_FINISH;

			mcsdl_delay(MCSDL_DELAY_15US);
	}

	mcsdl_delay(MCSDL_DELAY_500US);                     // delay about  500us

	//------------------------------------------------------
	//   Read command's result
	//------------------------------------------------------
	/*
	//Cowon 에서의 Download문제 발생으로 확인필요.
	    bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, i2c_buffer, 1 );

    if( bRet == FALSE || i2c_buffer[0] != MCSDL_I2C_ACK_PREPARE_PROGRAM)
        goto MCSDL_I2C_PREPARE_PROGRAM_FINISH;

    */
	bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[4], 1 );

	if( bRet == FALSE || i2c_buffer[4] != MCSDL_I2C_ACK_PREPARE_PROGRAM)
		goto MCSDL_I2C_PREPARE_PROGRAM_FINISH;

	mcsdl_delay(MCSDL_DELAY_100US);                     // delay about  100us

   nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_PREPARE_PROGRAM_FINISH :

   return nRet;

}

//--------------------------------------------
//
//   Program Flash
//
//--------------------------------------------

static int mcsdl_i2c_program_flash( UINT8 *pData, UINT16 nAddr_start, UINT8 cLength )
{
	int nRet = MCSDL_RET_PROGRAM_FLASH_FAILED;

	int     i,j;
	BOOLEAN   bRet;
	UINT8    cData;
	UINT8    tmp;
	UINT8    cmd[4];


	//-----------------------------
	// Send Read code
	//-----------------------------

	cmd[0] = MCSDL_ISP_CMD_PROGRAM_FLASH;
	cmd[1] = (UINT8)((nAddr_start >> 8 ) & 0xFF);
	cmd[2] = (UINT8)((nAddr_start      ) & 0xFF);
	cmd[3] = cLength;

	for(i=0; i<4; i++){

		bRet = _i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &cmd[i], 1 );

		mcsdl_delay(MCSDL_DELAY_15US);

		if( bRet == FALSE )
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;

	}
	//-----------------------------
	// Check command result
	//-----------------------------

	bRet = _i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &cData, 1 );

	if( bRet == FALSE || cData != MCSDL_MDS_ACK_PROGRAM_FLASH ){

		goto MCSDL_I2C_PROGRAM_FLASH_FINISH;
	}


	//-----------------------------
	// Program Data
	//-----------------------------

	mcsdl_delay(MCSDL_DELAY_150US);                  // Delay about 150us

	for(i=0; i<(int)cmd[3]; i+=2){

		bRet = _i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &pData[i+1], 1 );

		if( bRet == FALSE )
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;

		mcsdl_delay(MCSDL_DELAY_100US);                  // Delay about 150us

		bRet = _i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &pData[i], 1 );

		mcsdl_delay(MCSDL_DELAY_150US);                  // Delay about 150us
              //LGE_UPDATE_S/sunkyo.park/kick dog fatal/ALESSI
              if((i%0x4000)==0) msleep(1);
		//LGE_UPDATE_E
		if( bRet == FALSE )
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;

	}


	nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_PROGRAM_FLASH_FINISH :

   return nRet;
}


//============================================================
//
//	Delay Function
//
//============================================================
static void mcsdl_delay(unsigned long nCount)
{

	#if 1

		//delay_usec(nCount);			// Baseband delay function
		udelay(nCount);

	#else

		UINT32 i;

		for(i=0;i<nCount;i++){

		}

	#endif
}


//============================================================
//
//	Porting section 6.	I2C function calling
//
//	Connect baseband i2c function
//
//	Warning 1. !!!!  Burst mode is not supported. Transfer 1 byte Only.
//
//		Every i2c packet has to
//			" START > Slave address > One byte > STOP " at download mode.
//
//	Warning 2. !!!!  Check return value of i2c function.
//
//		_i2c_read_(), _i2c_write_() must return
//			TRUE (1) if success,
//			FALSE(0) if failed.
//
//		If baseband i2c function returns different value, convert return value.
//			ex> baseband_return = baseband_i2c_read( slave_addr, pData, cLength );
//				return ( baseband_return == BASEBAND_RETURN_VALUE_SUCCESS );
//
//
//	Warning 3. !!!!  Check Slave address
//
//		Slave address is '0x7F' at download mode. ( Diffrent with Normal touch working mode )
//		'0x7F' is original address,
//			If shift << 1 bit, It becomes '0xFE'
//
//============================================================

BOOLEAN _i2c_read_( UINT8 slave_addr, UINT8 *pData, UINT8 cLength)
{
	BOOLEAN bRet;

	#ifdef USE_BASEBAND_I2C_FUNCTION

		//bRet = baseband_i2c_read( slave_addr, pData, cLength );
		bRet = lge_melfas_i2c_read ( slave_addr, pData, cLength );

	#else

	bRet = FALSE;

	#endif

	return ( bRet == TRUE );
}

BOOLEAN _i2c_write_(UINT8 slave_addr, UINT8 *pData, UINT8 cLength)
{
	BOOLEAN bRet;

	#ifdef USE_BASEBAND_I2C_FUNCTION

		//bRet = baseband_i2c_write( slave_addr, pData, cLength );
		bRet = lge_melfas_i2c_write ( slave_addr, pData, cLength );

	#else

	bRet = FALSE;

	#endif

	return ( bRet == TRUE );
}




//============================================================
//
//	Debugging print functions.
//
//	Change printk(KERN_INFO ) to Baseband printing function
//
//============================================================

#ifdef MELFAS_ENABLE_DBG_PRINT

static void mcsdl_print_result(int nRet)
{
	if( nRet == MCSDL_RET_SUCCESS ){

		printk(KERN_INFO " MELFAS Firmware downloading SUCCESS.\n");

	}else{

		printk(KERN_INFO " MELFAS Firmware downloading FAILED  :  ");

		switch( nRet ){

			case MCSDL_RET_SUCCESS                  		:   printk(KERN_INFO "MCSDL_RET_SUCCESS\n" );                 	break;
			case MCSDL_RET_ENTER_DOWNLOAD_MODE_FAILED   	:   printk(KERN_INFO "MCSDL_RET_ENTER_ISP_MODE_FAILED\n" );      	break;
			case MCSDL_RET_ERASE_FLASH_FAILED           	:   printk(KERN_INFO "MCSDL_RET_ERASE_FLASH_FAILED\n" );         	break;
			case MCSDL_RET_READ_FLASH_FAILED				:   printk(KERN_INFO "MCSDL_RET_READ_FLASH_FAILED\n" );         	break;
			case MCSDL_RET_READ_EEPROM_FAILED           	:   printk(KERN_INFO "MCSDL_RET_READ_EEPROM_FAILED\n" );         	break;
			case MCSDL_RET_READ_INFORMAION_FAILED        	:   printk(KERN_INFO "MCSDL_RET_READ_INFORMAION_FAILED\n" );     	break;
			case MCSDL_RET_PROGRAM_FLASH_FAILED				:   printk(KERN_INFO "MCSDL_RET_PROGRAM_FLASH_FAILED\n" );      	break;
			case MCSDL_RET_PROGRAM_EEPROM_FAILED        	:   printk(KERN_INFO "MCSDL_RET_PROGRAM_EEPROM_FAILED\n" );      	break;
			case MCSDL_RET_PREPARE_PROGRAM_FAILED    		:   printk(KERN_INFO "MCSDL_RET_PROGRAM_INFORMAION_FAILED\n" );  break;
			case MCSDL_RET_PROGRAM_VERIFY_FAILED			:   printk(KERN_INFO "MCSDL_RET_PROGRAM_VERIFY_FAILED\n" );      	break;

			case MCSDL_RET_WRONG_MODE_ERROR             	:   printk(KERN_INFO "MCSDL_RET_WRONG_MODE_ERROR\n" );         	break;
			case MCSDL_RET_WRONG_SLAVE_SELECTION_ERROR		:   printk(KERN_INFO "MCSDL_RET_WRONG_SLAVE_SELECTION_ERROR\n" ); break;
			case MCSDL_RET_COMMUNICATION_FAILED				:   printk(KERN_INFO "MCSDL_RET_COMMUNICATION_FAILED\n" );      	break;
			case MCSDL_RET_READING_HEXFILE_FAILED       	:   printk(KERN_INFO "MCSDL_RET_READING_HEXFILE_FAILED\n" );      break;
			case MCSDL_RET_WRONG_PARAMETER       			:   printk(KERN_INFO "MCSDL_RET_WRONG_PARAMETER\n" );      		break;
			case MCSDL_RET_FILE_ACCESS_FAILED       		:   printk(KERN_INFO "MCSDL_RET_FILE_ACCESS_FAILED\n" );      	break;
			case MCSDL_RET_MELLOC_FAILED     		  		:   printk(KERN_INFO "MCSDL_RET_MELLOC_FAILED\n" );      			break;
			case MCSDL_RET_WRONG_MODULE_REVISION     		:   printk(KERN_INFO "MCSDL_RET_WRONG_MODULE_REVISION\n" );      	break;

			default                             			:	printk(KERN_INFO "UNKNOWN ERROR. [0x%02X].\n", nRet );      	break;
		}

		printk(KERN_INFO "\n");
	}

}

#endif

//============================================================
//
//	Porting section 4-1. Delay function
//
//	For initial testing of delay and gpio control
//
//	You can confirm GPIO control and delay time by calling this function.
//
//============================================================

#if MELFAS_ENABLE_DELAY_TEST


void mcsdl_delay_test(INT32 nCount)
{
	INT16 i;

	MELFAS_DISABLE_BASEBAND_ISR();					// Disable Baseband touch interrupt ISR.
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();			// Disable Baseband watchdog timer

	TKEY_I2C_SET_OUTPUT();
	//--------------------------------
	//	Repeating 'nCount' times
	//--------------------------------

	TKEY_I2C_SCL_SET_HIGH();

	for( i=0; i<nCount; i++ ){

		#if 1

		TKEY_I2C_SCL_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_15US);

		TKEY_I2C_SCL_SET_HIGH();

		mcsdl_delay(MCSDL_DELAY_100US);

		#elif 0

		TKEY_I2C_SCL_SET_LOW();

	   	mcsdl_delay(MCSDL_DELAY_500US);

		TKEY_I2C_SCL_SET_HIGH();

    	mcsdl_delay(MCSDL_DELAY_1MS);

		#else

		TKEY_I2C_SCL_SET_LOW();

    	mcsdl_delay(MCSDL_DELAY_25MS);

		TKEY_INTR_SET_LOW();

    	mcsdl_delay(MCSDL_DELAY_45MS);

		TKEY_INTR_SET_HIGH();

    	#endif
	}

	TKEY_I2C_SCL_SET_HIGH();

	MELFAS_ROLLBACK_BASEBAND_ISR();					// Roll-back Baseband touch interrupt ISR.
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();			// Roll-back Baseband watchdog timer
}


#endif // #ifdef MCSDL_ENABLE_DELAY_TEST
#endif // #if LGE_MELFAS_UPDATE_ENABLE


