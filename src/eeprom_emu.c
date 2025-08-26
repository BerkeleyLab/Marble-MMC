/* Emulate EEPROM with FLASH
 *
 * cf. ST App. note AN3390
 * "EEPROM emulation in STM32F2xx microcontrollers"
 *
 * Uses two sectors.  Use "tag" instead of "address".
 * Each tag selects 6 bytes.  With 1 byte CRC, this
 * gives an 8 byte block.
 *
 * The first block in search sector is a header indicating
 * the state of that sector: erased, valid, moving.
 *
 * Tags 0x00 and 0xff have special meaning, and can not
 * be used by application code.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "flash.h"
#include "common.h"
#include "marble_api.h"

//#define DEBUG_PRINT
#include "dbg.h"

#include "eeprom_emu.h"

#ifndef SIMULATION
  #ifdef MARBLE_V2
    // assigned in linker script
    // symbol value (address) is really a size in bytes
    extern const char eeprom_size;
    #define EEPROM_COUNT ((size_t)&eeprom_size/sizeof(ee_frame))
  #else // !MARBLE_V2
    #error "This emulation layer should only be used for Marble (STM32-based)"
  #endif // ifdef MARBLE_V2
#else
  #include "sim_api.h"
#endif  /* SIMULATION */

typedef enum {
    ee_erased = 0xff,
    ee_valid = 0x55,
    ee_moving = 0x00,
    ee_invalid,
} ee_state_t;

// number of bits to encode a state
static const size_t ee_state_bits = 2u;

// frame[0] is the page header.
// frame[1] .. frame[EEPROM_COUNT-1] are possibly valid frames
extern
ee_frame eeprom0_base;    // Defined in linker file or sim/sim_flash.c (ifdef SIMULATION)
extern
ee_frame eeprom1_base;    // Defined in linker file or sim/sim_flash.c (ifdef SIMULATION)

// corresponding erase sectors
static const uint8_t eeprom0_sector = 1;
static const uint8_t eeprom1_sector = 2;

static
ee_frame* ee_active;

static
uint8_t count_bits_set(uint32_t v)
{
    return __builtin_popcount(v);
}

static
int bit_check(uint8_t cnt)
{
    size_t nbits = 8u*sizeof(ee_frame)/ee_state_bits;

    if(cnt <= nbits/4u)
        return 0;
    else if(cnt >= (nbits/4u)*3u)
        return 1;
    else
        return -1;
}

static
ee_state_t ee_page_state(void *raw)
{
    const uint8_t *base = (const uint8_t *)raw;
    uint8_t nbit0 = 0u;
    uint8_t nbit1 = 0u;

    for(size_t i=0u; i<sizeof(ee_frame); i++) {
        uint8_t hdr = base[i];
        nbit0 += count_bits_set(0x55 & (hdr >> 0u));
        nbit1 += count_bits_set(0x55 & (hdr >> 1u));
    }

    // Apply threshold test to decide if we believe this header.
    int bit0 = bit_check(nbit0);
    int bit1 = bit_check(nbit1);
    if(bit0 < 0 || bit1 < 0)
        return ee_invalid;

    switch((bit1<<1u) | bit0) {
    case 0x0: return ee_moving;
    case 0x1: return ee_valid;
    case 0x3: return ee_erased;
    default: return ee_invalid;
    }
}

static
int ee_page_set_state(void *raw, ee_state_t state)
{
    uint8_t hdr_valid[sizeof(ee_frame)];
    for(size_t i=0u; i<sizeof(hdr_valid); i++) {
        hdr_valid[i] = (uint8_t)state;
    }
    return fmc_flash_program(raw, hdr_valid, sizeof(hdr_valid));
}

static
uint8_t ee_frame_crc(const ee_frame* frame)
{
    uint8_t crc = frame->tag;

    for(size_t i=0; i<sizeof(frame->val); i++)
        crc ^= frame->val[i];

    return crc;
}

