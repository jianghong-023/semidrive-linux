/*
 * ak7738.h  --  audio driver for ak7738
 *
 * Copyright (C) 2015 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      15/05/14	    1.1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK7738_H
#define _AK7738_H

#include "ak7738ctl.h"

#define AK7738_I2C_IF
#define AK7738_IO_CONTROL

#define NUM_SYNCDOMAIN  7

/* AK7738_C1_CLOCK_SETTING2 (0xC1)  Fields */
#define AK7738_AIF_BICK32		(2 << 4)
#define AK7738_AIF_BICK48		(1 << 4)
#define AK7738_AIF_BICK64		(0 << 4)
#define AK7738_AIF_TDM			(3 << 4)	//TDM256 bit is set "1" at initialization.

/* TDMMODE Input Source */
#define AK7738_TDM_DSP				(0 << 2)
#define AK7738_TDM_DSP_AD1			(1 << 2)
#define AK7738_TDM_DSP_AD1_AD2		(2 << 2)

/* User Setting */
//#define DIGITAL_MIC
//#define CLOCK_MODE_BICK
//#define CLOCK_MODE_18_432
#define AK7738_AUDIO_IF_MODE		AK7738_AIF_BICK64	//32fs, 48fs, 64fs, 256fs(TDM)
#define AK7738_TDM_INPUT_SOURCE		AK7738_TDM_DSP		//Effective only in TDM mode
#define AK7738_BCKP_BIT				(0 << 6)	//BICK Edge Setting
#define AK7738_SOCFG_BIT  			(0 << 4)	//SO pin Hi-z Setting
#define AK7738_DMCLKP1_BIT			(0 << 6)	//DigitalMIC 1 Channnel Setting
#define AK7738_DMCLKP2_BIT			(0 << 3)	//DigitalMIC 1 Channnel Setting
/* User Setting */

