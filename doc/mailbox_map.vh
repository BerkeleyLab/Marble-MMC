`ifndef __MAILBOX_MAP_VH
`define __MAILBOX_MAP_VH

localparam MAILBOX_HASH = 32'h2d40ee95;

//  Page 0
localparam MAGIC_NUMBER_ADDR = 'h0;
localparam MAGIC_NUMBER_SIZE = 2;
localparam VERSION_MAJOR_ADDR = 'h2;
localparam VERSION_MAJOR_SIZE = 1;
localparam VERSION_MINOR_ADDR = 'h3;
localparam VERSION_MINOR_SIZE = 1;
//  Page 2
localparam FMC_MGT_CTL_ADDR = 'h20;
localparam FMC_MGT_CTL_SIZE = 1;
//  Page 3
localparam COUNT_ADDR = 'h30;
localparam COUNT_SIZE = 2;
localparam WD_STATE_ADDR = 'h32;
localparam WD_STATE_SIZE = 1;
localparam LM75_0_ADDR = 'h34;
localparam LM75_0_SIZE = 2;
localparam LM75_1_ADDR = 'h36;
localparam LM75_1_SIZE = 2;
localparam FMC_ST_ADDR = 'h38;
localparam FMC_ST_SIZE = 1;
localparam PWR_ST_ADDR = 'h39;
localparam PWR_ST_SIZE = 1;
localparam MGTMUX_ST_ADDR = 'h3a;
localparam MGTMUX_ST_SIZE = 1;
localparam GIT32_ADDR = 'h3c;
localparam GIT32_SIZE = 4;
//  Page 4
localparam MAX_T1_HI_ADDR = 'h40;
localparam MAX_T1_HI_SIZE = 1;
localparam MAX_T1_LO_ADDR = 'h41;
localparam MAX_T1_LO_SIZE = 1;
localparam MAX_T2_HI_ADDR = 'h42;
localparam MAX_T2_HI_SIZE = 1;
localparam MAX_T2_LO_ADDR = 'h43;
localparam MAX_T2_LO_SIZE = 1;
localparam MAX_F1_TACH_ADDR = 'h44;
localparam MAX_F1_TACH_SIZE = 1;
localparam MAX_F2_TACH_ADDR = 'h45;
localparam MAX_F2_TACH_SIZE = 1;
localparam MAX_F1_DUTY_ADDR = 'h46;
localparam MAX_F1_DUTY_SIZE = 1;
localparam MAX_F2_DUTY_ADDR = 'h47;
localparam MAX_F2_DUTY_SIZE = 1;
localparam PCB_REV_ADDR = 'h48;
localparam PCB_REV_SIZE = 1;
localparam COUNT_ADDR = 'h4a;
localparam COUNT_SIZE = 2;
localparam HASH_ADDR = 'h4c;
localparam HASH_SIZE = 4;
//  Page 5
localparam I2C_BUS_STATUS_ADDR = 'h50;
localparam I2C_BUS_STATUS_SIZE = 1;
//  Page 6
localparam FSYNTH_I2C_ADDR_ADDR = 'h60;
localparam FSYNTH_I2C_ADDR_SIZE = 1;
localparam FSYNTH_CONFIG_ADDR = 'h61;
localparam FSYNTH_CONFIG_SIZE = 1;
localparam FSYNTH_FREQ_ADDR = 'h62;
localparam FSYNTH_FREQ_SIZE = 4;
//  Page 7
localparam WD_NONCE_ADDR = 'h70;
localparam WD_NONCE_SIZE = 8;
//  Page 8
localparam WD_HASH_ADDR = 'h80;
localparam WD_HASH_SIZE = 8;
//  Page 9
localparam VOUT_1V0_ADDR = 'h90;
localparam VOUT_1V0_SIZE = 2;
localparam IOUT_1V0_ADDR = 'h92;
localparam IOUT_1V0_SIZE = 2;
localparam VOUT_1V8_ADDR = 'h94;
localparam VOUT_1V8_SIZE = 2;
localparam IOUT_1V8_ADDR = 'h96;
localparam IOUT_1V8_SIZE = 2;
localparam VOUT_2V5_ADDR = 'h98;
localparam VOUT_2V5_SIZE = 2;
localparam IOUT_2V5_ADDR = 'h9a;
localparam IOUT_2V5_SIZE = 2;
localparam VOUT_3V3_ADDR = 'h9c;
localparam VOUT_3V3_SIZE = 2;
localparam IOUT_3V3_ADDR = 'h9e;
localparam IOUT_3V3_SIZE = 2;
//  Page 10
localparam PMOD_LED_0_ADDR = 'ha0;
localparam PMOD_LED_0_SIZE = 1;
localparam PMOD_LED_1_ADDR = 'ha1;
localparam PMOD_LED_1_SIZE = 1;
localparam PMOD_LED_2_ADDR = 'ha2;
localparam PMOD_LED_2_SIZE = 1;
localparam PMOD_LED_3_ADDR = 'ha3;
localparam PMOD_LED_3_SIZE = 1;
localparam PMOD_LED_4_ADDR = 'ha4;
localparam PMOD_LED_4_SIZE = 1;
localparam PMOD_LED_5_ADDR = 'ha5;
localparam PMOD_LED_5_SIZE = 1;
localparam PMOD_LED_6_ADDR = 'ha6;
localparam PMOD_LED_6_SIZE = 1;
localparam PMOD_LED_7_ADDR = 'ha7;
localparam PMOD_LED_7_SIZE = 1;
`endif // __MAILBOX_MAP_VH
