// TI File $Revision: /main/7 $
// Checkin $Date: January 20, 2005   10:05:26 $
//###########################################################################
//
// FILE:    CAN_Boot.c
//
// TITLE:   280x CAN Boot mode routines
//
// Functions:
//
//     Uint32 CAN_Boot(void)
//     void CAN_Init(void)
//     Uint32 CAN_GetWordData(void)
//
// Notes:
// BRP = 2, Bit time = 10. This would yield the following bit rates with the
// default PLL setting:
// XCLKIN = 40 MHz	SYSCLKOUT = 20 MHz	Bit rate = 1 Mbits/s
// XCLKIN = 20 MHz	SYSCLKOUT = 10 MHz	Bit rate = 500 kbits/s
// XCLKIN = 10 MHz	SYSCLKOUT =  5 MHz	Bit rate = 250 kbits/s
// XCLKIN =  5 MHz	SYSCLKOUT = 2.5MHz	Bit rate = 125 kbits/s
//###########################################################################
// $TI Release:$
// $Release Date:$
//###########################################################################

#include "DSP2803x_Device.h"
#include "Boot.h"

/*---- Flash API include file -------------------------------------------------*/
#include "Flash2803x_API_Library.h"

#define BOOT_MODE_ADDR	(0x7FC)
#define BOOT_KEY_WORD1	(0x4142)
#define BOOT_KEY_WORD2	(0x4B53)
#define BOOT_KEY_WORD3	(0x5543)
#define BOOT_KEY_WORD4	(0x4B53)
#define FLASH_STAT_ADDR	(0x3F6000)
#define FLASH_SUCCESS	(0xAAAA)

#define DELAY_US(A)  DSP28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)

#define RAM_START		((Uint16*)0x00000480)
#define OTP_END			((Uint16*)0x3D7BFD)
//#define OTP_END			((Uint16*)0x93FD)

#define LOAD_ADDRESS_ON_FAIL	(0x3D7820)

// Private functions
Uint32 CAN_Boot(void);
void CAN_Init(void);
Uint16 CAN_GetWordData(void);
void CopyToRam(Uint16 * ramAddr, Uint16 * otpAddr);
Uint32 Bootload(void);

// External functions
/*
extern void CopyData(void);
extern Uint32 GetLongData(void);
extern void ReadReservedFn(void);
*/
extern void InitSysCtrl();


// Reserve boot pass addresses
#pragma DATA_SECTION(bootPass, "BootPass");
const Uint32 bootPass = 0x0;
#pragma DATA_SECTION(bootPass2, "BootPass");
const Uint32 bootPass2 = 0x0;

#pragma DATA_SECTION(bootKey, "KeyVal");
const Uint16 bootKey = KEY_VAL;

#pragma DATA_SECTION(bootMode, "BootMode");
const Uint16 bootMode = OTP_BOOT;

//#################################################
// Uint32 CAN_Boot(void)
//--------------------------------------------
// This module is the main CAN boot routine.
// It will load code via the CAN-A port.
//
// It will return a entry point address back
// to the InitBoot routine which in turn calls
// the ExitBoot routine.
//--------------------------------------------

