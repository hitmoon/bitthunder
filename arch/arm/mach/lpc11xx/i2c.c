/**
 *	LPC11xx Hal for BitThunder
 *	I2C Driver Implementation.
 *
 *	This driver serves as robust example as to how implement a fully functional I2C device driver
 *	for BitThunder.
 *
 *	This driver should be easily ported to I2C peripherals on other processors with little effort.
 *
 *	@author		Robert Steinbauer <rsteinbauer@riegl.com>
 *	@copyright	(c)2012 Robert Steinbauer
 *	@copyright	(c)2012 Riegl Laser Measurement Systems GmbH
 *
 **/
#include <bitthunder.h>	 				// Include all those wonderful BitThunder APIs.
#include "i2c.h"						// Includes a full hardware definition for the integrated I2Cs.
#include "rcc.h"						// Used for getting access to rcc regs, to determine the real Clock Freq.
#include "ioconfig.h"					// Used for getting access to IOCON regs.
#include <collections/bt_fifo.h>

/**
 *	All driver modules in the system shall be tagged with some helpful information.
 *	This way we know who to blame when things go wrong!
 **/
BT_DEF_MODULE_NAME						("LPC11xx-I2C")
BT_DEF_MODULE_DESCRIPTION				("Simple I2C device for the LPC11xx Embedded Platform")
BT_DEF_MODULE_AUTHOR					("Robert Steinbauer")
BT_DEF_MODULE_EMAIL						("rsteinbauer@riegl.com")

/**
 *	We can define how a handle should look in a I2C driver, probably we only need a
 *	hardware-ID number. (Remember, try to keep HANDLES as low-cost as possible).
 **/
struct _BT_OPAQUE_HANDLE {
	BT_HANDLE_HEADER 		h;			///< All handles must include a handle header.
	LPC11xx_I2C_REGS	   *pRegs;
	const BT_INTEGRATED_DEVICE   *pDevice;
	BT_I2C_OPERATING_MODE	eMode;		///< Operational mode, i.e. buffered/polling mode.
	BT_HANDLE		   		hRxFifo;		///< RX fifo - ring buffer.
	BT_HANDLE		   		hTxFifo;		///< TX fifo - ring buffer.
};

static BT_HANDLE g_I2C_HANDLES[3] = {
	NULL,
	NULL,
	NULL,
};

static const BT_IF_HANDLE oHandleInterface;	// Prototype for the I2COpen function.
static void disablei2cPeripheralClock(BT_HANDLE hI2C);


BT_ERROR BT_NVIC_IRQ_26(void) {
	return 0;
}

BT_ERROR BT_NVIC_IRQ_27(void) {
	return 0;
}
BT_ERROR BT_NVIC_IRQ_28(void) {
	return 0;
}

static BT_ERROR i2cDisable(BT_HANDLE hI2C);
static BT_ERROR i2cEnable(BT_HANDLE hI2C);


static void ResetI2C(BT_HANDLE hI2C)
{
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;

	pRegs->LPC11xx_I2C_CONCLR = 0xFFFFFFFF;

}

/**
 *	All modules MUST provide a FULL cleanup function. This must cleanup all allocations etc
 *	e.g. we will have to populate this when we add circular buffers!
 *
 **/
static BT_ERROR i2cCleanup(BT_HANDLE hI2C) {
	ResetI2C(hI2C);

	// Disable peripheral clock.
	disablei2cPeripheralClock(hI2C);

	// Free any buffers if used.
	if(hI2C->eMode == BT_I2C_MODE_BUFFERED) {
		BT_CloseHandle(hI2C->hTxFifo);
		BT_CloseHandle(hI2C->hRxFifo);
	}

	const BT_RESOURCE *pResource = BT_GetIntegratedResource(hI2C->pDevice, BT_RESOURCE_IRQ, 0);

	BT_DisableInterrupt(pResource->ulStart);

	pResource = BT_GetIntegratedResource(hI2C->pDevice, BT_RESOURCE_ENUM, 0);

	g_I2C_HANDLES[pResource->ulStart] = NULL;	// Finally mark the hardware as not used.

	return BT_ERR_NONE;
}

