/**
@verbatim

Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.

        ADDI-DATA GmbH
        Dieselstrasse 3
        D-77833 Ottersweier
        Tel: +19(0)7223/9493-0
        Fax: +49(0)7223/9493-92
        http://www.addi-data-com
        info@addi-data.com

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

You shoud also find the complete GPL in the COPYING file accompanying this source code.

@endverbatim
*/
/*

  +-----------------------------------------------------------------------+
  | (C) ADDI-DATA GmbH          Dieselstrasse 3      D-77833 Ottersweier  |
  +-----------------------------------------------------------------------+
  | Tel : +49 (0) 7223/9493-0     | email    : info@addi-data.com         |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
  +-----------------------------------------------------------------------+
  | Project   : ADDI DATA         | Compiler : GCC 			              |
  | Modulname : addi_eeprom.c     | Version  : 2.96                       |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stolz   | Date     :  02/12/2002                |
  +-----------------------------------------------------------------------+
  | Description : ADDI EEPROM  Module                                     |
  +-----------------------------------------------------------------------+
  |                             UPDATE'S                                  |
  +-----------------------------------------------------------------------+
  |   Date   |   Author  |          Description of updates                |
  +----------+-----------+------------------------------------------------+
  |          | 		 | 						  |
  |          |           | 						  |
  +----------+-----------+------------------------------------------------+
*/

#define NVCMD_BEGIN_READ 	(0x7 << 5 )	// nvRam begin read command
#define NVCMD_LOAD_LOW   	(0x4 << 5 )	// nvRam load low command
#define NVCMD_LOAD_HIGH  	(0x5 << 5 )	// nvRam load high command
#define EE76_CMD_LEN    	13	// bits in instructions
#define EE_READ         	0x0180	// 01 1000 0000 read instruction

#define	WORD				unsigned short
#define PWORD				unsigned short *
#define PDWORD				unsigned int  *

#ifndef DWORD
#define	DWORD				unsigned int
#endif

#define EEPROM_DIGITALINPUT 			0
#define EEPROM_DIGITALOUTPUT			1
#define EEPROM_ANALOGINPUT				2
#define EEPROM_ANALOGOUTPUT				3
#define EEPROM_TIMER					4
#define EEPROM_WATCHDOG					5
#define EEPROM_TIMER_WATCHDOG_COUNTER	10

struct str_Functionality {
	BYTE b_Type;
	WORD w_Address;
};

typedef struct {
	WORD w_HeaderSize;
	BYTE b_Nfunctions;
	struct str_Functionality s_Functions[7];
} str_MainHeader;

typedef struct {
	WORD w_Nchannel;
	BYTE b_Interruptible;
	WORD w_NinterruptLogic;
} str_DigitalInputHeader;

typedef struct {
	WORD w_Nchannel;
} str_DigitalOutputHeader;

// used for timer as well as watchdog

typedef struct {
	WORD w_HeaderSize;
	BYTE b_Resolution;
	BYTE b_Mode;		// in case of Watchdog it is functionality
	WORD w_MinTiming;
	BYTE b_TimeBase;
} str_TimerDetails;
typedef struct {

	WORD w_Ntimer;
	str_TimerDetails s_TimerDetails[4];	//  supports 4 timers
} str_TimerMainHeader;

typedef struct {
	WORD w_Nchannel;
	BYTE b_Resolution;
} str_AnalogOutputHeader;

typedef struct {
	WORD w_Nchannel;
	WORD w_MinConvertTiming;
	WORD w_MinDelayTiming;
	BYTE b_HasDma;
	BYTE b_Resolution;
} str_AnalogInputHeader;

		/*****************************************/
		/*            Read Header Functions              */
		/*****************************************/

INT i_EepromReadMainHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, struct comedi_device *dev);

INT i_EepromReadDigitalInputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_DigitalInputHeader * s_Header);

INT i_EepromReadDigitalOutputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_DigitalOutputHeader * s_Header);

INT i_EepromReadTimerHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_TimerMainHeader * s_Header);

INT i_EepromReadAnlogOutputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_AnalogOutputHeader * s_Header);

INT i_EepromReadAnlogInputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_AnalogInputHeader * s_Header);

		/******************************************/
		/*      Eeprom Specific Functions                         */
		/******************************************/
WORD w_EepromReadWord(WORD w_PCIBoardEepromAddress, PCHAR pc_PCIChipInformation,
	WORD w_EepromStartAddress);
