/* Copyright (c) 2022 Michael Davidsaver
 *
 * TODO - What about license?
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rtems.org/license/LICENSE.
 */

#include <errno.h>
#include "marble_api.h"
#include "flash.h"
#include "stm32f2xx_hal.h"

#define DEBUG_PRINT
#include "dbg.h"
#undef DEBUG_PRINT

#define FLASH_ERROR_TIMEOUT       0xFFFFFF

static int fmc_flash_unlock(volatile FLASH_TypeDef * const hw);
static void fmc_flash_lock(volatile FLASH_TypeDef * const hw);
static int fmc_flash_wait_idle(volatile FLASH_TypeDef * const hw);

int fmc_flash_init(void) {
  // TODO anything?
  return 0;
}

static int fmc_flash_unlock(volatile FLASH_TypeDef * const hw)
{
  int ret = 0;
  INTERRUPTS_DISABLE();

  if((hw->CR) & FLASH_CR_LOCK) {
    hw->KEYR = FLASH_KEY1; //0x45670123;
    hw->KEYR = FLASH_KEY2; //0xcdef89ab;

    if((hw->CR) & FLASH_CR_LOCK)
      ret = -EIO;

  } else {
    // concurrent operation in progress
    ret = -EBUSY;
  }

  INTERRUPTS_ENABLE();

  return ret;
}

static void fmc_flash_lock(volatile FLASH_TypeDef * const hw)
{
  INTERRUPTS_DISABLE();

  hw->CR &= ~(FLASH_CR_PG|FLASH_CR_SER|FLASH_CR_MER);

  hw->CR |= FLASH_CR_LOCK;

  INTERRUPTS_ENABLE();
}

static int fmc_flash_wait_idle(volatile FLASH_TypeDef * const hw)
{
  /* Flash operations are slow, but also prevent/slow down
   * normal execution.
   * TODO: Does sleeping vs. spinning have a practical benefit?
   */
  int cnt = 0;
  int to = FLASH_ERROR_TIMEOUT;   // IMPORTANT! We need this value in RAM not flash
  uint32_t srbit = FLASH_SR_BSY;  // This too.
  while(hw->SR & srbit) {
    if (cnt++ > to) { // If we read from flash here, we stall the bus
      return -EDEADLK;
    }
  }

  FLASH->SR |= FLASH_SR_PGPERR;  // Clear the PGPERR bit
  if(hw->SR & (FLASH_SR_OPERR|FLASH_SR_PGAERR
               |FLASH_SR_PGPERR|FLASH_SR_PGSERR
               |FLASH_SR_WRPERR))
  {
    printd("FLASH_SR ERR 0x%lx\r\n", hw->SR);
    return -EIO;
  }
  return 0;
}

int fmc_flash_program(void *paddr, const void *pvalue, size_t count)
{
  volatile FLASH_TypeDef * const hw = FLASH; /* see "stm32f207xx.h" */
  uint8_t psize = 0;

  // We will do pointer math on these below, so we need to tell them
  // how to increment/decrement
  uint8_t *value = (uint8_t *)pvalue;
  uint8_t *addr = (uint8_t *)paddr;

  /* use multi-byte if Vcc is high enough, and request is aligned */
  if(FLASH_VOLTAGE_MV > 2700 && (3&(size_t)addr)==0 && (3&count)==0) {
    psize = 2;
  } else if(FLASH_VOLTAGE_MV > 2100 && (1&(size_t)addr)==0 && (1&count)==0) {
    psize = 1;
  }
  uint8_t progwidth = 1u<<psize;

  int ret = fmc_flash_unlock(hw);
  printd("unlock? ret = %d\r\n", ret);
  if(ret) {
    return ret;
  }

  // TODO - Why the double-negative?  Does it evaluate differently than
  //        it would with no !'s?
  if(!!(ret = fmc_flash_wait_idle(hw))) {
    goto done;
  }

  hw->SR |= FLASH_SR_OPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR;

  INTERRUPTS_DISABLE();
  for(; count; count -= progwidth, value += progwidth, addr += progwidth) {
    uint32_t cr = hw->CR;
    cr  = FLASH_CR_PSIZE_SET(cr, psize);
    cr |= FLASH_CR_PG;
    cr &= ~(FLASH_CR_SER|FLASH_CR_MER);
    hw->CR = cr;

    switch(progwidth) {
    case 1: *(uint8_t*)addr  = *(const uint8_t*)value; break;
    case 2: *(uint16_t*)addr = *(const uint16_t*)value; break;
    case 4: *(uint32_t*)addr = *(const uint32_t*)value; break;
    default: ret = -EINVAL; goto done;
    }

    if(!!(ret = fmc_flash_wait_idle(hw))) {
      goto done;
    }
  }
done:
  fmc_flash_lock(hw);  //calls INTERRUPTS_ENABLE();
  return ret;
}

int fmc_flash_erase_sector(unsigned sectorn)
{
  volatile FLASH_TypeDef * const hw = FLASH; /* see "stm32f207xx.h" */
  uint8_t psize = 0;
  unsigned maxn = FLASH_CR_SNB_GET(0xffffffff);

  if(sectorn >= maxn) {
    return -EINVAL;
  }

  /* use multi-byte if Vcc is high enough */
  if(FLASH_VOLTAGE_MV > 2700) {
    psize = 2;
  } else if(FLASH_VOLTAGE_MV > 2100) {
    psize = 1;
  }

  int ret = fmc_flash_unlock(hw);
  if(ret) {
    return ret;
  }

  if(!!(ret = fmc_flash_wait_idle(hw))) {
    goto done;
  }

  hw->SR |=FLASH_SR_OPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR;

  uint32_t cr = hw->CR;
  cr  = FLASH_CR_PSIZE_SET(cr, psize);
  cr  = FLASH_CR_SNB_SET(cr, sectorn);
  cr |= FLASH_CR_SER;
  cr &= ~(FLASH_CR_PG|FLASH_CR_MER);
  hw->CR = cr;
  INTERRUPTS_DISABLE();
  hw->CR |= FLASH_CR_STRT;

  ret = fmc_flash_wait_idle(hw);

done:
  fmc_flash_lock(hw);  // calls INTERRUPTS_ENABLE();
  return ret;
}

void fmc_flash_cache_flush_all(void)
{
  volatile FLASH_TypeDef * const hw = FLASH; /* see "stm32f207xx.h" */
  const uint32_t cr = hw->CR;

  /* ensure disabled before resetting */
  hw->CR &= ~(FLASH_ACR_DCEN | FLASH_ACR_ICEN);

  uint32_t mask = 0u;
  if(cr & FLASH_ACR_DCEN)
      mask |= FLASH_ACR_DCRST;
  if(cr & FLASH_ACR_ICEN)
      mask |= FLASH_ACR_ICRST;

  /* toggle resets */
  hw->CR |= mask;
  hw->CR &= ~mask;

  /* (maybe) re-enable */
  hw->CR = cr;
  return;
}

int restore_flash(void) {
  // no-op (used in simulation)
  return 0;
}

