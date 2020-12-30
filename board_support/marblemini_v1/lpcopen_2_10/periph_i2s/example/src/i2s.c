/*
 * @brief I2S example
 * This example show how to use the I2S in 3 modes : Polling, Interrupt and DMA
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "board.h"
#include "uda1380.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#define BUFFER_FULL 0
#define BUFFER_EMPTY 1
#define BUFFER_AVAILABLE 2

typedef struct ring_buff {
	uint32_t buffer[256];
	uint8_t read_index;
	uint8_t write_index;
} Ring_Buffer_t;

static char WelcomeMenu[] = "\n\rHello NXP Semiconductors \n\r"
							"I2S DEMO : Connect audio headphone out from computer to line-in on tested board to get audio signal\n\r"
							"Please press \'1\' to test Polling mode\n\r"
							"Please press \'2\' to test Interrupt mode\n\r"
							"Please press \'3\' to test DMA mode\n\r"
							"Please press \'x\' to exit test mode\n\r"
							"Please press \'m\' to DISABLE/ENABLE mute\n\r";

static Ring_Buffer_t ring_buffer;

static uint8_t send_flag;
static uint8_t channelTC;
static uint8_t dmaChannelNum_I2S_Tx, dmaChannelNum_I2S_Rx;
static uint8_t dma_send_receive;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

uint8_t mute_status = 0;

static void mute_toggle()
{
	mute_status = !mute_status;
	if (mute_status) {
		Chip_I2S_EnableMute(LPC_I2S);
		DEBUGOUT("MUTE ENABLE\n\r");
	}
	else {
		Chip_I2S_DisableMute(LPC_I2S);
		DEBUGOUT("MUTE DISABLE\n\r");
	}
}
/* Get status of the ring buffer */
static uint8_t ring_buff_get_status(Ring_Buffer_t *ring_buff)
{
	if (ring_buff->read_index == ring_buff->write_index) {
		return BUFFER_EMPTY;
	}
	else if (ring_buff->read_index == (ring_buff->write_index) + 1) {
		return BUFFER_FULL;
	}
	else {return BUFFER_AVAILABLE; }
}

/* Interrupt routine for I2S example */
static void App_Interrupt_Test(void)
{
	uint8_t bufferUART, continue_Flag = 1;
	DEBUGOUT("I2S Interrupt mode\n\r");

	Chip_I2S_Int_RxCmd(LPC_I2S, ENABLE, 4);
	Chip_I2S_Int_TxCmd(LPC_I2S, ENABLE, 4);
	NVIC_EnableIRQ(I2S_IRQn);
	while (continue_Flag) {
		bufferUART = 0xFF;
		bufferUART = DEBUGIN();
		switch (bufferUART) {
		case 'x':
			continue_Flag = 0;
			Chip_I2S_Int_RxCmd(LPC_I2S, DISABLE, 4);
			NVIC_DisableIRQ(I2S_IRQn);
			DEBUGOUT(WelcomeMenu);
			break;

		case 'm':
			mute_toggle();
			break;

		default:
			break;
		}
	}
}

/* Polling routine for I2S example */
static void App_Polling_Test(void)
{
	uint32_t polling_data = 0;
	uint8_t bufferUART, continue_Flag = 1;
	DEBUGOUT("I2S Polling mode\n\r");
	while (continue_Flag) {
		bufferUART = 0xFF;
		bufferUART = DEBUGIN();
		switch (bufferUART) {
		case 'x':
			continue_Flag = 0;
			DEBUGOUT(WelcomeMenu);
			break;

		case 'm':
			mute_toggle();
			break;

		default:
			break;
		}

		if (Chip_I2S_GetRxLevel(LPC_I2S) > 0) {
			polling_data = Chip_I2S_Receive(LPC_I2S);
			send_flag = 1;
		}
		if ((Chip_I2S_GetTxLevel(LPC_I2S) < 4) && (send_flag == 1)) {
			Chip_I2S_Send(LPC_I2S, polling_data);
			send_flag = 0;
		}
	}
}