VOID v_EepromWaitBusy(WORD w_PCIBoardEepromAddress);
VOID v_EepromClock76(DWORD dw_Address, DWORD dw_RegisterValue);
VOID v_EepromWaitBusy(WORD w_PCIBoardEepromAddress);
VOID v_EepromSendCommand76(DWORD dw_Address, DWORD dw_EepromCommand,
	BYTE b_DataLengthInBits);
VOID v_EepromCs76Read(DWORD dw_Address, WORD w_offset, PWORD pw_Value);

/*
+----------------------------------------------------------------------------+
| Function   Name   : WORD w_EepromReadWord                                  |
|				(WORD	w_PCIBoardEepromAddress,             		 |
|				 PCHAR 	pc_PCIChipInformation,               		 |
|				 WORD   w_EepromStartAddress)                		 |
+----------------------------------------------------------------------------+
| Task              : Read from eepromn a word                               |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|		      WORD w_EepromStartAddress    : Selected eeprom address |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : Read word value from eeprom                            |
+----------------------------------------------------------------------------+
*/

WORD w_EepromReadWord(WORD w_PCIBoardEepromAddress, PCHAR pc_PCIChipInformation,
	WORD w_EepromStartAddress)
{

	BYTE b_Counter = 0;

	BYTE b_ReadByte = 0;

	BYTE b_ReadLowByte = 0;

	BYTE b_ReadHighByte = 0;

	BYTE b_SelectedAddressLow = 0;

	BYTE b_SelectedAddressHigh = 0;

	WORD w_ReadWord = 0;

	/**************************/

	/* Test the PCI chip type */

	/**************************/

	if ((!strcmp(pc_PCIChipInformation, "S5920")) ||
		(!strcmp(pc_PCIChipInformation, "S5933")))
	{

		for (b_Counter = 0; b_Counter < 2; b_Counter++)
		{

			b_SelectedAddressLow = (w_EepromStartAddress + b_Counter) % 256;	//Read the low 8 bit part

			b_SelectedAddressHigh = (w_EepromStartAddress + b_Counter) / 256;	//Read the high 8 bit part

	      /************************************/

			/* Select the load low address mode */

	      /************************************/

			outb(NVCMD_LOAD_LOW, w_PCIBoardEepromAddress + 0x3F);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /************************/

			/* Load the low address */

	      /************************/

			outb(b_SelectedAddressLow,
				w_PCIBoardEepromAddress + 0x3E);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /*************************************/

			/* Select the load high address mode */

	      /*************************************/

			outb(NVCMD_LOAD_HIGH, w_PCIBoardEepromAddress + 0x3F);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /*************************/

			/* Load the high address */

	      /*************************/

			outb(b_SelectedAddressHigh,
				w_PCIBoardEepromAddress + 0x3E);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /************************/

			/* Select the READ mode */

	      /************************/

			outb(NVCMD_BEGIN_READ, w_PCIBoardEepromAddress + 0x3F);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /*****************************/

			/* Read data into the EEPROM */

	      /*****************************/

			b_ReadByte = inb(w_PCIBoardEepromAddress + 0x3E);

	      /****************/

			/* Wait on busy */

	      /****************/

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      /*********************************/

			/* Select the upper address part */

	      /*********************************/

			if (b_Counter == 0)
			{

				b_ReadLowByte = b_ReadByte;

			}	// if(b_Counter==0)

			else
			{

				b_ReadHighByte = b_ReadByte;

			}	// if(b_Counter==0)

		}		// for (b_Counter=0; b_Counter<2; b_Counter++)

		w_ReadWord = (b_ReadLowByte | (((WORD) b_ReadHighByte) * 256));

	}			// end of if ((!strcmp(pc_PCIChipInformation, "S5920")) || (!strcmp(pc_PCIChipInformation, "S5933")))

	if (!strcmp(pc_PCIChipInformation, "93C76"))
	{

	   /*************************************/

		/* Read 16 bit from the EEPROM 93C76 */

	   /*************************************/

		v_EepromCs76Read(w_PCIBoardEepromAddress, w_EepromStartAddress,
			&w_ReadWord);

	}

	return (w_ReadWord);

}

/*

+----------------------------------------------------------------------------+

| Function   Name   : VOID v_EepromWaitBusy                                  |

|			(WORD	w_PCIBoardEepromAddress)                    	 |

+----------------------------------------------------------------------------+

| Task              : Wait the busy flag from PCI controller                 |

+----------------------------------------------------------------------------+

| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom base address |

+----------------------------------------------------------------------------+

| Output Parameters : -                                                      |

+----------------------------------------------------------------------------+

| Return Value      : -                                                      |

+----------------------------------------------------------------------------+

*/