#define MAX_BAUD_ERROR_RATE	3	/* max % error allowed */

static BT_ERROR i2cSetClockrate(BT_HANDLE hI2C, BT_I2C_CLOCKRATE eClockrate) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;

	BT_u32 ulInputClk;
	BT_u32 ulClock;
	BT_u32 ulClkPeriod;

	/*
	 *	We must determine the input clock frequency to the I2C peripheral.
	 */

	ulInputClk = BT_LPC11xx_GetSystemFrequency();

	/*
	 * Determine the Baud divider. It can be 4to 254.
	 * Loop through all possible combinations
	 */

	switch (eClockrate) {
	case BT_I2C_CLOCKRATE_100kHz: {
		ulClock = 100000;
		break;
	}
	case BT_I2C_CLOCKRATE_400kHz: {
		ulClock = 400000;
		break;
	}
	case BT_I2C_CLOCKRATE_1000kHz: {
		ulClock = 1000000;
		break;
	}
	case BT_I2C_CLOCKRATE_3400kHz: {
		ulClock = 3400000;
		break;
	}
	default: {
		ulClock = 100000;
		break;
	}
	}
	ulClkPeriod = ulInputClk / ulClock;

	pRegs->LPC11xx_I2C_SCLH = ulClkPeriod / 2;
	pRegs->LPC11xx_I2C_SCLL = ulClkPeriod - pRegs->LPC11xx_I2C_SCLH;

	return BT_ERR_NONE;
}

/**
 *	This actually allows the I2CS to be clocked!
 **/
static void enablei2cPeripheralClock(BT_HANDLE hI2C) {
	const BT_RESOURCE *pResource = BT_GetIntegratedResource(hI2C->pDevice, BT_RESOURCE_ENUM, 0);

	switch(pResource->ulStart) {
	case 0: {
		LPC11xx_RCC->SYSAHBCLKCTRL |= LPC11xx_RCC_SYSAHBCLKCTRL_I2C0EN;
		break;
	}
	default: {
		break;
	}
	}
}

/**
 *	If the serial port is not in use, we can make it sleep!
 **/
static void disablei2cPeripheralClock(BT_HANDLE hI2C) {
	const BT_RESOURCE *pResource = BT_GetIntegratedResource(hI2C->pDevice, BT_RESOURCE_ENUM, 0);

	switch(pResource->ulStart) {
	case 0: {
		LPC11xx_RCC->SYSAHBCLKCTRL &= ~LPC11xx_RCC_SYSAHBCLKCTRL_I2C0EN;
		break;
	}
	default: {
		break;
	}
	}
}

/**
 *	Function to test the current peripheral clock gate status of the devices.
 **/
static BT_BOOL isi2cPeripheralClockEnabled(BT_HANDLE hI2C) {
	const BT_RESOURCE *pResource = BT_GetIntegratedResource(hI2C->pDevice, BT_RESOURCE_ENUM, 0);

	switch(pResource->ulStart) {
	case 0: {
		if(LPC11xx_RCC->SYSAHBCLKCTRL & LPC11xx_RCC_SYSAHBCLKCTRL_I2C0EN) {
			return BT_TRUE;
		}
		break;
	}
	default: {
		break;
	}
	}

	return BT_FALSE;
}

/**
 *	This implements the I2C power management interface.
 *	It is called from the BT_SetPowerState() API!
 **/
static BT_ERROR i2cSetPowerState(BT_HANDLE hI2C, BT_POWER_STATE ePowerState) {

	switch(ePowerState) {
	case BT_POWER_STATE_ASLEEP: {
		disablei2cPeripheralClock(hI2C);
		break;
	}
	case BT_POWER_STATE_AWAKE: {
		enablei2cPeripheralClock(hI2C);
		break;
	}

	default: {
		//return BT_ERR_POWER_STATE_UNSUPPORTED;
		return (BT_ERROR) -1;
	}
	}

	return BT_ERR_NONE;
}

