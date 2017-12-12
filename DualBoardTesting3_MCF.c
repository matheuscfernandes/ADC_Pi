/*
 * ADS1256_test.c:
 *	Very simple program to test the serial port. Expects
 *	the port to be looped back to itself
 *
 *
 * To compile: sudo gcc DualBoardTesting3_MCF.c -o adcTest3_MCF -lbcm2835 -lwiringPi -lm -g
 *
 * /

/*
             define from bcm2835.h                       define from Board DVK511
                 3.3V | | 5V               ->                 3.3V | | 5V
    RPI_V2_GPIO_P1_03 | | 5V               ->                  SDA | | 5V
    RPI_V2_GPIO_P1_05 | | GND              ->                  SCL | | GND
       RPI_GPIO_P1_07 | | RPI_GPIO_P1_08   ->                  IO7 | | TX
                  GND | | RPI_GPIO_P1_10   ->                  GND | | RX
       RPI_GPIO_P1_11 | | RPI_GPIO_P1_12   ->                  IO0 | | IO1
    RPI_V2_GPIO_P1_13 | | GND              ->                  IO2 | | GND
       RPI_GPIO_P1_15 | | RPI_GPIO_P1_16   ->                  IO3 | | IO4
                  VCC | | RPI_GPIO_P1_18   ->                  VCC | | IO5
       RPI_GPIO_P1_19 | | GND              ->                 MOSI | | GND
       RPI_GPIO_P1_21 | | RPI_GPIO_P1_22   ->                 MISO | | IO6
       RPI_GPIO_P1_23 | | RPI_GPIO_P1_24   ->                  SCK | | CE0
                  GND | | RPI_GPIO_P1_26   ->                  GND | | CE1

::if your raspberry Pi is version 1 or rev 1 or rev A
RPI_V2_GPIO_P1_03->RPI_GPIO_P1_03
RPI_V2_GPIO_P1_05->RPI_GPIO_P1_05
RPI_V2_GPIO_P1_13->RPI_GPIO_P1_13
::
*/

// To do: Need to set up a unique chip select pin for the seocond ADC. Then which one is being addressed is controlled with the chip select, and everything beneath that should be the same.

#include <wiringPi.h>

#include <bcm2835.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <time.h>

#include <sys/time.h>
// #include <iostream>



//CS      -----   SPICS
//DIN     -----   MOSI
//DOUT  -----   MISO
//SCLK   -----   SCLK
//DRDY  -----   ctl_IO     data  starting
//RST     -----   ctl_IO     reset



#define  DRDY  RPI_GPIO_P1_11         //P0
#define  RST  RPI_GPIO_P1_12     //P1
#define	SPICS_A	RPI_GPIO_P1_15 //15	//P3 Select ADC Board 1
#define	SPICS_B	RPI_GPIO_P1_16 //16	//P4 Select ADC Board 2

#define CSA_1() bcm2835_gpio_write(SPICS_A,HIGH)
#define CSA_0()  bcm2835_gpio_write(SPICS_A,LOW)
#define CSB_1() bcm2835_gpio_write(SPICS_B,HIGH)
#define CSB_0()  bcm2835_gpio_write(SPICS_B,LOW)



//#define CSA_0() bcm2835_spi_chipSelect(BCM2835_SPI_CS0)
//#define CSB_0() bcm2835_spi_chipSelect(BCM2835_SPI_CS1)
//#define CSA_1() bcm2835_spi_chipSelect(BCM2835_SPI_CS0)
//#define CSB_1() bcm2835_spi_chipSelect(BCM2835_SPI_CS1)

#define DRDY_IS_LOW()	((bcm2835_gpio_lev(DRDY)==0))

#define RST_1() 	bcm2835_gpio_write(RST,HIGH);
#define RST_0() 	bcm2835_gpio_write(RST,LOW);



/* Unsigned integer types  */
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned long

/* GLOBAL VARIABLES */
	volatile int drdy_count;
	int activeBoard;



//typedef enum {FALSE = 0, TRUE = !FALSE} bool;


/* gain channel� */
typedef enum
{
	ADS1256_GAIN_1			= (0),	/* GAIN   1 */
	ADS1256_GAIN_2			= (1),	/*GAIN   2 */
	ADS1256_GAIN_4			= (2),	/*GAIN   4 */
	ADS1256_GAIN_8			= (3),	/*GAIN   8 */
	ADS1256_GAIN_16			= (4),	/* GAIN  16 */
	ADS1256_GAIN_32			= (5),	/*GAIN    32 */
	ADS1256_GAIN_64			= (6),	/*GAIN    64 */
}ADS1256_GAIN_E;

