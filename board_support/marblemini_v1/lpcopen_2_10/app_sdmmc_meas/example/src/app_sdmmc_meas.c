/*
 * @brief SD/MMC benachmark example
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
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

#include <string.h>
#include "board.h"
#include "chip.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#define debugstr(str)  DEBUGOUT(str)

/* Number of sectors to read/write */
#define NUM_SECTORS     256

/* Number of iterations of measurement */
#define NUM_ITER        16

/* Starting sector number for read/write */
#define START_SECTOR    32

/* Buffer size (in bytes) for R/W operations */
#define BUFFER_SIZE     (NUM_SECTORS * MMC_SECTOR_SIZE)

/* Buffers to store original data of SD/MMC card.
 * The data will be stored in this buffer, once read/write measurement
 * completed, the original contents will be restored into SD/MMC card.
 * This is done in order avoid corurupting the SD/MMC card
 */
static uint32_t *Buff_Backup = (uint32_t *)0xA0000000;

/* Buffers for read/write operation */
static uint32_t *Buff_Rd = (uint32_t *)0xA0100000;
static uint32_t *Buff_Wr = (uint32_t *)0xA0200000;

/* Measurement data */
static uint32_t rd_time_ms[NUM_ITER];
static uint32_t wr_time_ms[NUM_ITER];

/* SDC wait flag */
STATIC volatile int32_t sdcWaitExit = 0;

/* SDMMC event structure pointer */
STATIC SDMMC_EVENT_T *event;

/* Event result flag */
STATIC volatile Status  eventResult = ERROR;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* SDMMC card info structure */
SDMMC_CARD_T sdCardInfo;

/* Free running millisecond timer */
volatile uint32_t timerCntms = 0;

/*****************************************************************************
 * Private functions
 ****************************************************************************/
/* Delay callback for timed SDIF/SDMMC functions */
STATIC void waitMs(uint32_t time)
{
	uint32_t init = timerCntms;

	/* In an RTOS, the thread would sleep allowing other threads to run.
	   For standalone operation, we just spin on a timer */
	while (timerCntms < (init + time )) {}
}

