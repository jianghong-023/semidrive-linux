# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_linux.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_ref_native_ivi_serdes_8g_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_ref_native_ivi_8g.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_refa04_native_ivi_serdes_8g.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_refa04_native_ivi_serdes_8g_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_refa04_native_ivi_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_refa04_native_ivi_serdes_cockpit.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_refa04_native_ivi_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_refa04_native_ivi_serdes_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_refa04_native_ivi_a.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_refa04_native_ivi_b.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9e_refa04_native_ivi_avm.dtb

dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_ref_native_cluster_8g_bt.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_V9) += v9f_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_G9) += g9x_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_G9) += g9x_ii4_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_cluster_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_ivi_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_icl02_native_ivi.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_icl02_native_cluster.dts
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_icl02_native_cluster.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_ivi_serdes_db5071.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ms_native_linux_serdes_nobt.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_linux_serdes_classic.dtb

dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ref_native_ivi.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ref_native_ivi_serdes_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ref_native_linux_serdes.dtb

dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_ref_native_cluster_8g_bt_a.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_ref_native_cluster_8g_bt_b.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_ref_native_ivi_8g_a.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_ultra_ref_native_ivi_8g_b.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_ms_native_cluster_8g.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high-plus_ms_native_ivi_serdes_8g_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ms_native_cluster_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ms_native_ivi_serdes_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ms_native_ivi_serdes.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ms_native_cluster.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ms_native_ivi_serdes_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_mid_ms_native_linux_serdes_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_V9) += v9t_a_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_V9) += v9t_a_ref_gp106.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_V9) += v9t_b_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9e_ref_native_ivi_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9e_ms_native_ivi_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9e_ref_native_linux_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9e_refa04_native_linux_avm.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_X9) += x9_high_ref_native_serdes_nobt.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_D9) += d9_lite_d9310_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_D9) += d9_std_d9340_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_D9) += d9_plus_d9350_ap1_ref.dtb
dtb-$(CONFIG_ARCH_SEMIDRIVE_D9) += d9_plus_d9350_ap2_ref.dtb
#subdir-y	+= archive

always		:= $(dtb-y)
subdir-y	+= overlays
clean-files	:= *.dtb

# Add __symbols__ node
ifeq ($(CONFIG_ARCH_SEMIDRIVE),y)
	DTC_FLAGS ?= -@
endif