VOID v_EepromWaitBusy(WORD w_PCIBoardEepromAddress)
{

	BYTE b_EepromBusy = 0;

	do
	{

	   /*************/

		/* IMPORTANT */

	   /*************/

	   /************************************************************************/

		/* An error has been written in the AMCC 5933 book at the page B-13 */

		/* Ex: if you read a byte and look for the busy statusEEPROM=0x80 and   */

		/*      the operator register is AMCC_OP_REG_MCSR+3 */

		/*      WORD read  EEPROM=0x8000 andAMCC_OP_REG_MCSR+2                  */

		/*      DWORD read  EEPROM=0x80000000 and AMCC_OP_REG_MCSR */

	   /************************************************************************/

		b_EepromBusy = inb(w_PCIBoardEepromAddress + 0x3F);
		b_EepromBusy = b_EepromBusy & 0x80;

	}
	while (b_EepromBusy == 0x80);

}

/*

+---------------------------------------------------------------------------------+

| Function   Name   : VOID v_EepromClock76(DWORD dw_Address,                      |

|					   DWORD dw_RegisterValue)                 			  |

+---------------------------------------------------------------------------------+

| Task              : This function sends the clocking sequence to the EEPROM.    |

+---------------------------------------------------------------------------------+

| Input Parameters  : DWORD dw_Address : PCI eeprom base address                  |

|		      DWORD dw_RegisterValue : PCI eeprom register value to write.|

+---------------------------------------------------------------------------------+

| Output Parameters : -                                                           |

+---------------------------------------------------------------------------------+

| Return Value      : -                                                           |

+---------------------------------------------------------------------------------+

*/

VOID v_EepromClock76(DWORD dw_Address, DWORD dw_RegisterValue)
{

   /************************/

	/* Set EEPROM clock Low */

   /************************/

	outl(dw_RegisterValue & 0x6, dw_Address);

   /***************/

	/* Wait 0.1 ms */

   /***************/

	udelay(100);

   /*************************/

	/* Set EEPROM clock High */

   /*************************/

	outl(dw_RegisterValue | 0x1, dw_Address);

   /***************/

	/* Wait 0.1 ms */

   /***************/

	udelay(100);

}

/*

+---------------------------------------------------------------------------------+

| Function   Name   : VOID v_EepromSendCommand76(DWORD dw_Address,                |

|					   DWORD   dw_EepromCommand,                		  |

|					   BYTE    b_DataLengthInBits)                        |

+---------------------------------------------------------------------------------+

| Task              : This function sends a Command to the EEPROM 93C76.          |

+---------------------------------------------------------------------------------+

| Input Parameters  : DWORD dw_Address : PCI eeprom base address                  |

|		      DWORD dw_EepromCommand : PCI eeprom command to write.       |

|		      BYTE  b_DataLengthInBits : PCI eeprom command data length.  |

+---------------------------------------------------------------------------------+

| Output Parameters : -                                                           |

+---------------------------------------------------------------------------------+

| Return Value      : -                                                           |

+---------------------------------------------------------------------------------+

*/

VOID v_EepromSendCommand76(DWORD dw_Address, DWORD dw_EepromCommand,
	BYTE b_DataLengthInBits)
{

	CHAR c_BitPos = 0;

	DWORD dw_RegisterValue = 0;

   /*****************************/

	/* Enable EEPROM Chip Select */

   /*****************************/

	dw_RegisterValue = 0x2;

   /********************************************************************/

	/* Toggle EEPROM's Chip select to get it out of Shift Register Mode */

   /********************************************************************/

	outl(dw_RegisterValue, dw_Address);

   /***************/

	/* Wait 0.1 ms */

   /***************/

	udelay(100);

   /*******************************************/

	/* Send EEPROM command - one bit at a time */

   /*******************************************/

	for (c_BitPos = (b_DataLengthInBits - 1); c_BitPos >= 0; c_BitPos--)
	{

      /**********************************/

		/* Check if current bit is 0 or 1 */

      /**********************************/

		if (dw_EepromCommand & (1 << c_BitPos))
		{

	 /***********/

			/* Write 1 */

	 /***********/

			dw_RegisterValue = dw_RegisterValue | 0x4;

		}

		else
		{

	 /***********/

			/* Write 0 */

	 /***********/

			dw_RegisterValue = dw_RegisterValue & 0x3;

		}

      /*********************/

		/* Write the command */

      /*********************/

		outl(dw_RegisterValue, dw_Address);

      /***************/

		/* Wait 0.1 ms */

      /***************/

		udelay(100);

      /****************************/

		/* Trigger the EEPROM clock */

      /****************************/

		v_EepromClock76(dw_Address, dw_RegisterValue);

	}

}