/* Sampling speed choice*/
/*
	11110000 = 30,000SPS (default)
	11100000 = 15,000SPS
	11010000 = 7,500SPS
	11000000 = 3,750SPS
	10110000 = 2,000SPS
	10100001 = 1,000SPS
	10010010 = 500SPS
	10000010 = 100SPS
	01110010 = 60SPS
	01100011 = 50SPS
	01010011 = 30SPS
	01000011 = 25SPS
	00110011 = 15SPS
	00100011 = 10SPS
	00010011 = 5SPS
	00000011 = 2.5SPS
*/
typedef enum
{
	ADS1256_30000SPS = 0,
	ADS1256_15000SPS,
	ADS1256_7500SPS,
	ADS1256_3750SPS,
	ADS1256_2000SPS,
	ADS1256_1000SPS,
	ADS1256_500SPS,
	ADS1256_100SPS,
	ADS1256_60SPS,
	ADS1256_50SPS,
	ADS1256_30SPS,
	ADS1256_25SPS,
	ADS1256_15SPS,
	ADS1256_10SPS,
	ADS1256_5SPS,
	ADS1256_2d5SPS,

	ADS1256_DRATE_MAX
}ADS1256_DRATE_E;

#define ADS1256_DRAE_COUNT = 15;

typedef struct
{
	ADS1256_GAIN_E Gain;		/* GAIN  */
	ADS1256_DRATE_E DataRate;	/* DATA output  speed*/
	int32_t AdcNow[8];			/* ADC  Conversion value */
	uint8_t Channel;			/* The current channel*/
	uint8_t ScanMode;	/*Scanning mode,   0  Single-ended input  8 channel�� 1 Differential input  4 channel*/
}ADS1256_VAR_T;



/*Register definition�� Table 23. Register Map --- ADS1256 datasheet Page 30*/
enum
{
	/*Register address, followed by reset the default values */
	REG_STATUS = 0,	// x1H
	REG_MUX    = 1, // 01H
	REG_ADCON  = 2, // 20H
	REG_DRATE  = 3, // F0H

	REG_IO     = 4, // E0H
	REG_OFC0   = 5, // xxH
	REG_OFC1   = 6, // xxH
	REG_OFC2   = 7, // xxH
	REG_FSC0   = 8, // xxH
	REG_FSC1   = 9, // xxH
	REG_FSC2   = 10, // xxH
};

/* Command definition�� TTable 24. Command Definitions --- ADS1256 datasheet Page 34 */
enum
{
	CMD_WAKEUP  = 0x00,	// Completes SYNC and Exits Standby Mode 0000  0000 (00h)
	CMD_RDATA   = 0x01, // Read Data 0000  0001 (01h)
	CMD_RDATAC  = 0x03, // Read Data Continuously 0000   0011 (03h)
	CMD_SDATAC  = 0x0F, // Stop Read Data Continuously 0000   1111 (0Fh)
	CMD_RREG    = 0x10, // Read from REG rrr 0001 rrrr (1xh)
	CMD_WREG    = 0x50, // Write to REG rrr 0101 rrrr (5xh)
	CMD_SELFCAL = 0xF0, // Offset and Gain Self-Calibration 1111    0000 (F0h)
	CMD_SELFOCAL= 0xF1, // Offset Self-Calibration 1111    0001 (F1h)
	CMD_SELFGCAL= 0xF2, // Gain Self-Calibration 1111    0010 (F2h)
	CMD_SYSOCAL = 0xF3, // System Offset Calibration 1111   0011 (F3h)
	CMD_SYSGCAL = 0xF4, // System Gain Calibration 1111    0100 (F4h)
	CMD_SYNC    = 0xFC, // Synchronize the A/D Conversion 1111   1100 (FCh)
	CMD_STANDBY = 0xFD, // Begin Standby Mode 1111   1101 (FDh)
	CMD_RESET   = 0xFE, // Reset to Power-Up Values 1111   1110 (FEh)
};


ADS1256_VAR_T g_tADS1256;
static const uint8_t s_tabDataRate[ADS1256_DRATE_MAX] =
{
	0xF0,		/*reset the default values  */
	0xE0,
	0xD0,
	0xC0,
	0xB0,
	0xA1,
	0x92,
	0x82,
	0x72,
	0x63,
	0x53,
	0x43,
	0x33,
	0x20,
	0x13,
	0x03
};







