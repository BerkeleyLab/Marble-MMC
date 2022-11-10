/*
 * File: max6639.h
 * Date: 221108
 * Desc: Register map and helper macros for interfacing with MAX6639 temperature
 *       and fan controller.
 */

#ifndef __MAX6639_H_
#define __MAX6639_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* =============================== Memory Map =============================== */
#define MAX6639_TEMP_CH1                                                (0x0)
#define MAX6639_TEMP_CH2                                                (0x1)
#define MAX6639_STATUS                                                  (0x2)
#define MAX6639_OUT_MASK                                                (0x3)
#define MAX6639_GLOBAL_CONFIG                                           (0x4)
#define MAX6639_TEMP_EXT_CH1                                            (0x5)
#define MAX6639_TEMP_EXT_CH2                                            (0x6)
#define MAX6639_NALERT_LIM_CH1                                          (0x8)
#define MAX6639_NALERT_LIM_CH2                                          (0x9)
#define MAX6639_NOT_LIM_CH1                                             (0xa)
#define MAX6639_NOT_LIM_CH2                                             (0xb)
#define MAX6639_NTHERM_LIM_CH1                                          (0xc)
#define MAX6639_NTHERM_LIM_CH2                                          (0xd)
#define MAX6639_FAN1_CONFIG1                                           (0x10)
#define MAX6639_FAN1_CONFIG2A                                          (0x11)
#define MAX6639_FAN1_CONFIG2B                                          (0x12)
#define MAX6639_FAN1_CONFIG3                                           (0x13)
#define MAX6639_FAN2_CONFIG1                                           (0x14)
#define MAX6639_FAN2_CONFIG2A                                          (0x15)
#define MAX6639_FAN2_CONFIG2B                                          (0x16)
#define MAX6639_FAN2_CONFIG3                                           (0x17)
#define MAX6639_FAN1_TACH_CNT                                          (0x20)
#define MAX6639_FAN2_TACH_CNT                                          (0x21)
#define MAX6639_FAN1_START_TACH_CNT                                    (0x22)
#define MAX6639_FAN2_MAX_TACH_CNT                                      (0x23)
#define MAX6639_FAN1_PULSE_PER_REV                                     (0x24)
#define MAX6639_FAN2_PULSE_PER_REV                                     (0x25)
#define MAX6639_FAN1_DUTY                                              (0x26)
#define MAX6639_FAN2_DUTY                                              (0x27)
#define MAX6639_MIN_FANSTART_TEMP_CH1                                  (0x28)
#define MAX6639_MIN_FANSTART_TEMP_CH2                                  (0x29)
#define MAX6639_DEV_ID                                                 (0x3d)
#define MAX6639_MFG_ID                                                 (0x3e)
#define MAX6639_DEV_REV                                                (0x3f)

/* ================================ X-Macros ================================ */
#define MAX6639_FOR_EACH_REGISTER() \
  X(MAX6639_TEMP_CH1, "Temperature Channel 1") \
  X(MAX6639_TEMP_CH2, "Temperature Channel 2") \
  X(MAX6639_STATUS, "Status Byte") \
  X(MAX6639_OUT_MASK, "Output Mask") \
  X(MAX6639_GLOBAL_CONFIG, "Global Configuration") \
  X(MAX6639_TEMP_EXT_CH1, "Channel 1 Extended Temperature") \
  X(MAX6639_TEMP_EXT_CH2, "Channel 2 Extended Temperature") \
  X(MAX6639_NALERT_LIM_CH1, "Channel 1 nALERT Limit") \
  X(MAX6639_NALERT_LIM_CH2, "Channel 2 nALERT Limit") \
  X(MAX6639_NOT_LIM_CH1, "Channel 1 nOT Limit") \
  X(MAX6639_NOT_LIM_CH2, "Channel 2 nOT Limit") \
  X(MAX6639_NTHERM_LIM_CH1, "Channel 1 nTHERM Limit") \
  X(MAX6639_NTHERM_LIM_CH2, "Channel 2 nTHERM Limit") \
  X(MAX6639_FAN1_CONFIG1, "Fan 1 Configuration 1") \
  X(MAX6639_FAN1_CONFIG2A, "Fan 1 Configuration 2a") \
  X(MAX6639_FAN1_CONFIG2B, "Fan 1 Configuration 2b") \
  X(MAX6639_FAN1_CONFIG3, "Fan 1 Configuration 3") \
  X(MAX6639_FAN2_CONFIG1, "Fan 2 Configuration 1") \
  X(MAX6639_FAN2_CONFIG2A, "Fan 2 Configuration 2a") \
  X(MAX6639_FAN2_CONFIG2B, "Fan 2 Configuration 2b") \
  X(MAX6639_FAN2_CONFIG3, "Fan 2 Configuration 3") \
  X(MAX6639_FAN1_TACH_CNT, "Fan 1 Tachometer Count") \
  X(MAX6639_FAN2_TACH_CNT, "Fan 2 Tachometer Count") \
  X(MAX6639_FAN1_START_TACH_CNT, "Fan 1 Start Tach Count") \
  X(MAX6639_FAN2_MAX_TACH_CNT, "Fan 2 Max Tach Count") \
  X(MAX6639_FAN1_PULSE_PER_REV, "Pulses Per Revolution Fan 1") \
  X(MAX6639_FAN2_PULSE_PER_REV, "Pulses Per Revolution Fan 2") \
  X(MAX6639_FAN1_DUTY, "Fan 1 Duty Cycle") \
  X(MAX6639_FAN2_DUTY, "Fan 2 Duty Cycle") \
  X(MAX6639_MIN_FANSTART_TEMP_CH1, "Channel 1 Minimum Fan Start Temperature") \
  X(MAX6639_MIN_FANSTART_TEMP_CH2, "Channel 2 Minimum Fan Start Temperature") \
  X(MAX6639_DEV_ID, "Device ID") \
  X(MAX6639_MFG_ID, "Manufacturer ID") \
  X(MAX6639_DEV_REV, "Device Revision")

/* ============================= Helper Macros ============================== */
#define MAX6639_GET_TEMP_DOUBLE(rTemp, rTempExt) \
   ((double)(((uint16_t)rTemp << 3) | (uint16_t)rTempExt >> 5)/8)

#ifdef __cplusplus
}
#endif

#endif /* __MAX6639_H_ */
