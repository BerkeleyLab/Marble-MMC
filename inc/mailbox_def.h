/*
 * File: mailbox_def.h
 * Desc: Header file for inc/mailbox_def. Auto-generated by scripts/mkmbox.py.
 */

#ifndef __MAILBOX_DEF_H_
#define __MAILBOX_DEF_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* ============= DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY ============== */
/* ============== Define mailbox structure in scripts/mbox.def ============== */

typedef enum {
  MB2_FMC_MGT_CTL,
  MB2_SIZE // 1
} PAGE2_ENUM;

typedef enum {
  MB3_COUNT_1,
  MB3_COUNT_0,
  MB3_PAD2,
  MB3_PAD3,
  MB3_LM75_0_1,
  MB3_LM75_0_0,
  MB3_LM75_1_1,
  MB3_LM75_1_0,
  MB3_FMC_ST,
  MB3_PWR_ST,
  MB3_MGTMUX_ST,
  MB3_PAD11,
  MB3_GIT32_3,
  MB3_GIT32_2,
  MB3_GIT32_1,
  MB3_GIT32_0,
  MB3_SIZE // 16
} PAGE3_ENUM;

typedef enum {
  MB4_MAX_T1_HI,
  MB4_MAX_T1_LO,
  MB4_MAX_T2_HI,
  MB4_MAX_T2_LO,
  MB4_MAX_F1_TACH,
  MB4_MAX_F2_TACH,
  MB4_MAX_F1_DUTY,
  MB4_MAX_F2_DUTY,
  MB4_PCB_REV,
  MB4_PAD11,
  MB4_COUNT_1,
  MB4_COUNT_0,
  MB4_HASH_3,
  MB4_HASH_2,
  MB4_HASH_1,
  MB4_HASH_0,
  MB4_SIZE // 16
} PAGE4_ENUM;

typedef enum {
  MB5_I2C_BUS_STATUS,
  MB5_SIZE // 1
} PAGE5_ENUM;

typedef enum {
  MB6_FSYNTH_I2C_ADDR,
  MB6_FSYNTH_CONFIG,
  MB6_FSYNTH_FREQ_3,
  MB6_FSYNTH_FREQ_2,
  MB6_FSYNTH_FREQ_1,
  MB6_FSYNTH_FREQ_0,
  MB6_SIZE // 6
} PAGE6_ENUM;


uint32_t mailbox_get_hash(void);
void mailbox_update_input(void);
void mailbox_update_output(void);
void mailbox_read_print_all(void);


#ifdef __cplusplus
}
#endif

#endif /* __MAILBOX_DEF_H_ */