void  bsp_DelayUS(uint64_t micros);
void ADS1256_StartScan(uint8_t _ucScanMode);
static void ADS1256_Send8Bit(uint8_t _data);
void ADS1256_CfgADC(ADS1256_GAIN_E _gain, ADS1256_DRATE_E _drate);
static void ADS1256_DelayDATA(void);
static uint8_t ADS1256_Recive8Bit(void);
static void ADS1256_WriteReg(uint8_t _RegID, uint8_t _RegValue);
static uint8_t ADS1256_ReadReg(uint8_t _RegID);
static void ADS1256_WriteCmd(uint8_t _cmd);
uint8_t ADS1256_ReadChipID(void);
static void ADS1256_SetChannal(uint8_t _ch);
static void ADS1256_SetDiffChannal(uint8_t _ch);
static void ADS1256_WaitDRDY(void);
static int32_t ADS1256_ReadData(void);

int32_t ADS1256_GetAdc(uint8_t _ch);
void ADS1256_ISR(void);
uint8_t ADS1256_Scan(void);




void  bsp_DelayUS(uint64_t micros)
{
		bcm2835_delayMicroseconds (micros);
}


/*
*********************************************************************************************************
*	name: bsp_InitADS1256
*	function: Configuration of the STM32 GPIO and SPI interface��The connection ADS1256
*	parameter: NULL
*	The return value: NULL
*********************************************************************************************************
*/


void bsp_InitADS1256(void)
{
#ifdef SOFT_SPI
	if (activeBoard == 0) CSA_1();
	else CSB_1();
	SCK_0();
	DI_0();
#endif

//ADS1256_CfgADC(ADS1256_GAIN_1, ADS1256_1000SPS);	/* ����ADC������ ����1:1, ������������ 1KHz */
}




/*
*********************************************************************************************************
*	name: ADS1256_StartScan
*	function: Configuration DRDY PIN for external interrupt is triggered
*	parameter: _ucDiffMode : 0  Single-ended input  8 channel�� 1 Differential input  4 channe
*	The return value: NULL
*********************************************************************************************************
*/
void ADS1256_StartScan(uint8_t _ucScanMode)
{
	g_tADS1256.ScanMode = _ucScanMode;
	/* ��ʼɨ��ǰ, �������������� */
	{
		uint8_t i;

		g_tADS1256.Channel = 0;

		for (i = 0; i < 8; i++)
		{
			g_tADS1256.AdcNow[i] = 0;
		}
	}

}

/*
*********************************************************************************************************
*	name: ADS1256_Send8Bit
*	function: SPI bus to send 8 bit data
*	parameter: _data:  data
*	The return value: NULL
*********************************************************************************************************
*/
static void ADS1256_Send8Bit(uint8_t _data)
{

	bsp_DelayUS(2);
	bcm2835_spi_transfer(_data);
}

/*
*********************************************************************************************************
*	name: ADS1256_CfgADC
*	function: The configuration parameters of ADC, gain and data rate
*	parameter: _gain:gain 1-64
*                      _drate:  data  rate
*	The return value: NULL
*********************************************************************************************************
*/
void ADS1256_CfgADC(ADS1256_GAIN_E _gain, ADS1256_DRATE_E _drate)
{
	g_tADS1256.Gain = _gain;
	g_tADS1256.DataRate = _drate;

	ADS1256_WaitDRDY();

	{
		uint8_t buf[4];		/* Storage ads1256 register configuration parameters */

		/*Status register define
			Bits 7-4 ID3, ID2, ID1, ID0  Factory Programmed Identification Bits (Read Only)

			Bit 3 ORDER: Data Output Bit Order
				0 = Most Significant Bit First (default)
				1 = Least Significant Bit First
			Input data  is always shifted in most significant byte and bit first. Output data is always shifted out most significant
			byte first. The ORDER bit only controls the bit order of the output data within the byte.

			Bit 2 ACAL : Auto-Calibration
				0 = Auto-Calibration Disabled (default)
				1 = Auto-Calibration Enabled
			When Auto-Calibration is enabled, self-calibration begins at the completion of the WREG command that changes
			the PGA (bits 0-2 of ADCON register), DR (bits 7-0 in the DRATE register) or BUFEN (bit 1 in the STATUS register)
			values.

			Bit 1 BUFEN: Analog Input Buffer Enable
				0 = Buffer Disabled (default)
				1 = Buffer Enabled

			Bit 0 DRDY :  Data Ready (Read Only)
				This bit duplicates the state of the DRDY pin.

			ACAL=1  enable  calibration
		*/
		//buf[0] = (0 << 3) | (1 << 2) | (1 << 1);//enable the internal buffer
        buf[0] = (0 << 3) | (1 << 2) | (0 << 1);  // The internal buffer is prohibited

        //ADS1256_WriteReg(REG_STATUS, (0 << 3) | (1 << 2) | (1 << 1));

		buf[1] = 0x08;

		/*	ADCON: A/D Control Register (Address 02h)
			Bit 7 Reserved, always 0 (Read Only)
			Bits 6-5 CLK1, CLK0 : D0/CLKOUT Clock Out Rate Setting
				00 = Clock Out OFF
				01 = Clock Out Frequency = fCLKIN (default)
				10 = Clock Out Frequency = fCLKIN/2
				11 = Clock Out Frequency = fCLKIN/4
				When not using CLKOUT, it is recommended that it be turned off. These bits can only be reset using the RESET pin.

			Bits 4-3 SDCS1, SCDS0: Sensor Detect Current Sources
				00 = Sensor Detect OFF (default)
				01 = Sensor Detect Current = 0.5 �� A
				10 = Sensor Detect Current = 2 �� A
				11 = Sensor Detect Current = 10�� A
				The Sensor Detect Current Sources can be activated to verify  the integrity of an external sensor supplying a signal to the
				ADS1255/6. A shorted sensor produces a very small signal while an open-circuit sensor produces a very large signal.

			Bits 2-0 PGA2, PGA1, PGA0: Programmable Gain Amplifier Setting
				000 = 1 (default)
				001 = 2
				010 = 4
				011 = 8
				100 = 16
				101 = 32
				110 = 64
				111 = 64
		*/
		buf[2] = (0 << 5) | (0 << 3) | (_gain << 0);
		//ADS1256_WriteReg(REG_ADCON, (0 << 5) | (0 << 2) | (GAIN_1 << 1));	/*choose 1: gain 1 ;input 5V/
		buf[3] = s_tabDataRate[_drate];	// DRATE_10SPS;

		if (activeBoard == 0) CSA_0();
		else CSB_0();	/* SPIƬѡ = 0 */
		ADS1256_Send8Bit(CMD_WREG | 0);	/* Write command register, send the register address */
		ADS1256_Send8Bit(0x03);			/* Register number 4,Initialize the number  -1*/

		ADS1256_Send8Bit(buf[0]);	/* Set the status register */
		ADS1256_Send8Bit(buf[1]);	/* Set the input channel parameters */
		ADS1256_Send8Bit(buf[2]);	/* Set the ADCON control register,gain */
		ADS1256_Send8Bit(buf[3]);	/* Set the output rate */

		if (activeBoard == 0) CSA_1();
		else CSB_1();					/* SPI  cs = 1 */
	}

	bsp_DelayUS(50);
}