#pragma CODE_SECTION(CAN_Boot, ".OTP_INIT")
Uint32 CAN_Boot()
{
	Uint16 *ramAddr = RAM_START;
	Uint16 *otpAddr = (Uint16 *)&Bootload;

	if (*((Uint16 *) FLASH_STAT_ADDR) == FLASH_SUCCESS)
	{
		Uint16 * modeAddr = (Uint16 *) BOOT_MODE_ADDR;
		if (*modeAddr != BOOT_KEY_WORD1)
		{
		   return FLASH_ENTRY_POINT;
		}
		modeAddr++;

		if (*modeAddr != BOOT_KEY_WORD2)
		{
		   return FLASH_ENTRY_POINT;
		}
		modeAddr++;

		if (*modeAddr != BOOT_KEY_WORD3)
		{
		   return FLASH_ENTRY_POINT;
		}
		modeAddr++;

		if (*modeAddr != BOOT_KEY_WORD4)
		{
		   return FLASH_ENTRY_POINT;
		}
	}

	InitSysCtrl();

	// Use RAMM1 (Starting address 0x400) for bootloading
	CopyToRam(ramAddr, otpAddr);


   EALLOW;
   SysCtrlRegs.WDCR = 0x0068;	// Disable watchdog module

   // If the missing clock detect bit is set, just
   // loop here.
   if(SysCtrlRegs.PLLSTS.bit.MCLKSTS == 1)
   {
      for(;;);
   }

   CAN_Init();

   // Point to the bootload function now in RAM
   Uint32 (*bootload)(void) = (RAM_START);
   // Jump to bootload
   Uint32 returnAddr = (*bootload)();
   if (returnAddr == LOAD_ADDRESS_ON_FAIL)
   {
	   asm("   B #0xFFFFFFAF, UNC");
   }
   return returnAddr;
}


//#################################################
// void CAN_Init(void)
//----------------------------------------------
// Initialize the CAN-A port for communications
// with the host.
//----------------------------------------------

#pragma CODE_SECTION(CAN_Init, ".OTP_INIT")
void CAN_Init()
{

/* Create a shadow register structure for the CAN control registers. This is
 needed, since, only 32-bit access is allowed to these registers. 16-bit access
 to these registers could potentially corrupt the register contents. This is
 especially true while writing to a bit (or group of bits) among bits 16 - 31 */

   struct ECAN_REGS ECanaShadow;

   EALLOW;
/* Enable CAN clock  */
   SysCtrlRegs.PCLKCR0.bit.ECANAENCLK=1;

/* Configure eCAN-A pins using GPIO regs*/
   GpioCtrlRegs.GPAMUX2.bit.GPIO30 = 1; // GPIO30 is CANRXA
   GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 1; // GPIO31 is CANTXA

/* Configure eCAN RX and TX pins for eCAN transmissions using eCAN regs*/
   ECanaShadow.CANTIOC.all = ECanaRegs.CANTIOC.all;
   ECanaShadow.CANTIOC.bit.TXFUNC = 1;
   ECanaRegs.CANTIOC.all = ECanaShadow.CANTIOC.all;

   ECanaShadow.CANRIOC.all = ECanaRegs.CANRIOC.all;
   ECanaShadow.CANRIOC.bit.RXFUNC = 1;
   ECanaRegs.CANRIOC.all = ECanaShadow.CANRIOC.all;

/* Enable internal pullups for the CAN pins  */
   GpioCtrlRegs.GPAPUD.bit.GPIO30 = 0;
   GpioCtrlRegs.GPAPUD.bit.GPIO31 = 0;

/* Asynch Qual */
   GpioCtrlRegs.GPAQSEL2.bit.GPIO30 = 3;


   ECanaMboxes.MBOX0.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX1.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX2.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX3.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX4.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX5.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX6.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX7.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX8.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX9.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX10.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX11.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX12.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX13.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX14.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX15.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX16.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX17.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX18.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX19.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX20.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX21.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX22.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX23.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX24.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX25.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX26.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX27.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX28.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX29.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX30.MSGCTRL.all = 0x00000000;
   ECanaMboxes.MBOX31.MSGCTRL.all = 0x00000000;

// RMPn, GIFn bits are all zero upon reset and are cleared again
//	as a matter of precaution.

    ECanaRegs.CANTA.all = 0xFFFFFFFF;   /* Clear all TAn bits */

/* Clear all RMPn bits */

   ECanaRegs.CANRMP.all = 0xFFFFFFFF;

/* Clear all interrupt flag bits */

   ECanaRegs.CANGIF0.all = 0xFFFFFFFF;
   ECanaRegs.CANGIF1.all = 0xFFFFFFFF;

/* Configure bit timing parameters for eCANA*/

   ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
   ECanaShadow.CANMC.bit.CCR = 1 ;            // Set CCR = 1
   ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;



   do
   {
     ECanaShadow.CANES.all = ECanaRegs.CANES.all;
   } while(ECanaShadow.CANES.bit.CCE != 1 );    // Wait for CCE bit to be set..

   ECanaShadow.CANBTC.all = 0;
   ECanaShadow.CANBTC.bit.BRPREG = 1;
   ECanaShadow.CANBTC.bit.TSEG1REG = 10;
   ECanaShadow.CANBTC.bit.TSEG2REG = 2;
   ECanaShadow.CANBTC.bit.SJWREG = 1;
   ECanaShadow.CANBTC.bit.SAM = 0;
   ECanaRegs.CANBTC.all = ECanaShadow.CANBTC.all;

   ECanaShadow.CANMC.all = ECanaRegs.CANMC.all;
   ECanaShadow.CANMC.bit.CCR = 0 ;            // Set CCR = 0
   ECanaRegs.CANMC.all = ECanaShadow.CANMC.all;


   do
   {
     ECanaShadow.CANES.all = ECanaRegs.CANES.all;
   } while(ECanaShadow.CANES.bit.CCE != 0 );  // Wait for CCE bit to be cleared..


/* Disable all Mailboxes  */

   ECanaRegs.CANME.all = 0;		// Required before writing the MSGIDs

/* Assign MSGID to MBOX1 */
   ECanaMboxes.MBOX1.MSGID.all = 0x00040000;
   ECanaMboxes.MBOX2.MSGID.all = 0x00080000;

/* Configure MBOX1 to be a receive MBOX */
/* Configure MBOX2 to be a transmit MBOX */
   ECanaRegs.CANMD.all = 0x0002;

/* Enable MBOX1 and MBOX2 */

   ECanaRegs.CANME.all = 0x0006;



   return;
}