static
int ee_frame_check(const ee_frame* frame)
{
    return frame->tag!=0x00 && frame->tag!=0xff && frame->crc == ee_frame_crc(frame);
}

// search bank for last valid tag.
static
const ee_frame* ee_find(const ee_frame* bank, ee_tags_t tag)
{
    const ee_frame* found = NULL;
    if(tag==0 || tag==0xff) {
        return found;
    }
    size_t i;
    for(i=1u; i<EEPROM_COUNT; i++) {
        if(bank[i].tag==0xff) {
            printd("Empty\r\n");
            break;
        }
        if(bank[i].tag!=tag || !ee_frame_check(&bank[i])) {
            continue;
        }
        found = &bank[i];
    }
    return found;
}

static
int ee_write(ee_frame* bank, ee_tags_t tag, const ee_val_t val)
{
    const ee_frame* prev = NULL;

    size_t n;
    for(n=1u; n<EEPROM_COUNT; n++) {
        const ee_frame* f = &bank[n];

        if(f->tag==0xff)
            break;

        if(f->tag==tag && ee_frame_check(f))
            prev = f;
    }

    if(prev && memcmp(prev->val, val, sizeof(prev->val))==0) {
        return 0; // ignore write of duplicate
    }

    if(n==EEPROM_COUNT) {
        return -ENOSPC; // full!
    }

    // we have space, and 'n' points to an unused block
    ee_frame f;
    f.tag = tag;
    memcpy(f.val, val, sizeof(f.val));
    f.crc = ee_frame_crc(&f);

    return fmc_flash_program((ee_frame*)&bank[n], &f, sizeof(f));
}

static
int ee_migrate(ee_frame* __restrict__ dst, const ee_frame* __restrict__ src)
{
    int ret = 0;

    // Scan backwards in source frame to first last valid of each tag.
    for(size_t srcn = EEPROM_COUNT-1u; srcn; srcn--) {
        const ee_frame* sf = &src[srcn];

        if(!ee_frame_check(sf))
            continue;

        // does tag already exist in destination?
        const ee_frame* df = (const ee_frame*)ee_find(dst, sf->tag);
        if(!df) { // not found, migrate tag
            ret = ee_write(dst, sf->tag, sf->val);

        } else { // found, assume tag already migrated
        }
    }

    if(ret) {
        printf("ERROR: migrating %p <- %p : %d\n", (void *)dst, (const void *)src, ret);
    }
    return ret;
}

int eeprom_system_init(void)
{
    if (restore_flash() < 0) {
      fmc_flash_erase_sector(1);
      fmc_flash_erase_sector(2);
    }
    fmc_flash_init();
    ee_active = NULL;

    int ret = 0;
    ee_state_t e0 = ee_page_state(&eeprom0_base);
    ee_state_t e1 = ee_page_state(&eeprom1_base);

    if((e0==ee_valid) ^ (e1==ee_valid)) { // one bank valid
        printd("obv\r\n");
        if(e0==ee_valid) {
            printd("e0v\r\n");
            if(e1==ee_moving) {// need to finish migrating bank1
                ret = ee_migrate(&eeprom0_base, &eeprom1_base);
                if(!ret)
                    ret = fmc_flash_erase_sector(eeprom1_sector);
                fmc_flash_cache_flush_all();
            }
            if(!ret)
                ee_active = &eeprom0_base;

        } else { // e1==ee_valid
            printd("e1v\r\n");
            if(e0==ee_moving) {// need to finish migrating bank0
                ret = ee_migrate(&eeprom1_base, &eeprom0_base);
                if(!ret)
                    ret = fmc_flash_erase_sector(eeprom0_sector);
                fmc_flash_cache_flush_all();
            }
            if(!ret)
                ee_active = &eeprom1_base;
        }

    } else {
        printd("nov\r\n");
        if(!(e0==ee_erased && e1==ee_erased)) {
            printf("ERROR: EEFLASH invalid!  Reformatting...\n");
            ret = fmc_flash_erase_sector(eeprom0_sector);
            ret |= fmc_flash_erase_sector(eeprom1_sector);
            fmc_flash_cache_flush_all();
        }
        ret = ee_page_set_state(&eeprom0_base, ee_valid);
        if(!ret) {
            printd("e0 active\r\n");
            ee_active = &eeprom0_base;
            e0 = ee_page_state(&eeprom0_base);
            printd("e0 page_state = %u\r\n", e0);
        } else {
            printd("no active\r\n");
        }
    }
    printd("eeprom_init (%d)\r\n", ret);
    return ret;
}