/**
 *	This implements the I2C power management interface.
 *	It is called from the BT_GetPowerState() API!
 **/
static BT_ERROR i2cGetPowerState(BT_HANDLE hI2C, BT_POWER_STATE *pePowerState) {
	if(isi2cPeripheralClockEnabled(hI2C)) {
		return BT_POWER_STATE_AWAKE;
	}
	return BT_POWER_STATE_ASLEEP;
}

/**
 *	Complete a full configuration of the I2C.
 **/
static BT_ERROR i2cSetConfig(BT_HANDLE hI2C, BT_I2C_CONFIG *pConfig) {
	BT_ERROR Error = BT_ERR_NONE;

	i2cEnable(hI2C);

	i2cSetClockrate(hI2C, pConfig->ulClockrate);

	switch(pConfig->eMode) {
	case BT_I2C_MODE_POLLED: {
		if(hI2C->eMode !=  BT_I2C_MODE_POLLED) {

			if(hI2C->hTxFifo) {
				BT_CloseHandle(hI2C->hTxFifo);
				hI2C->hTxFifo = NULL;
			}
			if(hI2C->hRxFifo) {
				BT_CloseHandle(hI2C->hRxFifo);
				hI2C->hRxFifo = NULL;
			}

			// Disable TX and RX interrupts
			//@@pRegs->IER &= ~LPC11xx_I2C_IER_RBRIE;	// Disable the interrupt

			hI2C->eMode = BT_I2C_MODE_POLLED;
		}
		break;
	}

	case BT_I2C_MODE_BUFFERED:
	{
		if(hI2C->eMode != BT_I2C_MODE_BUFFERED) {
			if(!hI2C->hRxFifo && !hI2C->hTxFifo) {
				hI2C->hRxFifo = BT_FifoCreate(pConfig->ulRxBufferSize, 1, 0, &Error);
				hI2C->hTxFifo = BT_FifoCreate(pConfig->ulTxBufferSize, 1, 0, &Error);

				//@@pRegs->IER |= LPC11xx_I2C_IER_RBRIE;	// Enable the interrupt
				hI2C->eMode = BT_I2C_MODE_BUFFERED;
			}
		}
		break;
	}

	default:
		// Unsupported operating mode!
		break;
	}

	return BT_ERR_NONE;
}

/**
 *	Get a full configuration of the I2C.
 **/
static BT_ERROR i2cGetConfig(BT_HANDLE hI2C, BT_I2C_CONFIG *pConfig) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;

	pConfig->eMode 			= hI2C->eMode;

	BT_ERROR Error = BT_ERR_NONE;

	BT_u32 ulInputClk;

	ulInputClk = BT_LPC11xx_GetSystemFrequency();

	BT_u32 ulClkPeriod = ulInputClk / (pRegs->LPC11xx_I2C_SCLH + pRegs->LPC11xx_I2C_SCLL);

	/*switch (ulClkPeriod) {
	case 99000..101000:{
		pConfig->ulClockrate 	= BT_I2C_CLOCKRATE_100kHz;
		break;
	}
	case 399000..401000:{
		pConfig->ulClockrate 	= BT_I2C_CLOCKRATE_400kHz;
		break;
	}
	case 999000..1001000:{
		pConfig->ulClockrate 	= BT_I2C_CLOCKRATE_1000kHz;
		break;
	}
	case 3399000..3401000:{
		pConfig->ulClockrate 	= BT_I2C_CLOCKRATE_3400kHz;
		break;
	}
	}*/

	pConfig->ulTxBufferSize = BT_FifoSize(hI2C->hTxFifo, &Error);
	pConfig->ulRxBufferSize = BT_FifoSize(hI2C->hRxFifo, &Error);
	pConfig->eMode			= hI2C->eMode;


	return Error;
}