//#################################################
// Uint16 CAN_GetWordData(void)
//-----------------------------------------------
// This routine fetches two bytes from the CAN-A
// port and puts them together to form a single
// 16-bit value.  It is assumed that the host is
// sending the data in the order LSB followed by MSB.
//-----------------------------------------------

/*
#pragma CODE_SECTION(CAN_GetWordData, ".OTP")
Uint16 CAN_GetWordData()
{
   Uint16 wordData;
   Uint16 byteData;

   wordData = 0x0000;
   byteData = 0x0000;

// Fetch the LSB
   while(ECanaRegs.CANRMP.all == 0) { }
   wordData =  (Uint16) ECanaMboxes.MBOX1.MDL.byte.BYTE0;	// LS byte

   // Fetch the MSB

   byteData =  (Uint16)ECanaMboxes.MBOX1.MDL.byte.BYTE1;	// MS byte

   // form the wordData from the MSB:LSB
   wordData |= (byteData << 8);


	ECanaRegs.CANRMP.all = 0xFFFFFFFF;

   return wordData;
}
*/

#pragma CODE_SECTION(CopyToRam, ".OTP_INIT")
void CopyToRam(Uint16 * ramAddr, Uint16 * otpAddr)
{
	Uint16 words = OTP_END - otpAddr;
	Uint16 i;

	for(i = 0; i < words; i++)
	{
		*ramAddr = *otpAddr;
		ramAddr++;
		otpAddr++;
	}

}