/* DMA routine for I2S example */
static void App_DMA_Test(void)
{
	uint8_t continue_Flag = 1, bufferUART = 0xFF;

	Chip_I2S_DMA_TxCmd(LPC_I2S, I2S_DMA_REQUEST_CHANNEL_1, ENABLE, 4);
	Chip_I2S_DMA_RxCmd(LPC_I2S, I2S_DMA_REQUEST_CHANNEL_2, ENABLE, 4);

	/* Initialize GPDMA controller */
	Chip_GPDMA_Init(LPC_GPDMA);

	/* Setting GPDMA interrupt */
	NVIC_DisableIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, ((0x01 << 3) | 0x01));
	NVIC_EnableIRQ(DMA_IRQn);

	dmaChannelNum_I2S_Rx = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_I2S_Channel_1);

	Chip_GPDMA_Transfer(LPC_GPDMA, dmaChannelNum_I2S_Rx,
					  GPDMA_CONN_I2S_Channel_1,
					  GPDMA_CONN_I2S_Channel_0,
					  GPDMA_TRANSFERTYPE_P2P_CONTROLLER_SrcPERIPHERAL,
					  1);

	DEBUGOUT("I2S DMA mode\n\r");
	while (continue_Flag) {
		bufferUART = 0xFF;
		bufferUART = DEBUGIN();
		switch (bufferUART) {
		case 'x':
			continue_Flag = 0;
			Chip_I2S_DMA_RxCmd(LPC_I2S, I2S_DMA_REQUEST_CHANNEL_2, DISABLE, 1);
			Chip_I2S_DMA_TxCmd(LPC_I2S, I2S_DMA_REQUEST_CHANNEL_1, DISABLE, 1);

			Chip_GPDMA_Stop(LPC_GPDMA, dmaChannelNum_I2S_Rx);
			NVIC_DisableIRQ(DMA_IRQn);
			DEBUGOUT(WelcomeMenu);
			break;

		case 'm':
			mute_toggle();
			break;

		default:
			break;
		}
	}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	DMA interrupt handler sub-routine
 * @return	Nothing
 */
void DMA_IRQHandler(void)
{
	if (dma_send_receive == 1) {
		if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChannelNum_I2S_Rx) == SUCCESS) {
			channelTC++;
		}
		else {
			/* Process error here */
		}
	}
	else {
		if (Chip_GPDMA_Interrupt(LPC_GPDMA, dmaChannelNum_I2S_Tx) == SUCCESS) {
			channelTC++;
		}
		else {
			/* Process error here */
		}
	}
}

/**
 * @brief	I2S interrupt handler sub-routine
 * @return	Nothing
 */
void I2S_IRQHandler(void)
{
	while ((ring_buff_get_status(&ring_buffer) != BUFFER_FULL) && (Chip_I2S_GetRxLevel(LPC_I2S) > 0)) {
		ring_buffer.buffer[ring_buffer.write_index++] = Chip_I2S_Receive(LPC_I2S);
	}
	while ((ring_buff_get_status(&ring_buffer) != BUFFER_EMPTY) && (Chip_I2S_GetTxLevel(LPC_I2S) < 8)) {
		Chip_I2S_Send(LPC_I2S, ring_buffer.buffer[ring_buffer.read_index++]);
	}
}

/**
 * @brief  Main routine for I2S example
 * @return Nothing
 */
int main(void)
{

	I2S_AUDIO_FORMAT_T audio_Confg;
	uint8_t bufferUART, continue_Flag = 1;
	audio_Confg.SampleRate = 48000;
	/* Select audio data is 2 channels (1 is mono, 2 is stereo) */
	audio_Confg.ChannelNumber = 2;
	/* Select audio data is 16 bits */
	audio_Confg.WordWidth = 16;

	SystemCoreClockUpdate();
	Board_Init();

#if defined( __GNUC__ )
	__sys_write(0, "", 0);
#endif

	DEBUGOUT(WelcomeMenu);

	Board_Audio_Init(LPC_I2S, UDA1380_LINE_IN);
	Chip_I2S_Init(LPC_I2S);
	Chip_I2S_RxConfig(LPC_I2S, &audio_Confg);
	Chip_I2S_TxConfig(LPC_I2S, &audio_Confg);

	Chip_I2S_TxStop(LPC_I2S);
	Chip_I2S_DisableMute(LPC_I2S);
	Chip_I2S_TxStart(LPC_I2S);
	send_flag = 0;
	while (continue_Flag) {
		bufferUART = 0xFF;
		bufferUART = DEBUGIN();
		switch (bufferUART) {
		case '1':
			App_Polling_Test();
			break;

		case '2':
			App_Interrupt_Test();
			break;

		case '3':
			App_DMA_Test();
			break;

		case 'x':
			continue_Flag = 0;
			DEBUGOUT("Thanks for using\n\r");
			break;

		default:
			break;
		}
	}
	return 0;
}