/*
*********************************************************************************************************
*	name: ADS1256_DelayDATA
*	function: delay
*	parameter: NULL
*	The return value: NULL
*********************************************************************************************************
*/
static void ADS1256_DelayDATA(void)
{
	/*
		Delay from last SCLK edge for DIN to first SCLK rising edge for DOUT: RDATA, RDATAC,RREG Commands
		min  50   CLK = 50 * 0.13uS = 6.5uS
	*/
	bsp_DelayUS(10);	/* The minimum time delay 6.5us */
}




/*
*********************************************************************************************************
*	name: ADS1256_Recive8Bit
*	function: SPI bus receive function
*	parameter: NULL
*	The return value: NULL
*********************************************************************************************************
*/
static uint8_t ADS1256_Recive8Bit(void)
{
	uint8_t read = 0;
	read = bcm2835_spi_transfer(0xff);
	return read;
}

/*
*********************************************************************************************************
*	name: ADS1256_WriteReg
*	function: Write the corresponding register
*	parameter: _RegID: register  ID
*			 _RegValue: register Value
*	The return value: NULL
*********************************************************************************************************
*/
static void ADS1256_WriteReg(uint8_t _RegID, uint8_t _RegValue)
{
	if (activeBoard == 0) CSA_0();
	else CSB_0();	/* SPI  cs  = 0 */
	ADS1256_Send8Bit(CMD_WREG | _RegID);	/*Write command register */
	ADS1256_Send8Bit(0x00);		/*Write the register number */

	ADS1256_Send8Bit(_RegValue);	/*send register value */
	if (activeBoard == 0) CSA_1();
	else CSB_1();	/* SPI   cs = 1 */
}

/*
*********************************************************************************************************
*	name: ADS1256_ReadReg
*	function: Read  the corresponding register
*	parameter: _RegID: register  ID
*	The return value: read register value
*********************************************************************************************************
*/
static uint8_t ADS1256_ReadReg(uint8_t _RegID)
{
	uint8_t read;

	if (activeBoard == 0) CSA_0();
	else CSB_0();	/* SPI  cs  = 0 */
	ADS1256_Send8Bit(CMD_RREG | _RegID);	/* Write command register */
	ADS1256_Send8Bit(0x00);	/* Write the register number */

	ADS1256_DelayDATA();	/*delay time */

	read = ADS1256_Recive8Bit();	/* Read the register values */
	if (activeBoard == 0) CSA_1();
	else CSB_1();	/* SPI   cs  = 1 */

	return read;
}

