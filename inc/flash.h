/**
 * @file
 * @ingroup fmc_flash
 * @brief Flash support.
 */

/*
 * TODO - Copyright?
 * Copyright (c) 2022 Michael Davidsaver  All rights reserved.
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rtems.org/license/LICENSE.
 */

#ifndef __FLASH_H
#define __FLASH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// NOTE: The stm32f207xx.h device header from STM32-HAL library defines
//       bit 1 of FLASH_SR register using nomenclature inconsistent with
//       the datasheet PM0059 where the flash interface registers are
//       defined.  This define 'fixes' that.
#define FLASH_SR_OPERR                        FLASH_SR_SOP
// NOTE: The stm32f207xx.h device header from STM32-HAL library defines
//       the FLASH->CR->SNB field width to be 5 bits (3 to 7), inconsistent
//       with the datasheet PM0059 which defines it as 4 bits (3 to 6).
//       This define 'fixes' that.
#define FLASH_CR_SNB_Msk_Fix                  (0xFU << FLASH_CR_SNB_Pos)

// Convenience function
#define FLASH_CR_PSIZE_SET(reg, val)          SET_FIELD_MASK(reg, val << FLASH_CR_PSIZE_Pos, FLASH_CR_PSIZE_Msk)
#define FLASH_CR_SNB_SET(reg, val)            SET_FIELD_MASK(reg, val << FLASH_CR_SNB_Pos, FLASH_CR_SNB_Msk_Fix)
#define FLASH_CR_SNB_GET(val)                 EXTRACT_FIELD(val, FLASH_CR_SNB_Pos, FLASH_CR_SNB_Pos + 3)

/** TODO - Remove/modify mds's doxygen refs?
 * @defgroup fmc_flash Flash Support
 * @ingroup RTEMSBSPsARMSTM32F4
 * @brief Flash Support
 * @{
 */

/**
 * @brief Currently no-op on hardware. Used for sim mode.
 */
int fmc_flash_init(void);

/** @brief Write to FLASH
 * @param addr Address of first byte to be written
 * @param value pointer to byte value to write
 * @param count Number of bytes
 * @return 0 on success, or -errno
 *
 * The caller is responsible for ensuring that addr is a FLASH address.
 *
 * A successful write updates the FLASH cache.  It is _not_ necessary
 * to fmc_flash_cache_flush_all()
 */
int fmc_flash_program(void *paddr, const void *pvalue, size_t count);

/** @brief Erase FLASH sector
 * @param sectorn Sector index
 * @return 0 on success, or -errno
 *
 * The caller is responsible for understanding the mapping between
 * sector index and address.
 *
 * @note Erasing invalidates FLASH cache.  Caller must
 *       fmc_flash_cache_flush_all() after successful erase(s).
 */
int fmc_flash_erase_sector(unsigned sectorn);

/** @brief Synchronize FLASH read cache after erase operations
 * @return
 */
void fmc_flash_cache_flush_all(void);

int restore_flash(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FLASH_H */
