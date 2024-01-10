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

/* int sim_lass_init(unsigned short int port);
 *  Initialize LASS device listening on UDP port 'port'
 *  Returns 0 on success, -1 on failure.
 */
int sim_lass_init(unsigned short int port);

/* void sim_lass_service(void);
 *  Handle and respond to any LASS packets over UDP
 *  This function does not block (early exit when no UDP packet
 *  is pending) and should be called periodically in the main()
 *  loop or via equivalent scheduler.
 */
void sim_lass_service(void);

void print_mem_blocks(void);

#ifdef __cplusplus
}
#endif

#endif // __SIM_LASS_H