/*
*********************************************************************************************************
*	name: ADS1256_WriteCmd
*	function: Sending a single byte order
*	parameter: _cmd : command
*	The return value: NULL
*********************************************************************************************************
*/
static void ADS1256_WriteCmd(uint8_t _cmd)
{
	if (activeBoard == 0) CSA_0();
	else CSB_0();	/* SPI   cs = 0 */
	ADS1256_Send8Bit(_cmd);
	if (activeBoard == 0) CSA_1();
	else CSB_1();	/* SPI  cs  = 1 */
}

/*
*********************************************************************************************************
*	name: ADS1256_ReadChipID
*	function: Read the chip ID
*	parameter: _cmd : NULL
*	The return value: four high status register
*********************************************************************************************************
*/
uint8_t ADS1256_ReadChipID(void)
{
	uint8_t id;

	ADS1256_WaitDRDY();
	id = ADS1256_ReadReg(REG_STATUS);
	return (id >> 4);
}

/*
*********************************************************************************************************
*	name: ADS1256_SetChannal
*	function: Configuration channel number
*	parameter:  _ch:  channel number  0--7
*	The return value: NULL
*********************************************************************************************************
*/
static void ADS1256_SetChannal(uint8_t _ch)
{
	/*
	Bits 7-4 PSEL3, PSEL2, PSEL1, PSEL0: Positive Input Channel (AINP) Select
		0000 = AIN0 (default)
		0001 = AIN1
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when PSEL3 = 1, PSEL2, PSEL1, PSEL0 are ��don��t care��)

		NOTE: When using an ADS1255 make sure to only select the available inputs.

	Bits 3-0 NSEL3, NSEL2, NSEL1, NSEL0: Negative Input Channel (AINN)Select
		0000 = AIN0
		0001 = AIN1 (default)
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when NSEL3 = 1, NSEL2, NSEL1, NSEL0 are ��don��t care��)
	*/
	if (_ch > 7)
	{
		return;
	}
	ADS1256_WriteReg(REG_MUX, (_ch << 4) | (1 << 3));	/* Bit3 = 1, AINN connection AINCOM */
}

/*
*********************************************************************************************************
*	name: ADS1256_SetDiffChannal
*	function: The configuration difference channel
*	parameter:  _ch:  channel number  0--3
*	The return value:  four high status register
*********************************************************************************************************
*/
static void ADS1256_SetDiffChannal(uint8_t _ch)
{
	/*
	Bits 7-4 PSEL3, PSEL2, PSEL1, PSEL0: Positive Input Channel (AINP) Select
		0000 = AIN0 (default)
		0001 = AIN1
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when PSEL3 = 1, PSEL2, PSEL1, PSEL0 are ��don��t care��)

		NOTE: When using an ADS1255 make sure to only select the available inputs.

	Bits 3-0 NSEL3, NSEL2, NSEL1, NSEL0: Negative Input Channel (AINN)Select
		0000 = AIN0
		0001 = AIN1 (default)
		0010 = AIN2 (ADS1256 only)
		0011 = AIN3 (ADS1256 only)
		0100 = AIN4 (ADS1256 only)
		0101 = AIN5 (ADS1256 only)
		0110 = AIN6 (ADS1256 only)
		0111 = AIN7 (ADS1256 only)
		1xxx = AINCOM (when NSEL3 = 1, NSEL2, NSEL1, NSEL0 are ��don��t care��)
	*/
	if (_ch == 0)
	{
		ADS1256_WriteReg(REG_MUX, (0 << 4) | 1);	/* DiffChannal  AIN0�� AIN1 */
	}
	else if (_ch == 1)
	{
		ADS1256_WriteReg(REG_MUX, (2 << 4) | 3);	/*DiffChannal   AIN2�� AIN3 */
	}
	else if (_ch == 2)
	{
		ADS1256_WriteReg(REG_MUX, (4 << 4) | 5);	/*DiffChannal    AIN4�� AIN5 */
	}
	else if (_ch == 3)
	{
		ADS1256_WriteReg(REG_MUX, (6 << 4) | 7);	/*DiffChannal   AIN6�� AIN7 */
	}
}

/*
*********************************************************************************************************
*	name: ADS1256_WaitDRDY
*	function: delay time  wait for automatic calibration
*	parameter:  NULL
*	The return value:  NULL
*********************************************************************************************************
*/
static void ADS1256_WaitDRDY(void)
{
	uint32_t i;

	for (i = 0; i < 400000; i++)
	{
		if (DRDY_IS_LOW())
		{
			break;
		}
	}
	if (i >= 400000)
	{
		printf("ADS1256_WaitDRDY() Time Out ...\r\n");
	}
}