#define AK7738_00_STSCLOCK_SETTING1        0x00
#define AK7738_01_STSCLOCK_SETTING2        0x01
#define AK7738_02_MICBIAS_PLOWER           0x02
#define AK7738_03_SYNCDOMAIN1_SETTING1     0x03
#define AK7738_04_SYNCDOMAIN1_SETTING2     0x04
#define AK7738_05_SYNCDOMAIN2_SETTING1     0x05
#define AK7738_06_SYNCDOMAIN2_SETTING2     0x06
#define AK7738_07_SYNCDOMAIN3_SETTING1     0x07
#define AK7738_08_SYNCDOMAIN3_SETTING2     0x08
#define AK7738_09_SYNCDOMAIN4_SETTING1     0x09
#define AK7738_0A_SYNCDOMAIN4_SETTING2     0x0A
#define AK7738_0B_SYNCDOMAIN5_SETTING1     0x0B
#define AK7738_0C_SYNCDOMAIN5_SETTING2     0x0C
#define AK7738_0D_SYNCDOMAIN6_SETTING1     0x0D
#define AK7738_0E_SYNCDOMAIN6_SETTING2     0x0E
#define AK7738_0E_SYNCDOMAIN7_SETTING1     0x0F
#define AK7738_10_SYNCDOMAIN7_SETTING2     0x10
#define AK7738_11_BUS_DSP_CLOCK_SETTING    0x11
#define AK7738_12_BUS_CLOCK_SETTING2       0x12
#define AK7738_13_CLOKO_OUTPUT_SETTING     0x13
#define AK7738_14_RESERVED                 0x14
#define AK7738_15_SYNCDOMAIN_SELECT1       0x15
#define AK7738_16_SYNCDOMAIN_SELECT2       0x16
#define AK7738_17_SYNCDOMAIN_SELECT3       0x17
#define AK7738_18_SYNCDOMAIN_SELECT4       0x18
#define AK7738_19_SYNCDOMAIN_SELECT5       0x19
#define AK7738_1A_SYNCDOMAIN_SELECT6       0x1A
#define AK7738_1B_SYNCDOMAIN_SELECT7       0x1B
#define AK7738_1C_SYNCDOMAIN_SELECT8       0x1C
#define AK7738_1D_SYNCDOMAIN_SELECT9       0x1D
#define AK7738_1E_SYNCDOMAIN_SELECT10      0x1E
#define AK7738_1F_SYNCDOMAIN_SELECT11      0x1F
#define AK7738_20_SYNCDOMAIN_SELECT12      0x20
#define AK7738_21_SYNCDOMAIN_SELECT13      0x21
#define AK7738_22_RESERVED                 0x22
#define AK7738_23_SDOUT1_DATA_SELECT       0x23
#define AK7738_24_SDOUT2_DATA_SELECT       0x24
#define AK7738_25_SDOUT3_DATA_SELECT       0x25
#define AK7738_26_SDOUT4_DATA_SELECT       0x26
#define AK7738_27_SDOUT5_DATA_SELECT       0x27
#define AK7738_28_SDOUT6_DATA_SELECT       0x28
#define AK7738_29_DAC1_DATA_SELECT         0x29
#define AK7738_2A_DAC2_DATA_SELECT         0x2A
#define AK7738_2B_DSP1_IN1_DATA_SELECT     0x2B
#define AK7738_2C_DSP1_IN2_DATA_SELECT     0x2C
#define AK7738_2D_DSP1_IN3_DATA_SELECT     0x2D
#define AK7738_2E_DSP1_IN4_DATA_SELECT     0x2E
#define AK7738_2F_DSP1_IN5_DATA_SELECT     0x2F
#define AK7738_30_DSP1_IN6_DATA_SELECT     0x30
#define AK7738_31_DSP2_IN1_DATA_SELECT     0x31
#define AK7738_32_DSP2_IN2_DATA_SELECT     0x32
#define AK7738_33_DSP2_IN3_DATA_SELECT     0x33
#define AK7738_34_DSP2_IN4_DATA_SELECT     0x34
#define AK7738_35_DSP2_IN5_DATA_SELECT     0x35
#define AK7738_36_DSP2_IN6_DATA_SELECT     0x36
#define AK7738_37_SRC1_DATA_SELECT         0x37
#define AK7738_38_SRC2_DATA_SELECT         0x38
#define AK7738_39_SRC3_DATA_SELECT         0x39
#define AK7738_3A_SRC4_DATA_SELECT         0x3A
#define AK7738_3B_FSCONV1_DATA_SELECT      0x3B
#define AK7738_3C_FSCONV2_DATA_SELECT      0x3C
#define AK7738_3D_MIXERA_CH1_DATA_SELECT   0x3D
#define AK7738_3E_MIXERA_CH2_DATA_SELECT   0x3E
#define AK7738_3F_MIXERB_CH1_DATA_SELECT   0x3F
#define AK7738_40_MIXERB_CH2_DATA_SELECT   0x40
#define AK7738_41_DIT_DATA_SELECT          0x41
#define AK7738_42_RESERVED                 0x42
#define AK7738_43_RESERVED                 0x43
#define AK7738_44_CLOCKFORMAT_SETTING1     0x44
#define AK7738_45_CLOCKFORMAT_SETTING2     0x45
#define AK7738_46_CLOCKFORMAT_SETTING3     0x46
#define AK7738_47_RESERVED                 0x47
#define AK7738_48_SDIN1_FORMAT             0x48
#define AK7738_49_SDIN2_FORMAT             0x49
#define AK7738_4A_SDIN3_FORMAT             0x4A
#define AK7738_4B_SDIN4_FORMAT             0x4B
#define AK7738_4C_SDIN5_FORMAT             0x4C
#define AK7738_4D_SDIN6_FORMAT             0x4D
#define AK7738_4E_SDOUT1_FORMAT            0x4E
#define AK7738_4F_SDOUT2_FORMAT            0x4F
#define AK7738_50_SDOUT3_FORMAT            0x50
#define AK7738_51_SDOUT4_FORMAT            0x51
#define AK7738_52_SDOUT5_FORMAT            0x52
#define AK7738_53_SDOUT6_FORMAT            0x53
#define AK7738_54_SDOUT_MODE_SETTING       0x54
#define AK7738_55_TDM_MODE_SETTING         0x55
#define AK7738_56_RESERVED                 0x56
#define AK7738_57_OUTPUT_PORT_SELECT       0x57
#define AK7738_58_OUTPUT_PORT_ENABLE       0x58
#define AK7738_59_INPUT_PORT_SELECT        0x59
#define AK7738_5A_RESERVED                 0x5A
#define AK7738_5B_MIXER_A_SETTING          0x5B
#define AK7738_5C_MIXER_B_SETTING          0x5C
#define AK7738_5D_MICAMP_GAIN              0x5D
#define AK7738_5E_MICAMP_GAIN_CONTROL      0x5E
#define AK7738_5F_ADC1_LCH_VOLUME          0x5F
#define AK7738_60_ADC1_RCH_VOLUME          0x60
#define AK7738_61_ADC2_LCH_VOLUME          0x61
#define AK7738_62_ADC2_RCH_VOLUME          0x62
#define AK7738_63_ADCM_VOLUME              0x63