/* Function which sets up the event driven wakeup */
STATIC void setupEvWakeup(void *pEvent)
{
#ifdef SDC_DMA_ENABLE
	/* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
	NVIC_ClearPendingIRQ(DMA_IRQn);
#endif
	event = (SDMMC_EVENT_T *)pEvent;
	sdcWaitExit = 0;
	eventResult = ERROR;
#ifdef SDC_DMA_ENABLE
	NVIC_EnableIRQ(DMA_IRQn);
#endif
}

/* Wait callback function for SDMMC driven by the IRQ flag.
 * Return 0 on success, or failure condition (Nonzero)
 */
STATIC uint32_t waitEvIRQDriven(void)
{
	/* Wait for event, would be nice to have a timeout, but keep it  simple */
	while (sdcWaitExit == 0) {}
	if (eventResult) {
		return 0;
	}

	return 1;
}

/* Initialize the Timer at 1ms */
STATIC void initAppTimer(void)
{
	/* Setup Systick to tick every 1 microseconds */
	SysTick_Config(SystemCoreClock / 1000);
}

/* Initialize SD/MMC */
STATIC void initAppSDMMC()
{
	memset(&sdCardInfo, 0, sizeof(sdCardInfo));
	sdCardInfo.evsetup_cb = setupEvWakeup;
	sdCardInfo.waitfunc_cb = waitEvIRQDriven;
	sdCardInfo.msdelay_func = waitMs;

	Board_SDC_Init();

	Chip_SDC_Init(LPC_SDC);
}

/* Buffer initialisation function */
static void Prepare_Buffer(uint32_t value)
{
    uint32_t i;

    for(i = 0; i < (BUFFER_SIZE/sizeof(uint32_t)); i++)
    {
        Buff_Rd[i] = 0x0;
        Buff_Wr[i] = i + value;
    }
}

/* Print the result of a data transfer */
STATIC void print_meas_data(void)
{
	static char debugBuf[64];
    uint64_t tot_sum_rd, tot_sum_wr;
    uint32_t i, rd_ave, wr_ave;
	uint32_t clk = SystemCoreClock/1000000;

    /* Print Number of Interations */
	debugstr("\r\n=====================\r\n");
    debugstr("SDMMC Measurements \r\n");
	debugstr("=====================\r\n");
	sprintf(debugBuf, "No. of Iterations: %u \r\n", NUM_ITER);
	debugstr(debugBuf);
    sprintf(debugBuf, "No. of Sectors for R/W: %u \r\n", NUM_SECTORS);
    debugstr(debugBuf);
	sprintf(debugBuf, "Sector size : %u bytes \r\n", MMC_SECTOR_SIZE);
	debugstr(debugBuf);
	sprintf(debugBuf, "Data Transfered : %u bytes\r\n", (MMC_SECTOR_SIZE * NUM_SECTORS));
	debugstr(debugBuf);
    tot_sum_rd = tot_sum_wr = 0;
    for(i = 0; i < NUM_ITER; i++) {
        tot_sum_rd += rd_time_ms[i];
        tot_sum_wr += wr_time_ms[i];
    }
    rd_ave = tot_sum_rd / NUM_ITER;
    wr_ave = tot_sum_wr / NUM_ITER;
	sprintf(debugBuf, "CPU Speed: %lu.%lu MHz\r\n", clk, (SystemCoreClock / 10000) - (clk * 100));
	debugstr(debugBuf);
	sprintf(debugBuf, "READ: Ave Time: %u msecs Ave Speed : %u KB/sec\r\n", rd_ave, ((NUM_SECTORS * MMC_SECTOR_SIZE)/rd_ave));
    debugstr(debugBuf);
	sprintf(debugBuf, "WRITE:Ave Time: %u msecs Ave Speed : %u KB/sec \r\n", wr_ave, ((NUM_SECTORS * MMC_SECTOR_SIZE)/wr_ave));
    debugstr(debugBuf);
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	System tick interrupt handler
 * @return	Nothing
 */
void SysTick_Handler(void)
{
	timerCntms++;
}

#ifdef SDC_DMA_ENABLE
/**
 * @brief	GPDMA interrupt handler sub-routine
 * @return	Nothing
 */
void DMA_IRQHandler(void)
{
	eventResult = Chip_GPDMA_Interrupt(LPC_GPDMA, event->DmaChannel);
	sdcWaitExit = 1;
	NVIC_DisableIRQ(DMA_IRQn);
}
#endif /*SDC_DMA_ENABLE*/

/**
 * @brief	SDC interrupt handler sub-routine
 * @return	Nothing
 */
void SDIO_IRQHandler(void)
{
	int32_t Ret;
#ifdef SDC_DMA_ENABLE
	Ret = Chip_SDMMC_IRQHandler(LPC_SDC, NULL,0,NULL,0);
#else
	if(event->Index < event->Size) {
		if(event->Dir) { /* receive */
			Ret = Chip_SDMMC_IRQHandler(LPC_SDC, NULL,0,(uint8_t*)event->Buffer,&event->Index);
		}
		else {
			Ret = Chip_SDMMC_IRQHandler(LPC_SDC, (uint8_t*)event->Buffer,&event->Index,NULL,0);
		}
	}
	else {
		Ret = Chip_SDMMC_IRQHandler(LPC_SDC, NULL,0,NULL,0);
	}
#endif
	if(Ret < 0) {
		eventResult = ERROR;
		sdcWaitExit = 1;
	}
#ifndef SDC_DMA_ENABLE
	else if(Ret == 0) {
		eventResult = SUCCESS;
		sdcWaitExit = 1;
	}
#endif
}

/**
 * @brief	Main routine for SDMMC example
 * @return	Nothing
 */
int main(void)
{
    uint32_t rc;	/* Result code */
    int32_t act_read, act_written;
    uint32_t i, ite_cnt;
	uint32_t start_time;
	uint32_t end_time;
	static char debugBuf[64];
	uint8_t ch;
    uint32_t backup = 0;

    /* Initialise Board */
	SystemCoreClockUpdate();
	Board_Init();

    /* Initialise SD/MMC */
	initAppSDMMC();

	/* Initialize SysTick Timer */
	initAppTimer();

	debugstr("\r\n==============================\r\n");
	debugstr("SDMMC CARD measurement demo\r\n");
	debugstr("==============================\r\n");

	/* Enable SD interrupt */
	NVIC_EnableIRQ(SDC_IRQn);

    /* Wait for a card to be inserted and
     * input from user to continue */
	debugstr("\r\nInsert SD/MMC card, then press 'C' or 'c' to continue...\r\n");
    do{
        ch = DEBUGIN();
    } while((ch != 'C') && (ch != 'c'));

    /* Enumerate the SDMMC card.
     * Note this function may block for a little while. */
	rc = Chip_SDMMC_Acquire(LPC_SDC, &sdCardInfo);
	if (!rc) {
	    debugstr("SD/MMC Card enumeration failed! ..\r\n");
		goto error_exit;
	}

    /* Take back up of SD/MMC card contents so that
     * it can be restored so that SD/MMC card is not corrupted
     */
    debugstr("\r\nTaking back up of card.. \r\n");
    act_read = Chip_SDMMC_ReadBlocks(LPC_SDC, &sdCardInfo, (void *)Buff_Backup, START_SECTOR, NUM_SECTORS);
    if(act_read == 0) {
        debugstr("Taking back up of card failed!.. \r\n");
		goto error_exit;
    }

    ite_cnt = 0;
    backup = 1;
    while(ite_cnt < NUM_ITER) {
        /* Prepare R/W buffers */
        Prepare_Buffer(ite_cnt);

        /* Write data to SD/MMC card */
	    start_time = timerCntms;
        act_written = Chip_SDMMC_WriteBlocks(LPC_SDC, &sdCardInfo, (void *)Buff_Wr, START_SECTOR, NUM_SECTORS);
	    end_time = timerCntms;
        if(act_written == 0) {
            sprintf(debugBuf, "WriteBlocks failed for Iter: %u! \r\n", ite_cnt);
	        debugstr(debugBuf);
		    goto error_exit;
        }

        if(end_time < start_time) {
            continue;
        }
        else {
            wr_time_ms[ite_cnt] = end_time - start_time;
        }

        /* Read data from SD/MMC card */
	    start_time = timerCntms;
        act_read = Chip_SDMMC_ReadBlocks(LPC_SDC, &sdCardInfo, (void *)Buff_Rd, START_SECTOR, NUM_SECTORS);
	    end_time = timerCntms;
        if(act_read == 0) {
            sprintf(debugBuf, "ReadBlocks failed for Iter: %u! \r\n", ite_cnt);
	        debugstr(debugBuf);
		    goto error_exit;
        }

        if(end_time < start_time) {
            continue;
        }
        else {
            rd_time_ms[ite_cnt] = end_time - start_time;
        }

        /* Comapre data */
        for(i = 0; i < (BUFFER_SIZE/sizeof(uint32_t)); i++)
        {
            if(Buff_Rd[i] != Buff_Wr[i])
            {
                sprintf(debugBuf, "Data mismacth: ind: %u Rd: 0x%x Wr: 0x%x \r\n", i, Buff_Rd[i], Buff_Wr[i]);
	            debugstr(debugBuf);
		        goto error_exit;
            }
        }
        ite_cnt++;
    }

	/* Print Measurement onto UART */
    print_meas_data();

error_exit:
    /* Restore if back up taken */
    if(backup) {
        debugstr("\r\nRestoring the contents of SDMMC card... \r\n");
        act_written = Chip_SDMMC_WriteBlocks(LPC_SDC, &sdCardInfo, (void *)Buff_Backup, START_SECTOR, NUM_SECTORS);
        if(act_written == 0) {
            debugstr("Restoring contents failed!.. \r\n");
        }
    }

	debugstr("\r\n========================================\r\n");
	debugstr("SDMMC CARD measurement demo completed\r\n");
	debugstr("========================================\r\n");

	for (;; ) {}
}

/**
 * @}
 */