/*
*********************************************************************************************************
*	name: ADS1256_ReadData
*	function: read ADC value
*	parameter: NULL
*	The return value:  NULL
*********************************************************************************************************
*/
static int32_t ADS1256_ReadData(void)
{
	uint32_t read = 0;
    static uint8_t buf[3];

	if (activeBoard == 0) CSA_0();
	else CSB_0();					/* SPI   cs = 0 */

	ADS1256_Send8Bit(CMD_RDATA);	/* read ADC command  */

	ADS1256_DelayDATA();	/*delay time  */

	/*Read the sample results 24bit*/
    buf[0] = ADS1256_Recive8Bit();
    buf[1] = ADS1256_Recive8Bit();
    buf[2] = ADS1256_Recive8Bit();

    read = ((uint32_t)buf[0] << 16) & 0x00FF0000;
    read |= ((uint32_t)buf[1] << 8);  /* Pay attention to It is wrong   read |= (buf[1] << 8) */
    read |= buf[2];

	if (activeBoard == 0) CSA_1();
	else CSB_1();	/* SPIƬѡ = 1 */

	/* Extend a signed number*/
    if (read & 0x800000)
    {
	    read |= 0xFF000000;
    }

	return (int32_t)read;
}


/*
*********************************************************************************************************
*	name: ADS1256_GetAdc
*	function: read ADC value
*	parameter:  channel number 0--7
*	The return value:  ADC vaule (signed number)
*********************************************************************************************************
*/
int32_t ADS1256_GetAdc(uint8_t _ch)
{
	int32_t iTemp;

	if (_ch > 7)
	{
		return 0;
	}

	iTemp = g_tADS1256.AdcNow[_ch];

	return iTemp;
}

/*
*********************************************************************************************************
*	name: ADS1256_ISR
*	function: Collection procedures
*	parameter: NULL
*	The return value:  NULL
*********************************************************************************************************
*/
void ADS1256_ISR(void)
{
	if (g_tADS1256.ScanMode == 0)	/*  0  Single-ended input  8 channel�� 1 Differential input  4 channe */
	{

		ADS1256_SetChannal(g_tADS1256.Channel);	/*Switch channel mode */
		bsp_DelayUS(5);

		ADS1256_WriteCmd(CMD_SYNC);
		bsp_DelayUS(5);

		ADS1256_WriteCmd(CMD_WAKEUP);
		bsp_DelayUS(25);

		if (g_tADS1256.Channel == 0)
		{
			g_tADS1256.AdcNow[7] = ADS1256_ReadData();
		}
		else
		{
			g_tADS1256.AdcNow[g_tADS1256.Channel-1] = ADS1256_ReadData();
		}

		if (++g_tADS1256.Channel >= 8)
		{
			g_tADS1256.Channel = 0;
		}
	}
	else	/*DiffChannal*/
	{

		ADS1256_SetDiffChannal(g_tADS1256.Channel);	/* change DiffChannal */
		bsp_DelayUS(5);

		ADS1256_WriteCmd(CMD_SYNC);
		bsp_DelayUS(5);

		ADS1256_WriteCmd(CMD_WAKEUP);
		bsp_DelayUS(25);

		if (g_tADS1256.Channel == 0)
		{
			g_tADS1256.AdcNow[3] = ADS1256_ReadData();
		}
		else
		{
			g_tADS1256.AdcNow[g_tADS1256.Channel-1] = ADS1256_ReadData();
		}

		if (++g_tADS1256.Channel >= 4)
		{
			g_tADS1256.Channel = 0;
		}
	}
}

/*
*********************************************************************************************************
*	name: ADS1256_Scan
*	function:
*	parameter:NULL
*	The return value:  1
*	Note from Andrew: This appears to be an inefficient way of checking if the next sample is ready. Next
*					 	function should be an improvement
*********************************************************************************************************
*/
uint8_t ADS1256_Scan(void)
{
	if (DRDY_IS_LOW())
	{
		ADS1256_ISR();
		return 1;
	}

	return 0;
}

/*
*********************************************************************************************************
*	name: Write_DAC8552
*	function:  DAC send data
*	parameter: channel : output channel number
*			   data : output DAC value
*	The return value:  NULL
*********************************************************************************************************
*/
void Write_DAC8552(uint8_t channel, uint16_t Data)
{
	uint8_t i;

	  if (activeBoard == 0) CSA_1();
	  else CSB_1();
	  if (activeBoard == 0) CSA_0();
	  else CSB_0();
      bcm2835_spi_transfer(channel);
      bcm2835_spi_transfer((Data>>8));
      bcm2835_spi_transfer((Data&0xff));
	  if (activeBoard == 0) CSA_1();
	  else CSB_1();
}
/*
*********************************************************************************************************
*	name: Voltage_Convert
*	function:  Voltage value conversion function
*	parameter: Vref : The reference voltage 3.3V or 5V
*			   voltage : output DAC value
*	The return value:  NULL
*********************************************************************************************************
*/
uint16_t Voltage_Convert(float Vref, float voltage)
{
	uint16_t _D_;
	_D_ = (uint16_t)(65536 * voltage / Vref);

	return _D_;
}