/**
 *	Make the I2C active (Set the Enable bit).
 **/
static BT_ERROR i2cEnable(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	volatile LPC11xx_RCC_REGS *pRCC = LPC11xx_RCC;

	pRCC->PRESETCTRL |= LPC11xx_RCC_PRESETCTRL_I2C_DEASSERT;

	pRegs->LPC11xx_I2C_CONSET |= LPC11xx_I2C_CONSET_I2EN;

	return BT_ERR_NONE;
}

/**
 *	Make the I2C inactive (Clear the Enable bit).
 **/
static BT_ERROR i2cDisable(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	volatile LPC11xx_RCC_REGS *pRCC = LPC11xx_RCC;

	pRCC->PRESETCTRL &= ~LPC11xx_RCC_PRESETCTRL_I2C_DEASSERT;

	pRegs->LPC11xx_I2C_CONCLR |= LPC11xx_I2C_CONSET_I2EN;

	return BT_ERR_NONE;
}

static BT_ERROR i2cStart(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_SI;

	pRegs->LPC11xx_I2C_CONSET |= LPC11xx_I2C_CONSET_STA;

	while (!(pRegs->LPC11xx_I2C_CONSET & LPC11xx_I2C_CONSET_SI));

	if ((pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_START_TRANSMITTED) &&
	    (pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_REPEAT_START_TRANSMITTED)) Error = -1;

	return Error;
}

static BT_ERROR i2cSendAddress(BT_HANDLE hI2C, BT_u32 ulAddress, BT_I2C_ACCESS_MODE eAccessMode) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	pRegs->LPC11xx_I2C_DAT = ulAddress << 1 | eAccessMode;

	pRegs->LPC11xx_I2C_CONCLR = (LPC11xx_I2C_CONCLR_STA | LPC11xx_I2C_CONCLR_SI);

	while (!(pRegs->LPC11xx_I2C_CONSET & LPC11xx_I2C_CONSET_SI));

	if ((pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_ADDRESS_W_ACK) &&
		(pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_ADDRESS_R_ACK)) Error = -1;

	return Error;
}

static BT_ERROR i2cSendNack(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_AA;	/* assert ACK after data is received */

	pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_SI;

	return Error;
}

static BT_ERROR i2cSendAck(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	pRegs->LPC11xx_I2C_CONSET = LPC11xx_I2C_CONSET_AA;	/* assert ACK after data is received */

	pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_SI;

	return Error;
}

static BT_ERROR i2cGetData(BT_HANDLE hI2C, BT_u8 *pDest, BT_u32 ulLength) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	while (ulLength--) {
		if (ulLength) i2cSendAck(hI2C);
		else i2cSendNack(hI2C);
		while (!(pRegs->LPC11xx_I2C_CONSET & LPC11xx_I2C_CONSET_SI));
		*pDest++ = pRegs->LPC11xx_I2C_DAT & 0xFF;
		if (pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_DATA_W_ACK) Error = -1;
	}

	return Error;
}

static BT_ERROR i2cSendData(BT_HANDLE hI2C, BT_u8 *pSrc, BT_u32 ulLength) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	while (ulLength--) {
		pRegs->LPC11xx_I2C_DAT = *pSrc++;

		pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_SI;

		while (!(pRegs->LPC11xx_I2C_CONSET & LPC11xx_I2C_CONSET_SI));

		if (pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_DATA_W_ACK) Error = -1;
	}

	return Error;
}


static BT_BOOL i2cGetAck(BT_HANDLE hI2C, BT_ERROR *pError) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	if (pError) *pError = BT_ERR_NONE;

	if ((pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_ADDRESS_W_ACK) &&
		(pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_ADDRESS_R_ACK) &&
		(pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_DATA_W_ACK   ) &&
		(pRegs->LPC11xx_I2C_STAT != LPC11xx_I2C_STAT_DATA_R_ACK   )) return BT_FALSE;

	return BT_TRUE;
}

