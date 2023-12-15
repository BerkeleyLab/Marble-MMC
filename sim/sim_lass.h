/*
 * File: sim_lass.h
 * Desc: Simulated memory implementing LASS (Lightweight Address Space Serialization)
 *       packet protocol (as does Packet Badger).
 *       See bedrock/badger/mem_gate.md or:
 *       https://berkeleylab.github.io/Bedrock/_gen_md/badger/mem_gate_md.html#lightweight-address-space-serialization
 */

#ifndef __SIM_LASS_H
#define __SIM_LASS_H

#ifdef __cplusplus
extern "C" {
#endif

#define ACCESS_BYTES    (0)
#define ACCESS_HALFWORD (1)
#define ACCESS_WORD     (2)

/* int lass_init(unsigned short int port);
 *  Initialize LASS device listening on UDP port 'port'
 *  Returns 0 on success, -1 on failure.
 */
int lass_init(unsigned short int port);

/* void lass_service(void);
 *  Handle and respond to any LASS packets over UDP
 *  This function does not block (early exit when no UDP packet
 *  is pending) and should be called periodically in the main()
 *  loop or via equivalent scheduler.
 */
void lass_service(void);

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

#endif // __SIM_LASS_H
