/* I need a place for exported function signatures that are
 * only in the sim platform
 */

#ifndef __SIM_API_H
#define __SIM_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_FLASH_FILENAME            "flash.bin"
#define FLASH_SECTOR_SIZE             (256)
#define EEPROM_COUNT                  ((size_t)FLASH_SECTOR_SIZE/sizeof(ee_frame))

int sim_spi_init(void);
void init_sim_ltm4673(void);

// LPC EEPROM driver emulation
#define EEPROM_PAGE_SIZE (64)
void Sim_EEPROM_Init(int size);
int Sim_EEPROM_Read(volatile uint8_t *dest, int size_bytes);
int Sim_EEPROM_Erase(int offset, int size);
int Sim_EEPROM_Write(const uint8_t *src, int offset, int size);

#ifdef __cplusplus
}
#endif

#endif // __SIM_API_H