static BT_ERROR i2cStop(BT_HANDLE hI2C) {
	volatile LPC11xx_I2C_REGS *pRegs = hI2C->pRegs;
	BT_ERROR Error = BT_ERR_NONE;

	pRegs->LPC11xx_I2C_CONSET |= LPC11xx_I2C_CONSET_STO;

	pRegs->LPC11xx_I2C_CONCLR = LPC11xx_I2C_CONCLR_SI;

	return Error;
}


static BT_ERROR i2cRead(BT_HANDLE hI2C, BT_u8 ucDevice, BT_u8 *pucDest, BT_u32 ulLength) {

	BT_ERROR Error = BT_ERR_NONE;

	switch(hI2C->eMode) {
	case BT_I2C_MODE_POLLED:
	{
		i2cStart(hI2C);
		Error = i2cSendAddress(hI2C, ucDevice, BT_I2C_READ_ACCESS);
		if (Error) goto err_out;
		Error = i2cGetData(hI2C, pucDest, ulLength);
		if (Error) goto err_out;
		Error = i2cStop(hI2C);

		break;
	}

	case BT_I2C_MODE_BUFFERED:
	{
		// Get bytes from RX buffer very quickly.
		break;
	}

	default:
		// ERR, invalid handle configuration.
		break;
	}
	return Error;

err_out:
	i2cStop(hI2C);
	return Error;
}

/**
 *	Implementing the CHAR dev write API.
 *
 *	Note, this doesn't implement ulFlags specific options yet!
 **/
static BT_ERROR i2cWrite(BT_HANDLE hI2C, BT_u8 ucDevice, BT_u8 *pucSource, BT_u32 ulLength) {
	BT_ERROR Error = BT_ERR_NONE;

	switch(hI2C->eMode) {
	case BT_I2C_MODE_POLLED:
	{
		i2cStart(hI2C);
		Error = i2cSendAddress(hI2C, ucDevice, BT_I2C_WRITE_ACCESS);
		if (Error) goto err_out;
		Error = i2cSendData(hI2C, pucSource, ulLength);
		if (Error) goto err_out;
		Error = i2cStop(hI2C);
		break;
	}

	case BT_I2C_MODE_BUFFERED:
	{
		break;
	}

	default:
		break;
	}
	return Error;

err_out:
	i2cStop(hI2C);
	return Error;
}


static const BT_DEV_IF_I2C oI2CConfigInterface = {
	.pfnSetClockrate	= i2cSetClockrate,											///< I2C setBaudrate implementation.
	.pfnSetConfig		= i2cSetConfig,												///< I2C set config imple.
	.pfnGetConfig		= i2cGetConfig,
	.pfnEnable			= i2cEnable,													///< Enable/disable the device.
	.pfnDisable			= i2cDisable,
	.pfnRead			= i2cRead,
	.pfnWrite			= i2cWrite,
	.pfnStart 			= i2cStart,
	.pfnSendAddress		= i2cSendAddress,
	.pfnGetData			= i2cGetData,
	.pfnSendData		= i2cSendData,
	.pfnSendNack		= i2cSendNack,
	.pfnSendAck			= i2cSendAck,
	.pfnGetAck			= i2cGetAck,
	.pfnStop			= i2cStop,
};

static const BT_IF_POWER oPowerInterface = {
	i2cSetPowerState,											///< Pointers to the power state API implementations.
	i2cGetPowerState,											///< This gets the current power state.
};


static const BT_DEV_IFS oConfigInterface = {
	(BT_DEV_INTERFACE) &oI2CConfigInterface,
};

const BT_IF_DEVICE BT_LPC11xx_I2C_oDeviceInterface = {
	&oPowerInterface,											///< Device does not support powerstate functionality.
	BT_DEV_IF_T_I2C,											///< Allow configuration through the I2C api.
	.unConfigIfs = {
		(BT_DEV_INTERFACE) &oI2CConfigInterface,
	},
	NULL,														///< Provide a Character device interface implementation.
};