int fmc_ee_reset(void)
{
    int ret = 0;

    ee_active = NULL;

    ret |= fmc_flash_erase_sector(eeprom0_sector);
    ret |= fmc_flash_erase_sector(eeprom1_sector);
    fmc_flash_cache_flush_all();
    ret |= eeprom_init();

    return ret;
}

int fmc_ee_read(ee_tags_t tag, ee_val_t val)
{
    const ee_frame* bank = ee_active;
    if(!bank) {
        return -EIO;
    }
    const ee_frame* f = ee_find(bank, tag);
    printd("ee_find(eeprom0_base, %u) = %p\r\n", tag, (void *)f);
    if(f) {
        memcpy(val, f->val, sizeof(f->val)/sizeof(uint8_t));
        return 0;
    } else {
        return -ENOENT;
    }
}

/* Helps avoid lots of unnecessary write cycles doing in migration in the unlikely event
 * that the active bank contains only unique tags, or bad blocks.
 */
static
int ee_is_full(const ee_frame* bank)
{
    uint32_t found[1u+sizeof(ee_tag_t)*8u/32u];

    memset(found, 0, sizeof(found)/sizeof(uint8_t));

    for(size_t n=1u; n<EEPROM_COUNT; n++) {
        const ee_frame* f = &bank[n];
        const size_t word = f->tag/8u;
        const uint8_t mask = 1u<<(f->tag%8u);

        if(f->tag==0xff) {
            return 0; // found an unused tag

        } else if(!ee_frame_check(f)) { // presume bad sector, treat as used

        } else if(found[word] & mask) {
            // found a valid duplicate, which will be free after migration
            return 0;

        } else {
            found[word] |= mask;
        }
    }

    return 1;
}

int fmc_ee_write(ee_tags_t tag, const ee_val_t val)
{
    if(tag==0 || tag==0xff) {
        return -EINVAL;
    }
    ee_frame* active = ee_active;

    if (active == &eeprom0_base) {
      printd("e0 active\r\n");
    } else if (active == &eeprom1_base) {
      printd("e1 active\r\n");
    } else {
      printd("active is %p\r\n", (void *)active);
    }

    if(!active) {
        return -EIO;
    }
    int ret = ee_write(active, tag, val);
    printd("ee_write ret = %s\r\n", decode_errno(ret));

    if(ret==-ENOSPC && !ee_is_full(active)) { // Try migration
        ee_frame* alt;
        unsigned activen;
        if(active==&eeprom0_base) {
            alt = &eeprom1_base;
            activen = eeprom0_sector;
        } else if(active==&eeprom1_base) {
            alt = &eeprom0_base;
            activen = eeprom1_sector;
        } else {
            return -EINVAL;
        }
        if(ee_page_state(alt)!=ee_erased) {
            return -EINVAL;
        }
        ret = ee_page_set_state((ee_frame*)active, ee_moving);
        if(!ret) {
            ret = ee_migrate(alt, active);
        }
        if(!ret) {
            ret = ee_page_set_state(alt, ee_valid);
        }
        if(!ret) {
            ret = fmc_flash_erase_sector(activen); // sets active = ee_erased
        }
        fmc_flash_cache_flush_all();

        if(!ret) {
            ee_active = alt;

            // retry on alternate bank
            ret = ee_write(alt, tag, val);
            // may still fail with -ENOSPC if really full.
        }
    }

    return ret;
}

// Unused signature for compatibility with cached EEPROM alternative
void eeprom_update(void) {
  return;
}