/*
*********************************************************************************************************
* 	name:	Wall clock timer
* 	purpose: get current wall time
* 	Author : Mysticial from https://stackoverflow.com/questions/17432502/how-can-i-measure-cpu-time-and-wall-clock-time-on-both-linux-windows
*
* *******************************************************************************************************
*/
double get_wall_time()
{
    struct timeval wallTime;
    return gettimeofday(&wallTime,NULL);
    // if (gettimeofday(&wallTime,NULL))
	// {
        //  Handle error
//         return 0;
// 	}
}


/*
*********************************************************************************************************
* 	name:	Initialize timer
* 	purpose: Start wall clock timer
* 	Author : Andrew Gross
*
* *******************************************************************************************************
*/
float initializeTimer()
{
        struct timespec startTime;

        clock_gettime(CLOCK_MONOTONIC, &startTime);
        return startTime.tv_sec + startTime.tv_nsec*1e-9;
        // return startTime.tv_sec;
}

/*
*********************************************************************************************************
* 	name:	Current time
* 	purpose: Get ellapsed time from start time
* 	Author : Andrew Gross
*
* *******************************************************************************************************
*/
float getCurrentTime(float startTime)
{
        struct timespec currentTime;

        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        return currentTime.tv_sec + currentTime.tv_nsec*1e-9 - startTime;
        // return currentTime.tv_sec - startTime;
}

/*
*********************************************************************************************************
* 	name:	Interrupt handlers
* 	purpose: to take in the interrupt trigger and publish the interrupt event
* 	Author : Brad Eisenschenk
*
* *******************************************************************************************************
*/
	static volatile int intrflag ;
	static volatile int lastintrflag;


	void InterruptPin0 (void) {
		if(DRDY_IS_LOW())
		{ intrflag= 1;
			ADS1256_ISR();
			}
		}				//trigger the interrupt flag so that the output will be written to file (in main)
	//void InterruptPin2 (void) { ++globalCounter [2]; }
	//void InterruptPin3 (void) { ++globalCounter [3]; }
	//void InterruptPin4 (void) { ++globalCounter [4]; }
	//void InterruptPin5 (void) { ++globalCounter [5]; }
	//void InterruptPin6 (void) { ++globalCounter [6]; }
	//void InterruptPin7 (void) { ++globalCounter [7]; }


/*
* *******************************************************************************************************
*
*********************************************************************************************************
*	name: main
*	function:
*	parameter: NULL
*	The return value:  NULL
*********************************************************************************************************
*/
	float R1;
	float R2;
	float a;
	float b;
	float c;
	float y;
	float temp;
	float tempK;
	float tempC;
	float tempF;