#define AK7738_64_RESERVED                 0x64
#define AK7738_65_RESERVED                 0x65
#define AK7738_66_AIN_FILTER               0x66
#define AK7738_67_ADC_MUTE_HPF             0x67
#define AK7738_68_DAC1_LCH_VOLUME          0x68
#define AK7738_69_DAC1_RCH_VOLUME          0x69
#define AK7738_6A_DAC2_LCH_VOLUME          0x6A
#define AK7738_6B_DAC2_RCH_VOLUME          0x6B

#define AK7738_6C_RESERVED                 0x6C
#define AK7738_6D_RESERVED                 0x6D
#define AK7738_6E_DAC_MUTE_FILTER          0x6E
#define AK7738_6F_SRC_CLOCK_SETTING        0x6F
#define AK7738_70_SRC_MUTE_SETTING         0x70
#define AK7738_71_FSCONV_MUTE_SETTING      0x71
#define AK7738_72_RESERVED                 0x72
#define AK7738_73_DSP_MEMORY_ASSIGNMENT1   0x73
#define AK7738_74_DSP_MEMORY_ASSIGNMENT2   0x74
#define AK7738_75_DSP12_DRAM_SETTING       0x75
#define AK7738_76_DSP1_DLRAM_SETTING       0x76
#define AK7738_77_DSP2_DLRAM_SETTING       0x77
#define AK7738_78_FFT_DLP0_SETTING         0x78
#define AK7738_79_RESERVED                 0x79
#define AK7738_7A_JX_SETTING               0x7A
#define AK7738_7B_STO_FLAG_SETTING1        0x7B
#define AK7738_7C_STO_FLAG_SETTING2        0x7C
#define AK7738_7D_RESERVED                 0x7D
#define AK7738_7E_DIT_STATUS_BIT1          0x7E
#define AK7738_7F_DIT_STATUS_BIT2          0x7F
#define AK7738_80_DIT_STATUS_BIT3          0x80
#define AK7738_81_DIT_STATUS_BIT4          0x81
#define AK7738_82_RESERVED                 0x82
#define AK7738_83_POWER_MANAGEMENT1        0x83
#define AK7738_84_POWER_MANAGEMENT2        0x84
#define AK7738_85_RESET_CTRL               0x85
#define AK7738_86_RESERVED                 0x86
#define AK7738_87_RESERVED                 0x87
#define AK7738_90_RESERVED                 0x90
#define AK7738_91_PAD_DRIVE_SEL2           0x91
#define AK7738_92_PAD_DRIVE_SEL3           0x92
#define AK7738_93_PAD_DRIVE_SEL4           0x93
#define AK7738_94_PAD_DRIVE_SEL5           0x94
#define AK7738_95_RESERVED                 0x95
#define AK7738_100_DEVICE_ID               0x100
#define AK7738_101_REVISION_NUM            0x101
#define AK7738_102_DSP_ERROR_STATUS        0x102
#define AK7738_103_SRC_STATUS              0x103
#define AK7738_104_STO_READ_OUT            0x104
#define AK7738_105_MICGAIN_READ            0x105
#define AK7738_VIRT_106_DSP1OUT1_MIX       0x106
#define AK7738_VIRT_107_DSP1OUT2_MIX       0x107
#define AK7738_VIRT_108_DSP1OUT3_MIX       0x108
#define AK7738_VIRT_109_DSP1OUT4_MIX       0x109
#define AK7738_VIRT_10A_DSP1OUT5_MIX       0x10A
#define AK7738_VIRT_10B_DSP1OUT6_MIX       0x10B
#define AK7738_VIRT_10C_DSP2OUT1_MIX       0x10C
#define AK7738_VIRT_10D_DSP2OUT2_MIX       0x10D
#define AK7738_VIRT_10E_DSP2OUT3_MIX       0x10E
#define AK7738_VIRT_10F_DSP2OUT4_MIX       0x10F
#define AK7738_VIRT_110_DSP2OUT5_MIX       0x110
#define AK7738_VIRT_111_DSP2OUT6_MIX       0x111


#define AK7738_MAX_REGISTERS (AK7738_VIRT_111_DSP2OUT6_MIX + 1)

#define AK7738_VIRT_REGISTER    AK7738_VIRT_106_DSP1OUT1_MIX

/* Bitfield Definitions */

/* AK7738_C0_CLOCK_SETTING1 (0xC0) Fields */
#define AK7738_FS				0x07
#define AK7738_FS_8KHZ			(0x00 << 0)
#define AK7738_FS_12KHZ			(0x01 << 0)
#define AK7738_FS_16KHZ			(0x02 << 0)
#define AK7738_FS_24KHZ			(0x03 << 0)
#define AK7738_FS_32KHZ			(0x04 << 0)
#define AK7738_FS_48KHZ			(0x05 << 0)
#define AK7738_FS_96KHZ			(0x06 << 0)

