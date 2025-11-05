/*
 * File: sim_lass.c
 * Desc: Simulated memory implementing LASS (Lightweight Address Space Serialization)
 *       packet protocol (as does Packet Badger).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sim_lass.h"
#include "udp_simple.h"
#include "common.h"

//#define CHATTER
#include "dbg.h"

/* ============================= Helper Macros ============================== */
#define LASS_CMD_WRITE        (0x00)
#define LASS_CMD_READ         (0x10)
#define LASS_CMD_BURST        (0x20)

// Maximum Ethernet packet size (overkill by UDP header & crc32)
#define ETH_MTU               (1500)

#define UDP_MAX_MSG_SIZE      ETH_MTU
#define LINE_WIDTH               (8)

#define MAX_MEM_BLOCKS          (10)

//#define DEBUG_MEM_BLOCK_TEST
//#define DEBUG_PRINT_REPLY
/* ================================ Typedefs ================================ */
typedef struct {
  uint8_t transaction_id[8];
  uint8_t cmd;
  uint8_t addr[3];
  uint8_t data[4];
} lass_pkt_t;

typedef struct {
  uint8_t transaction_id[8];
  uint8_t burst;
  uint8_t rep_count[3];
  uint8_t cmd;
  uint8_t addr[3];
  uint8_t data[4];
} lass_burst_pkt_t;

typedef struct {
  uint8_t cmd;
  uint8_t addr[3];
  uint8_t data[4];
} lass_beat_t;

typedef struct {
  uint8_t burst;
  uint8_t rep_count[3];
  uint8_t cmd;
  uint8_t addr[3];
  uint8_t data[4];
} lass_burst_beat_t;

typedef struct {
  uint32_t base;
  uint32_t size;
  uint8_t *mem;
  unsigned int asbyte;
} mem_block_t;

/* ============================ Static Variables ============================ */
#include "config_rom.h"
static uint8_t reply_buf[ETH_MTU];
static unsigned int to_send;
static mem_block_t mem_blocks[MAX_MEM_BLOCKS];
static mem_block_t *mem_blocks_sorted[MAX_MEM_BLOCKS];
static unsigned int mem_block_next;

/* =========================== Static Prototypes ============================ */
void print_lass(void *pkt_data, int size);
static const char *get_lass_cmd(uint8_t cmd);
static lass_beat_t *print_lass_beat(lass_beat_t *beat);
static void print_64bit_msb(void *pkt);
static unsigned int _get_rep_count(uint8_t *cnt);
static uint32_t _get_nbytes(uint8_t *data, int nbytes, int msb);
static void handle_beat(uint8_t cmd, uint32_t addr, uint32_t data);
static void queue_u32(uint32_t src, int msb);
static void init_response(uint32_t id0, uint32_t id1);
static void send_response(void);
static void ack_burst(unsigned int rep_count);
static void parse_lass(void *pkt_data, int size);
static lass_beat_t *parse_lass_beat(lass_beat_t *beat);
static void handle_write(uint32_t addr, uint32_t data);
static uint32_t handle_read(uint32_t addr);
static int vet_mem_block(uint32_t base, uint32_t size);
static void sort_mem_blocks(void);
static void print_mem_blocks(void);
#if 0
static void print_64bit_lsb(void *pkt);
#endif

/* ========================== Function Definitions ========================== */

/* int lass_init(unsigned short int port);
 *  Initialize LASS device listening on UDP port 'port'
 *  Returns 0 on success, -1 on failure.
 */
int lass_init(unsigned short int port) {
  if (udp_init(port) < 0) {
    return -1;
  }
  to_send = 0;
  mem_block_next = 0;

#ifdef DEBUG_MEM_BLOCK_TEST
  printf("Mem block test\r\n");
  // 0x100-0x200
  lass_mem_add(0x1000, 0x1000, NULL, ACCESS_BYTES);
  lass_mem_add(0x100, 0x100, NULL, ACCESS_BYTES);
  lass_mem_add(0x500, 0x200, NULL, ACCESS_BYTES);
  lass_mem_add(0x0, 0x80, NULL, ACCESS_BYTES);
  lass_mem_add(0x800, 0x20, NULL, ACCESS_BYTES);
  lass_mem_add(0x700, 0x100, NULL, ACCESS_BYTES);
#endif
  // Add config ROM
  lass_mem_add(0x800, CONFIG_ROM_SIZE, (void *)config_romx, ACCESS_HALFWORD);

  print_mem_blocks();
  return 0;
}