/*

+---------------------------------------------------------------------------------+

| Function   Name   : VOID v_EepromCs76Read(DWORD dw_Address,                     |

|					   WORD    w_offset,                      			  |

|					   PWORD   pw_Value)                      			  |

+---------------------------------------------------------------------------------+

| Task              : This function read a value from the EEPROM 93C76.           |

+---------------------------------------------------------------------------------+

| Input Parameters  : DWORD dw_Address : PCI eeprom base address                  |

|		      WORD    w_offset : Offset of the adress to read             |

|		      PWORD   pw_Value : PCI eeprom 16 bit read value.            |

+---------------------------------------------------------------------------------+

| Output Parameters : -                                                           |

+---------------------------------------------------------------------------------+

| Return Value      : -                                                           |

+---------------------------------------------------------------------------------+

*/

VOID v_EepromCs76Read(DWORD dw_Address, WORD w_offset, PWORD pw_Value)
{

	CHAR c_BitPos = 0;

	DWORD dw_RegisterValue = 0;

	DWORD dw_RegisterValueRead = 0;

   /*************************************************/

	/* Send EEPROM read command and offset to EEPROM */

   /*************************************************/

	v_EepromSendCommand76(dw_Address, (EE_READ << 4) | (w_offset / 2),
		EE76_CMD_LEN);

   /*******************************/

	/* Get the last register value */

   /*******************************/

	dw_RegisterValue = (((w_offset / 2) & 0x1) << 2) | 0x2;

   /*****************************/

	/* Set the 16-bit value of 0 */

   /*****************************/

	*pw_Value = 0;

   /************************/

	/* Get the 16-bit value */

   /************************/

	for (c_BitPos = 0; c_BitPos < 16; c_BitPos++)
	{

      /****************************/

		/* Trigger the EEPROM clock */

      /****************************/

		v_EepromClock76(dw_Address, dw_RegisterValue);

      /**********************/

		/* Get the result bit */

      /**********************/

		dw_RegisterValueRead = inl(dw_Address);

      /***************/

		/* Wait 0.1 ms */

      /***************/

		udelay(100);

      /***************************************/

		/* Get bit value and shift into result */

      /***************************************/

		if (dw_RegisterValueRead & 0x8)
		{

	 /**********/

			/* Read 1 */

	 /**********/

			*pw_Value = (*pw_Value << 1) | 0x1;

		}

		else
		{

	 /**********/

			/* Read 0 */

	 /**********/

			*pw_Value = (*pw_Value << 1);

		}

	}

   /*************************/

	/* Clear all EEPROM bits */

   /*************************/

	dw_RegisterValue = 0x0;

   /********************************************************************/

	/* Toggle EEPROM's Chip select to get it out of Shift Register Mode */

   /********************************************************************/

	outl(dw_RegisterValue, dw_Address);

   /***************/

	/* Wait 0.1 ms */

   /***************/

	udelay(100);

}

	/******************************************/
	/*      EEPROM HEADER READ FUNCTIONS      */
	/******************************************/

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadMainHeader(WORD w_PCIBoardEepromAddress,  |
|				PCHAR 	pc_PCIChipInformation,struct comedi_device *dev)    |
+----------------------------------------------------------------------------+
| Task              : Read from eeprom Main Header                           |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			  struct comedi_device *dev		   : comedi device structure |
|											 pointer				 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/

