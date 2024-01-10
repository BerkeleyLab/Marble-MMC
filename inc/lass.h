/*
 * File: lass.h
 * Desc: Memory implementing LASS (Lightweight Address Space Serialization)
 *       packet protocol (as does Packet Badger).
 *       See bedrock/badger/mem_gate.md or:
 *       https://berkeleylab.github.io/Bedrock/_gen_md/badger/mem_gate_md.html#lightweight-address-space-serialization
 */

#ifndef __LASS_H
#define __LASS_H

#ifdef __cplusplus
extern "C" {
#endif

#define ACCESS_BYTES    (0)
#define ACCESS_HALFWORD (1)
#define ACCESS_WORD     (2)

// Maximum ethernet packet size (overkill by UDP header & crc32)
#define ETH_MTU               (1500)

#define UDP_MAX_MSG_SIZE      ETH_MTU

/* int lass_init(void);
 *  Initialize LASS device listening on UDP port 'port'
 *  Returns 0 on success, -1 on failure.
 */
int lass_init(void);

/* int parse_lass(void *pkt_data, int size, volatile uint8_t **response, volatile int *response_size);
 *  Parse a 'pkt_data' according to LASS protocol.
 *  Returns 1 if it's a LASS packet and we should reply, else 0.
 *  If retval > 0, *response is a pointer to the response buffer and response_size
 *  is the length of the response.
 */
int parse_lass(void *pkt_data, int size, volatile uint8_t **response, volatile int *response_size);

/* int lass_mem_add(uint32_t base, uint32_t size, void *mem, unsigned int asbyte);
 *  Add a memory region for access via LASS device simulator.
 *  The memory region will extend from address 'base' to 'base+size-1' in the
 *  fictitious address space of the LASS protocol.  The pointer 'mem' should
 *  point to the base of the memory region.
 *
 *  WARNING! This is a great candidate for a seg fault if you don't ensure that
 *  'mem' points to a valid region of memory that is at least 'size' bytes long.
 *
 *  NOTE! All memory regions are R/W.  This could be trivially extended to allow
 *  read-only, but it would probably require a change to this function signature.
 *
 *  @params:
 *    base:   The LASS-protocol base address
 *    size:   The size (in bytes or words, see 'asbyte' below) of the memory region
 *    mem:    A pointer to the base of the memory region to be accessed.
 *    asbyte: If 0, memory is assumed to be word-addressable (and should be 4*size
 *            bytes in size).  Otherwise, memory is assumed to be byte-addressable
 *            and should be 'size' bytes in size.
 */
int lass_mem_add(uint32_t base, uint32_t size, void *mem, unsigned int asbyte);


#ifdef __cplusplus
}
#endif

#endif // __LASS_H