/* void lass_service(void);
 *  Handle and respond to any LASS packets over UDP
 *  This function does not block (early exit when no UDP packet
 *  is pending) and should be called periodically in the main()
 *  loop or via equivalent scheduler.
 */
void lass_service(void) {
  uint8_t pktdata[UDP_MAX_MSG_SIZE];
  int nsent = udp_receive(pktdata, UDP_MAX_MSG_SIZE);
  if (nsent == 0) {
    return;
  }
  printc("Received %d bytes\r\n", nsent);
  //print_lass((void *)pktdata, nsent);
  parse_lass((void *)pktdata, nsent);
  if (0) {
    for (int n = 0; n < nsent; n++) {
      printc("%02x ", pktdata[n]);
      if (((n % LINE_WIDTH) == LINE_WIDTH-1) || (n == nsent-1)) { printc("\r\n") };
    }
  }
  return;
}

int lass_mem_add(uint32_t base, uint32_t size, void *mem, unsigned int asbyte) {
  if (vet_mem_block(base, size)) {
    return -1;
  }
  mem_block_t *pmem;
  if (mem_block_next < MAX_MEM_BLOCKS) {
    pmem = &mem_blocks[mem_block_next++];
    pmem->base = base;
    pmem->size = size;
    pmem->mem = (uint8_t *)mem;
    pmem->asbyte = asbyte;
  }
  sort_mem_blocks();
  return 0;
}

static void print_mem_blocks(void) {
  mem_block_t *pmem;
  printf("BASE      END\r\n");
  for (unsigned int n = 0; n < mem_block_next; n++) {
    pmem = mem_blocks_sorted[n];
    printf("%08x  %08x\r\n", pmem->base, (pmem->base + pmem->size - 1));
  }
  printf("------------------\r\n");
  return;
}

/* static void sort_mem_blocks(void);
 *  An inefficient little sorting function which must be
 *  called after every addition to mem_blocks (it assumes
 *  mem_blocks_sorted is in fact sorted at the start of
 *  every function call).
 */
static void sort_mem_blocks(void) {
  if (mem_block_next < 2) {
    mem_blocks_sorted[mem_block_next-1] = &mem_blocks[mem_block_next-1];
    return;
  }
  mem_block_t *pmem = &mem_blocks[mem_block_next-1];
  uint32_t base;
  unsigned int n;
  unsigned int index = 0;
  // Find the index where pmem should be inserted
  for (n = 0; n < mem_block_next-1; n++) {
    base = mem_blocks_sorted[n]->base;
    if (pmem->base < base) {
      index = n;
      break;
    }
  }
  // Shift UP any entries above the index
  for (n = mem_block_next-1; n > index; n--) {
    mem_blocks_sorted[n] = mem_blocks_sorted[n-1];
  }
  // Insert pmem into its cozy little spot
  mem_blocks_sorted[index] = pmem;
  return;
}

static int vet_mem_block(uint32_t base, uint32_t size) {
  int overlap = 0;
  uint32_t _base, _size;
  for (unsigned int n = 0; n < mem_block_next; n++) {
    _base = mem_blocks[n].base;
    _size = mem_blocks[n].size;
    // Check lower boundary
    if ((base >= _base) && (base <= (_base + _size - 1))) {
      overlap = 1;
      break;
    }
    // Check upper boundary
    if (((base + size - 1) >= _base) && ((base + size - 1) <= (_base + _size - 1))) {
      overlap = 1;
      break;
    }
  }
  if (overlap) {
    printc("ERROR: Memory blocks overlap! Base+size 0x%x+0x%x overlaps with 0x%x+0x%x\r\n",
           base, size, _base, _size);
    return -1;
  }
  return 0;
}