INT i_EepromReadMainHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, struct comedi_device *dev)
{
	WORD w_Temp, i, w_Count = 0;
	UINT ui_Temp;
	str_MainHeader s_MainHeader;
	str_DigitalInputHeader s_DigitalInputHeader;
	str_DigitalOutputHeader s_DigitalOutputHeader;
	//str_TimerMainHeader     s_TimerMainHeader,s_WatchdogMainHeader;
	str_AnalogOutputHeader s_AnalogOutputHeader;
	str_AnalogInputHeader s_AnalogInputHeader;

	// Read size
	s_MainHeader.w_HeaderSize =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + 8);

	// Read nbr of functionality
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + 10);
	s_MainHeader.b_Nfunctions = (BYTE) w_Temp & 0x00FF;

	// Read functionality details
	for (i = 0; i < s_MainHeader.b_Nfunctions; i++) {
		// Read Type
		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation, 0x100 + 12 + w_Count);
		s_MainHeader.s_Functions[i].b_Type = (BYTE) w_Temp & 0x3F;
		w_Count = w_Count + 2;
		//Read Address
		s_MainHeader.s_Functions[i].w_Address =
			w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation, 0x100 + 12 + w_Count);
		w_Count = w_Count + 2;
	}

	// Display main header info
	for (i = 0; i < s_MainHeader.b_Nfunctions; i++) {

		switch (s_MainHeader.s_Functions[i].b_Type) {
		case EEPROM_DIGITALINPUT:
			i_EepromReadDigitalInputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_DigitalInputHeader);
			this_board->i_NbrDiChannel =
				s_DigitalInputHeader.w_Nchannel;
			break;

		case EEPROM_DIGITALOUTPUT:
			i_EepromReadDigitalOutputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_DigitalOutputHeader);
			this_board->i_NbrDoChannel =
				s_DigitalOutputHeader.w_Nchannel;
			ui_Temp = 0xffffffff;
			this_board->i_DoMaxdata =
				ui_Temp >> (32 - this_board->i_NbrDoChannel);
			break;

		case EEPROM_ANALOGINPUT:
			i_EepromReadAnlogInputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_AnalogInputHeader);
			if (!(strcmp(this_board->pc_DriverName, "apci3200")))
				this_board->i_NbrAiChannel =
					s_AnalogInputHeader.w_Nchannel * 4;
			else
				this_board->i_NbrAiChannel =
					s_AnalogInputHeader.w_Nchannel;
			this_board->i_Dma = s_AnalogInputHeader.b_HasDma;
			this_board->ui_MinAcquisitiontimeNs =
				(UINT) s_AnalogInputHeader.w_MinConvertTiming *
				1000;
			this_board->ui_MinDelaytimeNs =
				(UINT) s_AnalogInputHeader.w_MinDelayTiming *
				1000;
			ui_Temp = 0xffff;
			this_board->i_AiMaxdata =
				ui_Temp >> (16 -
				s_AnalogInputHeader.b_Resolution);
			break;

		case EEPROM_ANALOGOUTPUT:
			i_EepromReadAnlogOutputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_AnalogOutputHeader);
			this_board->i_NbrAoChannel =
				s_AnalogOutputHeader.w_Nchannel;
			ui_Temp = 0xffff;
			this_board->i_AoMaxdata =
				ui_Temp >> (16 -
				s_AnalogOutputHeader.b_Resolution);
			break;

		case EEPROM_TIMER:
			this_board->i_Timer = 1;	//Timer subdevice present
			break;

		case EEPROM_WATCHDOG:
			this_board->i_Timer = 1;	//Timer subdevice present
			break;

		case EEPROM_TIMER_WATCHDOG_COUNTER:
			this_board->i_Timer = 1;	//Timer subdevice present
		}
	}

	return 0;
}

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadDigitalInputHeader(WORD 					 |
|			w_PCIBoardEepromAddress,PCHAR pc_PCIChipInformation,	 |
|			WORD w_Address,str_DigitalInputHeader *s_Header)		 |
|																	 |
+----------------------------------------------------------------------------+
| Task              : Read Digital Input Header                              |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			 str_DigitalInputHeader *s_Header: Digita Input Header   |
|												   Pointer			 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/
INT i_EepromReadDigitalInputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_DigitalInputHeader * s_Header)
{
	WORD w_Temp;

	// read nbr of channels
	s_Header->w_Nchannel =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);

	// interruptible or not
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 8);
	s_Header->b_Interruptible = (BYTE) (w_Temp >> 7) & 0x01;