int  main()
{
	wiringPiSetup();
	float currentTime;
	float startTime;
	float powf(float t, float u);
	float iTempTot;
	// int activeBoard;
	float floatVoltage[8];
	float floatVoltage1[8];
	uint8_t id;
	uint8_t id1;
  	int32_t adc[8];
	int32_t volt[8];
	uint8_t ch_num;
	uint8_t buf[3];

	if (!bcm2835_init())
        	return 1;
	bcm2835_spi_begin();
		//bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, 0);
		//bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, 0);
    	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_LSBFIRST );      	// The default
    	bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);                   	// The default
    	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); 	// The default
    	//bcm2835SPIChipSelect(BCM2835_SPI_CS2);
    	bcm2835_gpio_fsel(SPICS_A, BCM2835_GPIO_FSEL_OUTP);				//Set SPICS_A as an output pin
    	bcm2835_gpio_write(SPICS_A, HIGH);								//Set SPICS_A to High
    	bcm2835_gpio_fsel(DRDY, BCM2835_GPIO_FSEL_INPT);				//Set DRDY as an Input
    	bcm2835_gpio_set_pud(DRDY, BCM2835_GPIO_PUD_UP);    			//Set DRDY to use internal pull up resistor
    	activeBoard = 0;
		//ADS1256_WriteReg(REG_MUX,0x01);
    	//ADS1256_WriteReg(REG_ADCON,0x20);
   	// ADS1256_CfgADC(ADS1256_GAIN_1, ADS1256_15SPS);
   	id = ADS1256_ReadChipID();
	if (id != 3)
	{
		printf("Error, ASD1256 Chip ID = 0x%d\r\n", (int)id);
	}
	else
	{
		printf("Ok, ASD1256 Chip ID = 0x%d\r\n", (int)id);
	}

  	// ADS1256_CfgADC(ADS1256_GAIN_4, ADS1256_15SPS);
  	ADS1256_CfgADC(ADS1256_GAIN_2, ADS1256_1000SPS);
   	ADS1256_StartScan(0);

  	//
		//bcm2835_gpio_write(SPICS_A, LOW);								//Set SPICS_A to Low
    	bcm2835_gpio_fsel(SPICS_B, BCM2835_GPIO_FSEL_OUTP);				//Set SPICS_B as an output pin
  	    bcm2835_gpio_write(SPICS_B, HIGH);								//Set SPICS_B to High
    	//bcm2835_gpio_fsel(DRDY, BCM2835_GPIO_FSEL_INPT);				//Set DRDY as an Input
    	//bcm2835_gpio_set_pud(DRDY, BCM2835_GPIO_PUD_UP);    			//Set DRDY to use internal pull up resistor
    	//ADS1256_WriteReg(REG_MUX,0x01);
    	//ADS1256_WriteReg(REG_ADCON,0x20);
   	// ADS1256_CfgADC(ADS1256_GAIN_1, ADS1256_15SPS);
   	activeBoard = 1;
   	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
   	id1 = ADS1256_ReadChipID();
	if (id1 != 3)
	{
		printf("Error, ASD1256 Chip ID = 0x%d\r\n", (int)id1);
	}
	else
	{
		printf("Ok, ASD1256 Chip ID = 0x%d\r\n", (int)id1);
	}

  	// ADS1256_CfgADC(ADS1256_GAIN_4, ADS1256_15SPS);
  	ADS1256_CfgADC(ADS1256_GAIN_2, ADS1256_1000SPS);
    ADS1256_StartScan(0);


   		//bcm2835_gpio_write(SPICS_B, LOW);								//Set SPICS_B to Low
  	    //bcm2835_gpio_write(SPICS_A, HIGH);								//Set SPICS_A to High

  	//



		ch_num = 8;
		int c=0;

		wiringPiISR (0, INT_EDGE_FALLING, &InterruptPin0);		//pin 0 (in wiringPi) is the DRDY pin, watching here for it to go "Active Low" to signal data is ready to be read

		FILE* dataLog = fopen("DualBoardTesting_MCF.csv", "w");

		startTime = initializeTimer();
		activeBoard = 0;
		bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
		while(1)
		{
			if (intrflag == 1)
			{
				adc[c]=ADS1256_GetAdc(c);			//pull data from adc in
				volt[c] = (adc[c] * 100) / 167;
				fprintf("%f\n",volt[c]);
				if (activeBoard == 0)
				{
					floatVoltage[c] = (float)volt[c]/1000000;
					if (c == 7)
					{
						//bcm2835_gpio_write(SPICS_A, LOW);								//Set SPICS_A to Low
						//bcm2835_gpio_write(SPICS_B, HIGH);								//Set SPICS_B to High
						activeBoard = 1;
						bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
						c = 0;
					}
					else
					{
						c += 1;
					}
				}
				else if (activeBoard == 1)
				{
					floatVoltage1[c] = (float)volt[c]/1000000;
					if (c == 7)
					{
						//bcm2835_gpio_write(SPICS_B, LOW);								//Set SPICS_B to Low
						//bcm2835_gpio_write(SPICS_A, HIGH);								//Set SPICS_A to High
						activeBoard = 0;
						bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
						c = 0;
					}
					else
					{
						c += 1;
					}
				}
				lastintrflag=1;
				// printf("\nADC Pin: %d , \tadc val: %d \tvolt : %f \n", c, adc[c], floatVoltage[c]);
				if( c == 0 && activeBoard == 0)
				{
					//c = 0;
					currentTime = getCurrentTime(startTime);
					fprintf(dataLog, "Time:%f, %f, %f, %f, %f, %f, %f\n", currentTime, floatVoltage[0], floatVoltage[1], floatVoltage[2], floatVoltage1[0], floatVoltage1[1], floatVoltage1[2]);
					fflush(dataLog);
				}
				//else
				//{
				//	c += 1;
				//}
			}
			if (intrflag != 0 && lastintrflag != 0)
			{
				intrflag=0;
			}

			else if( ( intrflag == 0 && lastintrflag == 1 ) || ( intrflag == 0 && lastintrflag == 0 ) )
			{
				// printf ("Waiting ... ") ; fflush (stdout) ;
				// bsp_DelayUS(50);
				lastintrflag=0;
			}
		// currentTime = getCurrentTime(startTime);
		// printf("Time: %f seconds\n", currentTime);
		}
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}