static const BT_IF_HANDLE oHandleInterface = {
	BT_MODULE_DEF_INFO,
	.oIfs = {
		(BT_HANDLE_INTERFACE) &BT_LPC11xx_I2C_oDeviceInterface,
	},
	BT_HANDLE_T_DEVICE,											///< Handle Type!
	i2cCleanup,												///< Handle's cleanup routine.
};

static BT_HANDLE i2c_probe(const BT_INTEGRATED_DEVICE *pDevice, BT_ERROR *pError) {

	BT_ERROR Error = BT_ERR_NONE;
	BT_HANDLE hI2C = NULL;


	const BT_RESOURCE *pResource = BT_GetIntegratedResource(pDevice, BT_RESOURCE_ENUM, 0);
	if(!pResource) {
		Error = BT_ERR_NO_MEMORY;
		goto err_free_out;
	}

	if (g_I2C_HANDLES[pResource->ulStart]){
		Error = BT_ERR_GENERIC;
		goto err_free_out;
	}

	hI2C = BT_CreateHandle(&oHandleInterface, sizeof(struct _BT_OPAQUE_HANDLE), pError);
	if(!hI2C) {
		goto err_out;
	}

	g_I2C_HANDLES[pResource->ulStart] = hI2C;

	hI2C->pDevice = pDevice;

	pResource = BT_GetIntegratedResource(pDevice, BT_RESOURCE_MEM, 0);
	if(!pResource) {
		Error = BT_ERR_NO_MEMORY;
		goto err_free_out;
	}

	hI2C->pRegs = (LPC11xx_I2C_REGS *) pResource->ulStart;

	i2cSetPowerState(hI2C, BT_POWER_STATE_AWAKE);

	// Reset all registers to their defaults!

	ResetI2C(hI2C);

	pResource = BT_GetIntegratedResource(pDevice, BT_RESOURCE_IRQ, 0);
	if(!pResource) {
		Error = BT_ERR_GENERIC;
		goto err_free_out;
	}

/*	On NVIC we don't need to register interrupts, LINKER has patched vector for us
 * Error = BT_RegisterInterrupt(pResource->ulStart, I2C_irq_handler, hI2C);
	if(Error) {
		goto err_free_out;
	}*/


	//@@Error = BT_EnableInterrupt(pResource->ulStart);

	return hI2C;

err_free_out:
	BT_DestroyHandle(hI2C);

err_out:
	if(pError) {
		*pError = Error;
	}
	return NULL;
}

BT_INTEGRATED_DRIVER_DEF I2C_driver = {
	.name 		= "LPC11xx,i2c",
	.pfnProbe	= i2c_probe,
};


#ifdef BT_CONFIG_MACH_LPC11xx_I2C_0
static const BT_RESOURCE oLPC11xx_i2c0_resources[] = {
	{
		.ulStart 			= BT_CONFIG_MACH_LPC11xx_I2C0_BASE,
		.ulEnd 				= BT_CONFIG_MACH_LPC11xx_I2C0_BASE + BT_SIZE_4K - 1,
		.ulFlags 			= BT_RESOURCE_MEM,
	},
	{
		.ulStart			= 0,
		.ulEnd				= 0,
		.ulFlags			= BT_RESOURCE_ENUM,
	},
	{
		.ulStart			= 31,
		.ulEnd				= 31,
		.ulFlags			= BT_RESOURCE_IRQ,
	},
};

static const BT_INTEGRATED_DEVICE oLPC11xx_i2c0_device = {
	.id						= 0,
	.name 					= "LPC11xx,i2c",
	.ulTotalResources 		= BT_ARRAY_SIZE(oLPC11xx_i2c0_resources),
	.pResources 			= oLPC11xx_i2c0_resources,
};

const BT_DEVFS_INODE_DEF oLPC11xx_i2c0_inode = {
	.szpName = "i2c0",
	.pDevice = &oLPC11xx_i2c0_device,
};
#endif