#pragma CODE_SECTION(Bootload, ".OTP")
Uint32 Bootload(void)
{
	Uint32 EntryAddr;
	EALLOW;

	Uint32 wordData;
	Uint16 byteData;
	Uint16 count = 0;

	struct HEADER {
	Uint16 BlockSize;
	Uint32 DestAddr;
	} BlockHeader;

	FLASH_ST FlashStatus;

/*------------------------------------------------------------------
  Initialize Flash_CPUScaleFactor.

   Flash_CPUScaleFactor is a 32-bit global variable that the flash
   API functions use to scale software delays. This scale factor
   must be initialized to SCALE_FACTOR by the user's code prior
   to calling any of the Flash API functions. This initialization
   is VITAL to the proper operation of the flash API functions.

   SCALE_FACTOR is defined in Example_Flash2803x_API.h as
	 #define SCALE_FACTOR  1048576.0L*( (200L/CPU_RATE) )

   This value is calculated during the compile based on the CPU
   rate, in nanoseconds, at which the algorithms will be run.
------------------------------------------------------------------*/
	EALLOW;
	Flash_CPUScaleFactor = SCALE_FACTOR;
	EDIS;

/*------------------------------------------------------------------
Initialize Flash_CallbackPtr.

Flash_CallbackPtr is a pointer to a function.  The API uses
this pointer to invoke a callback function during the API operations.
If this function is not going to be used, set the pointer to NULL
NULL is defined in <stdio.h>.
------------------------------------------------------------------*/
	EALLOW;
	Flash_CallbackPtr = 0;
	EDIS;

	ECanaRegs.CANMC.all = 2 | (0x100);
	ECanaMboxes.MBOX2.MSGID.bit.IDE = 0; 	//standard id
	ECanaMboxes.MBOX2.MSGID.bit.AME = 0; 	// all bit must match
	ECanaMboxes.MBOX2.MSGID.bit.AAM = 0; 	//RTR AUTO TRANSMIT
	ECanaMboxes.MBOX2.MSGCTRL.bit.DLC = 8;
	ECanaMboxes.MBOX2.MSGID.bit.STDMSGID = 0x2;

	ECanaRegs.CANMC.all = 2;

	if (Flash_Erase(SECTOR_F2803x, &FlashStatus) != 0)
	{
		ECanaRegs.CANMC.all = 2 | (0x100);
		ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
		ECanaMboxes.MBOX2.MDL.all =  0xFFFE;
		ECanaRegs.CANMC.all = 2;
		ECanaRegs.CANTRS.all = 0x4;

		while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
		ECanaRegs.CANTA.all = 0x4;   // Clear all TAn
		return LOAD_ADDRESS_ON_FAIL;
	}



	Uint16 i;
	// Read and discard the 8 reserved words.
	for(i = 0; i < 12; i++)
	{
		wordData = 0x0000;
		byteData = 0x0000;
		Uint32 delay = 0;
		// Fetch the LSB
		while(ECanaRegs.CANRMP.all == 0) {

			delay++;
		    if (delay >= 3000000) {
				ECanaRegs.CANTRS.all = 0x4;

				while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
				ECanaRegs.CANTA.all = 0x4;   // Clear all TAn
				delay = 0;
		    }
		}
		count++;
		if (count != ECanaMboxes.MBOX1.MDL.word.HI_WORD)
		{
			ECanaRegs.CANMC.all = 2 | (0x100);
			ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
			ECanaMboxes.MBOX2.MDL.all =  0xFFFF;
			ECanaRegs.CANMC.all = 2;
			ECanaRegs.CANTRS.all = 0x4;

			while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
			ECanaRegs.CANTA.all = 0x4;   // Clear all TAn

			return LOAD_ADDRESS_ON_FAIL;
		}

		wordData =  (Uint16) ECanaMboxes.MBOX1.MDL.byte.BYTE2;	// LS byte
		byteData =  (Uint16)ECanaMboxes.MBOX1.MDL.byte.BYTE3;	// MS byte

		// form the wordData from the MSB:LSB
		wordData |= (byteData << 8);

		if (i == 0)
		{
			// If the KeyValue was invalid, abort the load
			// and return the flash entry point.
			if (wordData != 0x08AA)
			{
				ECanaRegs.CANMC.all = 2 | (0x100);
				ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
				ECanaMboxes.MBOX2.MDL.all =  0xFFFD;
				ECanaRegs.CANMC.all = 2;
				ECanaRegs.CANTRS.all = 0x4;

				while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
				ECanaRegs.CANTA.all = 0x4;   // Clear all TAn
				return LOAD_ADDRESS_ON_FAIL;
			}
		}
		// Fetch the upper 1/2 of the EntryAddr
		if (i == 9)
		{
			EntryAddr = wordData << 16;
		}
		// Fetch the lower 1/2 of the EntryAddr
		if (i == 10)
		{
			EntryAddr |= wordData;
		}

		if (i == 11)
		{
			// Get the size in words of the first block
			BlockHeader.BlockSize = wordData;
		}
		/* Clear all RMPn bits */
		ECanaRegs.CANRMP.all = 0xFFFFFFFF;
	}

	/*
	* ==================================================================
	*  Copy program data section
	* ==================================================================
	*/

	// While the block size is > 0 copy the data
	// to the DestAddr.  There is no error checking
	// as it is assumed the DestAddr is a valid
	// memory location

	while(BlockHeader.BlockSize != (Uint16)0x0000)
	{
		for(i = 0; i < 2; i++)
		{
			wordData = 0x0000;
			byteData = 0x0000;

			// Fetch the LSB
			while(ECanaRegs.CANRMP.all == 0) { }
			count++;
			if (count != ECanaMboxes.MBOX1.MDL.word.HI_WORD)
			{
				ECanaRegs.CANMC.all = 2 | (0x100);
				ECanaMboxes.MBOX2.MDH.all = 0xFFFFFFFF;
				ECanaMboxes.MBOX2.MDL.all = 0xFFFFFFFF;
				ECanaRegs.CANMC.all = 2;
				ECanaRegs.CANTRS.all = 0x4;

				while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
				ECanaRegs.CANTA.all = 0x4;   // Clear all TAn

				return LOAD_ADDRESS_ON_FAIL;
			}
			wordData =  (Uint16) ECanaMboxes.MBOX1.MDL.byte.BYTE2;	// LS byte
			byteData =  (Uint16)ECanaMboxes.MBOX1.MDL.byte.BYTE3;	// MS byte

			// form the wordData from the MSB:LSB
			wordData |= (byteData << 8);

			// Fetch the upper 1/2 of the DestAddr
			if (i == 0)
			{
				BlockHeader.DestAddr = wordData << 16;
			}
			// Fetch the lower 1/2 of the DestAddr
			if (i == 1)
			{
				BlockHeader.DestAddr |= wordData;
			}

			/* Clear all RMPn bits */
			ECanaRegs.CANRMP.all = 0xFFFFFFFF;
		}

		for(i = 1; i <= BlockHeader.BlockSize; i++)
		{
			wordData = 0x0000;
			byteData = 0x0000;

			// Fetch the LSB
			while(ECanaRegs.CANRMP.all == 0) { }
			count ++;
			if (count != ECanaMboxes.MBOX1.MDL.word.HI_WORD)
			{
				ECanaRegs.CANMC.all = 2 | (0x100);
				ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
				ECanaMboxes.MBOX2.MDL.all = 0xFFFF;
				ECanaRegs.CANMC.all = 2;
				ECanaRegs.CANTRS.all = 0x4;

				while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
				ECanaRegs.CANTA.all = 0x4;   // Clear all TAn

				return LOAD_ADDRESS_ON_FAIL;
			}
			wordData =  (Uint16) ECanaMboxes.MBOX1.MDL.byte.BYTE2;	// LS byte

			// Fetch the MSB

			byteData =  (Uint16)ECanaMboxes.MBOX1.MDL.byte.BYTE3;	// MS byte

			// form the wordData from the MSB:LSB
			wordData |= (byteData << 8);

			/* Clear all RMPn bits */
			ECanaRegs.CANRMP.all = 0xFFFFFFFF;

			//*(Uint16 *)BlockHeader.DestAddr++ = wordData;
			if (Flash_Program((Uint16 *) BlockHeader.DestAddr, &wordData, 1, &FlashStatus) != 0)
			{
				ECanaRegs.CANMC.all = 2 | (0x100);
				ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
				ECanaMboxes.MBOX2.MDL.all =  0xFFFC;
				ECanaRegs.CANMC.all = 2;
				ECanaRegs.CANTRS.all = 0x4;

				while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
				ECanaRegs.CANTA.all = 0x4;   // Clear all TAn
				return 0x003d7800;
			}
			BlockHeader.DestAddr++;
		}


		wordData = 0x0000;
		byteData = 0x0000;

		// Fetch the LSB
		while(ECanaRegs.CANRMP.all == 0) { }
		count ++;
		if (count != ECanaMboxes.MBOX1.MDL.word.HI_WORD)
		{
			ECanaRegs.CANMC.all = 2 | (0x100);
			ECanaMboxes.MBOX2.MDH.all = 0xFFFF;
			ECanaMboxes.MBOX2.MDL.all = 0xFFFF;
			ECanaRegs.CANMC.all = 2;
			ECanaRegs.CANTRS.all = 0x4;

			while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
			ECanaRegs.CANTA.all = 0x4;   // Clear all TAn

			return LOAD_ADDRESS_ON_FAIL;
		}

		wordData =  (Uint16) ECanaMboxes.MBOX1.MDL.byte.BYTE2;	// LS byte
		byteData =  (Uint16)ECanaMboxes.MBOX1.MDL.byte.BYTE3;	// MS byte

		// form the wordData from the MSB:LSB
		wordData |= (byteData << 8);

		BlockHeader.BlockSize = wordData;


		/* Clear all RMPn bits */
		ECanaRegs.CANRMP.all = 0xFFFFFFFF;

	}

	Uint16 * modeAddr = (Uint16 *) BOOT_MODE_ADDR;
	for (i = 0; i < 4; i++)
	{
		*modeAddr++ = 0;
	}

	wordData = FLASH_SUCCESS;
	Flash_Program(((Uint16 *) FLASH_STAT_ADDR), &wordData, 1, &FlashStatus);

	ECanaRegs.CANMC.all = 2 | (0x100);
	ECanaMboxes.MBOX2.MDH.all = 0x0000;
	ECanaMboxes.MBOX2.MDL.all = 0x8000;
	ECanaRegs.CANMC.all = 2;
	ECanaRegs.CANTRS.all = 0x4;

	while(ECanaRegs.CANTA.all != 0x4 ) {}  // Wait for all TAn bits to be set..
	ECanaRegs.CANTA.all = 0x4;   // Clear all TAn

	EALLOW;
	SysCtrlRegs.WDCR = 0x0028; // Enable watchdog module
	SysCtrlRegs.WDKEY = 0x55;  // Clear the WD counter
	SysCtrlRegs.WDKEY = 0xAA;
	EDIS;

	return EntryAddr;
}