#define AK7738_M_S				0x30		//CKM1-0 (CKM2 bit is not use)
#define AK7738_M_S_0			(0 << 4)	//Master, XTI=12.288MHz
#define AK7738_M_S_1			(1 << 4)	//Master, XTI=18.432MHz
#define AK7738_M_S_2			(2 << 4)	//Slave, XTI=12.288MHz
#define AK7738_M_S_3			(3 << 4)	//Slave, BICK

/* AK7738_C2_SERIAL_DATA_FORMAT (0xC2) Fields */
/* LRCK I/F Format */
#define AK7738_LRIF_I2S_MODE		0
#define AK7738_LRIF_MSB_MODE		5
#define AK7738_LRIF_LSB_MODE		5
#define AK7738_LRIF_PCM_SHORT_MODE	6
#define AK7738_LRIF_PCM_LONG_MODE	7
/* Input Format is set as "MSB(24- bit)" by following register.
   CONT03(DIF2,DOF2), CONT06(DIFDA, DIF1), CONT07(DOF4,DOF3,DOF1) */

/* AK7738_CA_CLK_SDOUT_SETTING (0xCA) Fields */
#define AK7738_BICK_LRCK			(3 << 5)	//BICKOE, LRCKOE


#define MAX_LOOP_TIMES		3

#define COMMAND_WRITE_REG			0xC0
#define COMMAND_READ_REG			0x40

#define COMMAND_CRC_READ			0x72
#define COMMAND_MIR1_READ			0x76
#define COMMAND_MIR2_READ			0x77

#define COMMAND_WRITE_CRAM_RUN		0x80
#define COMMAND_WRITE_CRAM_EXEC     0xA4

#define COMMAND_WRITE_CRAM          0xB4
#define COMMAND_WRITE_PRAM          0xB8
#define COMMAND_WRITE_OFREG         0xB2

#define COMMAND_WRITE_CRAM_SUB      0xB6
#define COMMAND_WRITE_PRAM_SUB      0xBA

#define	TOTAL_NUM_OF_PRAM_MAX		40963
#define	TOTAL_NUM_OF_CRAM_MAX		18435
#define	TOTAL_NUM_OF_OFREG_MAX		195
#define	TOTAL_NUM_OF_SUB_PRAM_MAX	5123
#define	TOTAL_NUM_OF_SUB_CRAM_MAX	6147


static const char *ak7738_firmware_pram[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
    "dsp3_data1",
    "dsp3_data2",
    "dsp3_data3",
    "dsp3_data4",
    "dsp3_data5",
};

static const char *ak7738_firmware_cram[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
    "dsp3_data1",
    "dsp3_data2",
    "dsp3_data3",
    "dsp3_data4",
    "dsp3_data5",
};

static const char *ak7738_firmware_ofreg[] =
{
    "Off",
    "basic1",
    "basic2",
    "dsp1_data1",
    "dsp1_data2",
    "dsp1_data3",
    "dsp1_data4",
    "dsp1_data5",
    "dsp2_data1",
    "dsp2_data2",
    "dsp2_data3",
    "dsp2_data4",
    "dsp2_data5",
};

enum {
	AIF_PORT1 = 0,
	AIF_PORT2,
	AIF_PORT3,
	AIF_PORT4,
	AIF_PORT5,
	NUM_AIF_DAIS,
};
/*Next section is based on DSP firmware, need modify them by different
 * firmware.*/
// Component: SFADER_2 (fader_mul2_1)
#define AK7738_X9REF_FIRMWARE 1
#ifdef AK7738_X9REF_FIRMWARE
#define CRAM_ADDR_LEVEL_SFADER_2 0x0000
#define CRAM_ADDR_ATT_TIME_SFADER_2 0x0001
#define CRAM_ADDR_REL_TIME_SFADER_2 0x0003

// Component: HPF2_1 (biquad2_1)
#define CRAM_ADDR_BAND1_HPF2_1 0x0006

// Component: SFADER_3 (fader_mul2_2)
#define CRAM_ADDR_LEVEL_SFADER_3 0x0010
#define CRAM_ADDR_ATT_TIME_SFADER_3 0x0011
#define CRAM_ADDR_REL_TIME_SFADER_3 0x0013

// Component: HPF2_2 (biquad2_2)
#define CRAM_ADDR_BAND1_HPF2_2 0x0016

// Component: SWITCH_1 (mixerl2_1)
#define CRAM_ADDR_VOL_IN1_SWITCH_1 0x0020
#define CRAM_ADDR_VOL_IN2_SWITCH_1 0x0021
#endif
#endif

