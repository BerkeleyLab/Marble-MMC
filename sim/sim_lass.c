/*
 * File: sim_lass.c
 * Desc: Simulated memory implementing LASS (Lightweight Address Space Serialization)
 *       packet protocol (as does Packet Badger).
 */

#include <stdint.h>
#include <stdio.h>
#include "sim_lass.h"
#include "lass.h"
#include "udp_simple.h"
#include "common.h"

#define CHATTER
#include "dbg.h"
#include "config_rom.h"

#define LINE_WIDTH               (8)

static void send_response(uint8_t *_reply_buf, int _to_send);
/* ========================== Function Definitions ========================== */

/* int lass_init(unsigned short int port);
 *  Initialize LASS device listening on UDP port 'port'
 *  Returns 0 on success, -1 on failure.
 */
int sim_lass_init(unsigned short int port) {
  if (udp_init(port) < 0) {
    return -1;
  }

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
void sim_lass_service(void) {
  uint8_t pktdata[UDP_MAX_MSG_SIZE];
  int nsent = udp_receive(pktdata, UDP_MAX_MSG_SIZE);
  if (nsent == 0) {
    return;
  }
  printc("Received %d bytes\r\n", nsent);
  //print_lass((void *)pktdata, nsent);
  uint8_t *prsp;
  int rsp_size = 0;
  int do_reply = parse_lass((void *)pktdata, nsent, (volatile uint8_t **)&prsp, (volatile int *)&rsp_size);
  if (0) {
    for (int n = 0; n < nsent; n++) {
      printc("%02x ", pktdata[n]);
      if (((n % LINE_WIDTH) == LINE_WIDTH-1) || (n == nsent-1)) { printc("\r\n"); };
    }
  }
  if (do_reply) {
    send_response(prsp, rsp_size);
  }
  return;
}

static void send_response(uint8_t *_reply_buf, int _to_send) {
  int rval = udp_reply((const void *)_reply_buf, _to_send);
  if (rval == 0) {
    printc("sim_lass: send_response; rval is 0\r\n");
  }
  return;
}