static void parse_lass(void *pkt_data, int size) {
  lass_pkt_t *pkt = (lass_pkt_t *)pkt_data;
  uint32_t trans_id_0, trans_id_1;
  trans_id_0 = _get_nbytes(pkt->transaction_id, 4, 1);
  trans_id_1 = _get_nbytes((pkt->transaction_id+4), 4, 1);
  init_response(trans_id_0, trans_id_1);
  lass_beat_t *next = (lass_beat_t *)&(pkt->cmd);
  while (next < (lass_beat_t *)(((char *)pkt)+size-1)) {
    next = parse_lass_beat(next);
  }
#ifdef DEBUG_PRINT_REPLY
  printc("======================== DEBUG reply ========================\r\n");
  print_lass(reply_buf, to_send);
#endif
  send_response();
  return;
}
static void send_response(void) {
  int rval = udp_reply((const void *)reply_buf, to_send);
  if (rval > 0) {
    to_send = 0;
  } else {
    printc("sim_lass: send_response; rval = %d\r\n", rval);
  }
  return;
}

static void init_response(uint32_t id0, uint32_t id1) {
  printc("trans_id = %08x %08x\r\n", id0, id1);
  to_send = 0;
  queue_u32(id0, 1);
  queue_u32(id1, 1);
  return;
}

static void handle_beat(uint8_t cmd, uint32_t addr, uint32_t data) {
  switch (cmd) {
    case LASS_CMD_WRITE:
      handle_write(addr, data);
      break;
    case LASS_CMD_READ:
      data = handle_read(addr);
      break;
    default:  // BURST should not end up here
      break;
  }
  // Copy cmd & addr to response buffer
  addr += (cmd << 24);
  queue_u32(addr, 1);
  // Copy data to response buffer
  queue_u32(data, 1);
  return;
}

static void handle_write(uint32_t addr, uint32_t data) {
  printc("WRITE: addr = 0x%08x, data = 0x%08x\r\n", addr, data);
  mem_block_t *pmem;
  for (unsigned int n = 0; n < mem_block_next; n++) {
    pmem = mem_blocks_sorted[n];
    if ((addr >= pmem->base) && (addr <= pmem->base + pmem->size - 1)) {
      if (pmem->asbyte == ACCESS_BYTES) {
        // Store just 1 byte
        *(pmem->mem + (addr - pmem->base)) = (uint8_t)(data & 0xff);
      } else if (pmem->asbyte == ACCESS_HALFWORD) {
        // Store 2 bytes (half-word)
        // Let's try a simple memcpy. Might get the byte-order wrong
        memcpy((pmem->mem + (addr - pmem->base)), &data, 2);
      } else if (pmem->asbyte == ACCESS_WORD) {
        // Store 4 bytes (word)
        // Let's try a simple memcpy. Might get the byte-order wrong
        memcpy((pmem->mem + (addr - pmem->base)), &data, 4);
      }
      break;
    }
  }
  return;
}

static uint32_t handle_read(uint32_t addr) {
  mem_block_t *pmem;
  uint32_t data = 0;
  for (unsigned int n = 0; n < mem_block_next; n++) {
    pmem = mem_blocks_sorted[n];
    if ((addr >= pmem->base) && (addr <= (pmem->base + pmem->size - 1))) {
      if (pmem->asbyte == ACCESS_BYTES) {
        // Fetch just 1 byte
        data = (uint32_t)*(pmem->mem + (addr - pmem->base));
      } else if (pmem->asbyte == ACCESS_HALFWORD) {
        // Fetch 4 bytes (word)
        memcpy(&data, (pmem->mem + 2*(addr - pmem->base)), 2);
      } else if (pmem->asbyte == ACCESS_WORD) {
        // Fetch 4 bytes (word)
        memcpy(&data, (pmem->mem + 4*(addr - pmem->base)), 4);
      }
      break;
    }
  }
  printc("READ: addr = 0x%08x, data = 0x%08x\r\n", addr, data);
  return data;
}

static void ack_burst(unsigned int rep_count) {
  uint32_t burst_rep = (LASS_CMD_BURST << 24) | rep_count;
  queue_u32(burst_rep, 1);
  return;
}

static void queue_u32(uint32_t src, int msb) {
  if (msb) {
    for (int n = 0; n < 4; n++) {
      reply_buf[to_send++] = (src >> 8*(3-n)) & 0xff;
    }
  } else {
    for (int n = 0; n < 4; n++) {
      reply_buf[to_send++] = (src >> 8*n) & 0xff;
    }
  }
  return;
}