/*
Data frames with a Standard MSGID of 0x1 should be transmitted to the ECAN-A bootloader.
This data will be received in Mailbox1, whose MSGID is 0x1. No message filtering is employed.

Transmit only 2 bytes at a time, LSB first and MSB next. For example, to transmit
the word 0x08AA to the 280x, transmit AA first, followed by 08. Following is the
order in which data should be transmitted:
AA 08	-	Keyvalue
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
00 00	-	Part of 8 reserved words stream
bb aa	-	MS part of 32-bit address (aabb)
dd cc	-	LS part of 32-bit address (ccdd) - Final Entry-point address = 0xaabbccdd
nn mm	-	Length of first section (mm nn)
ff ee	-	MS part of 32-bit address (eeff)
hh gg	-	LS part of 32-bit address (gghh) - Entry-point address of first section = 0xeeffgghh
xx xx	-   First word of first section
xx xx	-	Second word......
...
...
...
xxx		- 	Last word of first section
nn mm	-	Length of second section (mm nn)
ff ee	-	MS part of 32-bit address (eeff)
hh gg	-	LS part of 32-bit address (gghh) - Entry-point address of second section = 0xeeffgghh
xx xx	-   First word of second section
xx xx	-	Second word......
...
...
...
xxx		- 	Last word of second section
(more sections, if need be)
00 00	- 	Section length of zero for next section indicates end of data.
*/

/*
Notes:
------
OTP boot to CAN bootloader changes:
1. Employed Shadow reads to CANES register (CCE bit). Disable
   watchdog in CAN bootloader.
*/
// EOF-------
