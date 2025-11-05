#ifndef __PHY_MDIO_H__
#define __PHY_MDIO_H__

void mdio_phy_print(int verbose);
void mdio_phy_reset(void);

// ================================ Registers ================================
// ======================== Page 0 =======================
#define MDIO_PHY_REG_CU_CTRL            (0)
#define MDIO_PHY_REG_CU_STAT            (1)
#define MDIO_PHY_REG_PHY_ID_1           (2)
#define MDIO_PHY_REG_PHY_ID_2           (3)
#define MDIO_PHY_REG_CU_AN_ADV          (4)
#define MDIO_PHY_REG_CU_LP_ABL          (5)
#define MDIO_PHY_REG_CU_AN_EXP          (6)
#define MDIO_PHY_REG_CU_NP_XMIT         (7)
#define MDIO_PHY_REG_CU_LP_NP           (8)
#define MDIO_PHY_REG_1000BASET_CTRL     (9)
#define MDIO_PHY_REG_1000BASET_STAT    (10)
#define MDIO_PHY_REG_EXT_STAT          (15)
#define MDIO_PHY_REG_CU_SP_CTRL_1      (16)
#define MDIO_PHY_REG_CU_SP_STAT_1      (17)

// ======================== Page 1 =======================
// ======================== Page 2 =======================

// ================================== Fields =================================
typedef struct __attribute__((packed)) {
  unsigned int regval: 16;
} regval_short;

typedef struct __attribute__((packed)) {
  unsigned int extended_capability: 1;
  unsigned int jabber_detect: 1;
  unsigned int cu_link_stat: 1;
  unsigned int an_ability: 1;
  unsigned int cu_remote_fault: 1;
  unsigned int cu_an_complete: 1;
  unsigned int mf_preamble_suppression: 1;
  unsigned int _reserved_7: 1;
  unsigned int extended_status: 1;
  unsigned int _cap_: 7;
} reg_CU_CTRL_t;

typedef union {
  reg_CU_CTRL_t bits;
  unsigned short val;
} reg_cu_ctrl;

typedef struct __attribute__((packed)) {
  unsigned int jabber: 1;
  unsigned int polarity: 1;
  unsigned int _reserved_2: 1;
  unsigned int global_link_status: 1;
  unsigned int cu_energy_detect_status: 1;
  unsigned int _reserved_5: 1;
  unsigned int mdi_crossover_status: 1;
  unsigned int _reserved_7: 1;
  unsigned int receive_pause_enabled: 1;
  unsigned int transmit_pause_enabled: 1;
  unsigned int copper_link: 1;
  unsigned int speed_duplex_resolved: 1;
  unsigned int page_received: 1;
  unsigned int duplex: 1;
  unsigned int speed: 2;
} reg_CU_SP_STAT_t;

typedef union {
  reg_CU_SP_STAT_t bits;
  unsigned short val;
} reg_cu_sp_stat;


#endif // __PHY_MDIO_H__