static lass_beat_t *parse_lass_beat(lass_beat_t *beat) {
  unsigned int rep_count;
  unsigned int addr;
  unsigned int data;
  if (beat->cmd == LASS_CMD_BURST) {
    lass_burst_beat_t *beat_b = (lass_burst_beat_t *)beat;
    rep_count = _get_rep_count(beat_b->rep_count);
    addr = _get_nbytes(beat_b->addr, 3, 1);
    ack_burst(rep_count);
    for (unsigned int n = 0; n < rep_count; n++) {
      data = _get_nbytes(&(beat_b->data[4*n]), 4, 1);
      handle_beat(beat_b->cmd, addr, data);
    }
  } else {
    rep_count = 0;
    addr = _get_nbytes(beat->addr, 3, 1);
    data = _get_nbytes(beat->data, 4, 1);
    handle_beat(beat->cmd, addr, data);
  }
  return (lass_beat_t *)(((char *)beat->data) + 4*(rep_count+1));
}

static uint32_t _get_nbytes(uint8_t *data, int nbytes, int msb) {
  uint32_t acc = 0;
  nbytes = MIN(nbytes, 4);
  if (msb) {
    for (int n = 0; n < nbytes; n++) {
      acc += data[nbytes-1-n] << 8*n;
    }
  } else {
    for (int n = 0; n < nbytes; n++) {
      acc += data[n] << 8*n;
    }
  }
  return acc;
}

static unsigned int _get_rep_count(uint8_t *cnt) {
  return (unsigned int)((*cnt << 16) | (*(cnt+1) << 8) | *(cnt+2));
}

void print_lass(void *pkt_data, int size) {
  lass_pkt_t *pkt = (lass_pkt_t *)pkt_data;
  printf("==== TRANSACTION_ID : 0x");
  print_64bit_msb(pkt->transaction_id);
  printf(" ====\r\n");
  lass_beat_t *next = (lass_beat_t *)&(pkt->cmd);
  while (next < (lass_beat_t *)(((char *)pkt)+size-1)) {
    next = print_lass_beat(next);
  }
  printf("==== END ====\r\n");
  return;
}

static const char *get_lass_cmd(uint8_t cmd) {
  switch (cmd) {
    case LASS_CMD_WRITE:
      return "LASS_CMD_WRITE";
      break;
    case LASS_CMD_READ:
      return "LASS_CMD_READ";
      break;
    case LASS_CMD_BURST:
      return "LASS_CMD_BURST";
      break;
  }
  return "LASS_CMD_UNKNOWN";
}

static void print_nbytes(void *pkt, int nbytes, int msb) {
  uint8_t *pbyte = (uint8_t *)pkt;
  if (msb) {
    for (int n = 0; n < nbytes; n++) {
      printf("%02x", pbyte[n]);
    }
  } else {
    for (int n = 0; n < nbytes; n++) {
      printf("%02x", pbyte[nbytes-n]);
    }
  }
  return;
}

static void print_64bit_msb(void *pkt) {
  print_nbytes(pkt, 8, 1);
  return;
}

#if 0
static void print_64bit_lsb(void *pkt) {
  print_nbytes(pkt, 8, 0);
  return;
}
#endif

static lass_beat_t *print_lass_beat(lass_beat_t *beat) {
  unsigned int rep_count;
  printf("%s:", get_lass_cmd(beat->cmd));
  if (beat->cmd == LASS_CMD_BURST) {
    lass_burst_beat_t *beat_b = (lass_burst_beat_t *)beat;
    rep_count = _get_rep_count(beat_b->rep_count);
    printf(" rep_count = 0x");
    print_nbytes(beat_b->rep_count, 3, 1);
    printf("\r\n");
    printf("  %s: addr = 0x", get_lass_cmd(beat_b->cmd));
    print_nbytes(beat_b->addr, 3, 1);
    printf("\r\n");
    for (unsigned int n = 0; n < rep_count; n++) {
      printf("    data%u = 0x", n);
      print_nbytes((beat_b->data) + 4*n, 4, 1);
      printf("\r\n");
    }
  } else {
    rep_count = 0;
    printf(" addr = 0x");
    print_nbytes(beat->addr, 3, 1);
    printf("\r\n");
    printf("  data = 0x");
    print_nbytes(beat->data, 4, 1);
    printf("\r\n");
  }
  return (lass_beat_t *)(((char *)beat->data) + 4*(rep_count+1));
}