// How many interruptible logic
	s_Header->w_NinterruptLogic =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 10);

	return 0;
}

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadDigitalOutputHeader(WORD 				 |
|			w_PCIBoardEepromAddress,PCHAR pc_PCIChipInformation,	 |
|			WORD w_Address,str_DigitalOutputHeader *s_Header)	     |
|																	 |
+----------------------------------------------------------------------------+
| Task              : Read Digital Output Header                             |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			 str_DigitalOutputHeader *s_Header: Digital Output Header|
|											   Pointer				 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/
INT i_EepromReadDigitalOutputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_DigitalOutputHeader * s_Header)
{
// Read Nbr channels
	s_Header->w_Nchannel =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);
	return 0;
}

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadTimerHeader(WORD w_PCIBoardEepromAddress, |
|			PCHAR pc_PCIChipInformation,WORD w_Address,				 |
|			str_TimerMainHeader *s_Header)							 |
+----------------------------------------------------------------------------+
| Task              : Read Timer or Watchdog Header                          |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			 str_TimerMainHeader *s_Header: Timer Header			 |
|											   Pointer				 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/
INT i_EepromReadTimerHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_TimerMainHeader * s_Header)
{

	WORD i, w_Size = 0, w_Temp;

//Read No of Timer
	s_Header->w_Ntimer =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);
//Read header size

	for (i = 0; i < s_Header->w_Ntimer; i++) {
		s_Header->s_TimerDetails[i].w_HeaderSize =
			w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 0);
		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 2);

		//Read Resolution
		s_Header->s_TimerDetails[i].b_Resolution =
			(BYTE) (w_Temp >> 10) & 0x3F;

		//Read Mode
		s_Header->s_TimerDetails[i].b_Mode =
			(BYTE) (w_Temp >> 4) & 0x3F;

		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 4);

		//Read MinTiming
		s_Header->s_TimerDetails[i].w_MinTiming = (w_Temp >> 6) & 0x3FF;

		//Read Timebase
		s_Header->s_TimerDetails[i].b_TimeBase = (BYTE) (w_Temp) & 0x3F;
		w_Size += s_Header->s_TimerDetails[i].w_HeaderSize;
	}

	return 0;
}

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadAnlogOutputHeader(WORD 					 |
|			w_PCIBoardEepromAddress,PCHAR pc_PCIChipInformation,	 |
|			WORD w_Address,str_AnalogOutputHeader *s_Header)         |
+----------------------------------------------------------------------------+
| Task              : Read Nalog Output  Header                              |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			 str_AnalogOutputHeader *s_Header:Anlog Output Header    |
|											   Pointer				 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/

INT i_EepromReadAnlogOutputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_AnalogOutputHeader * s_Header)
{
	WORD w_Temp;
	// No of channels for 1st hard component
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 10);
	s_Header->w_Nchannel = (w_Temp >> 4) & 0x03FF;
	// Resolution for 1st hard component
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 16);
	s_Header->b_Resolution = (BYTE) (w_Temp >> 8) & 0xFF;
	return 0;
}

/*
+----------------------------------------------------------------------------+
| Function Name  : INT i_EepromReadAnlogInputHeader(WORD 					 |
|			w_PCIBoardEepromAddress,PCHAR pc_PCIChipInformation,     |
|			WORD w_Address,str_AnalogInputHeader *s_Header)          |
+----------------------------------------------------------------------------+
| Task              : Read Nalog Output  Header                              |
+----------------------------------------------------------------------------+
| Input Parameters  : WORD w_PCIBoardEepromAddress : PCI eeprom address      |
|																	 |
|		      PCHAR pc_PCIChipInformation  : PCI Chip Type.          |
|																	 |
|			 str_AnalogInputHeader *s_Header:Anlog Input Header      |
|											   Pointer				 |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0							                             |
+----------------------------------------------------------------------------+
*/

// Reads only for ONE  hardware component
INT i_EepromReadAnlogInputHeader(WORD w_PCIBoardEepromAddress,
	PCHAR pc_PCIChipInformation, WORD w_Address,
	str_AnalogInputHeader * s_Header)
{
	WORD w_Temp, w_Offset;
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 10);
	s_Header->w_Nchannel = (w_Temp >> 4) & 0x03FF;
	s_Header->w_MinConvertTiming =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 16);
	s_Header->w_MinDelayTiming =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 30);
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 20);
	s_Header->b_HasDma = (w_Temp >> 13) & 0x01;	// whether dma present or not

	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation, 0x100 + w_Address + 72);	// reading Y
	w_Temp = w_Temp & 0x00FF;
	if (w_Temp)		//Y>0
	{
		w_Offset = 74 + (2 * w_Temp) + (10 * (1 + (w_Temp / 16)));	// offset of first analog input single header
		w_Offset = w_Offset + 2;	// resolution
	} else			//Y=0
	{
		w_Offset = 74;
		w_Offset = w_Offset + 2;	// resolution
	}

// read Resolution
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + w_Offset);
	s_Header->b_Resolution = w_Temp & 0x001F;	// last 5 bits

	return 0;
}
