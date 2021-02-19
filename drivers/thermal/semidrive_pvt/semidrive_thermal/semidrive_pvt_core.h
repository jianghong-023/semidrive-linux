

#ifndef __SEMIDRIVE_PVT_CORE_H__
#define __SEMIDRIVE_PVT_CORE_H__

#define PVT_ATTR_LENGTH	(40)

#define MAX_PVT_SENSOR_NUM		(4)


/*

PVT REG LIST

*/
#define PVT_REGISTER_DOUT_ADDR_OFFSET 0x4
#define PVT_REGISTER_HYST_H_OFFSET 0x8
#define PVT_REGISTER_HYST_L_OFFSET 0xC
#define PVT_REGISTER_HYST_R_OFFSET 0x10
#define PVT_REGISTER_HYST_F_OFFSET 0x14
#define PVT_REGISTER_HYST_TIME_OFFSET 0x18
#define PVT_REGISTER_INT_EN_OFFSET 0x1C
#define PVT_REGISTER_INT_STATUS_OFFSET 0x20
#define PVT_REGISTER_INT_CLR_OFFSET 0x24
#define PVT_REGISTER_HYST_H1_OFFSET 0x28
#define PVT_REGISTER_HYST_L1_OFFSET 0x2C
#define PVT_REGISTER_HYST_R1_OFFSET 0x30
#define PVT_REGISTER_HYST_F1_OFFSET 0x34
#define PVT_REGISTER_HYST_TIME1_OFFSET 0x38
#define PVT_REGISTER_INT_EN1_OFFSET 0x3C
#define PVT_REGISTER_INT_STATUS1_OFFSET 0x40
#define PVT_REGISTER_INT_CLR1_OFFSET 0x44


#define PVT_PRINT_WAIT_TIME 3 // seconds

#define PVT_PRINT_WAIT_TIME_VALUE (PVT_PRINT_WAIT_TIME*1000)

/* pvt ctrl
*____________________________________________________________________
*|------04------|-----03-----|-----02-----|-----01-----|-----00-----|
*|software ctrl |         p mode          |    v mode  | ctrl mode  |
*/
// ctrl mode : 0 from fuse; 1 from register
// v mode : 1 voltage detect mode
// p mode : 10 lvt ; 01 ulvt ; 11 svt
// software ctrl sensor enable: 0 disable ; 1 enable
// v mode and p mode all 0 is t mode(temperature mode)

#define PVT_CTRL_CTRL_MODE_SHEFT 0
#define PVT_CTRL_V_MODE_SHEFT 1
#define PVT_CTRL_P_MODE_0_SHEFT 2
#define PVT_CTRL_P_MODE_1_SHEFT 3
#define PVT_CTRL_SOFTWARE_CTRL_SHEFT 4



#define PVT_DEVICE_TYPE_ULVT 1
#define PVT_DEVICE_TYPE_LVT 2
#define PVT_DEVICE_TYPE_SVT 3

#define PVT_OUT_TYPE_OFF 0
#define PVT_OUT_TYPE_ON_ALL 1
#define PVT_OUT_TYPE_ON_ONE_TYPE 2

#define MAX_PVT_DEVICE_NUM 2
#define MAX_PVT_ALARM_NUM 2


#define pvtprintk(fmt, arg...) \
	pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)

#define to_thermal_sensor_device(x)  \
		container_of((x), struct semidrive_pvt_data, pdev)
#define SETMASK(width, shift)  ((width ? ((-1U) >> (32-width)) : 0)  << (shift))
#define CLRMASK(width, shift)  (~(SETMASK(width, shift)))
#define GET_BITS(shift, width, reg)     \
			(((reg) & SETMASK(width, shift)) >> (shift))
#define SET_BITS(shift, width, reg, val) \
			(((reg) & CLRMASK(width, shift)) | (val << (shift)))

typedef enum pvt_ctrl_offset {
    PVT_CTRL_CTRL_MODE = 0,
    PVT_CTRL_V_MODE,
    PVT_CTRL_P_MODE_0,
    PVT_CTRL_P_MODE_1,
    PVT_CTRL_SOFTWARE_CTRL,
} pvt_ctrl_offset_t;


enum pvt_mode_t {
	PVT_MODE_TYPE_TEMP = 0,
	PVT_MODE_TYPE_VOL,
	PVT_MODE_TYPE_P,
};


struct pvt_info_attr {
	struct device_attribute attr;
	char name[PVT_ATTR_LENGTH];
};

struct semidrive_pvt_data {
	void __iomem *base_addr;
	struct mutex ths_mutex_lock;
	struct platform_device *pdev;
	struct semidrive_pvt_controller *ctrl;
	struct thermal_sensor_coefficent *ths_coefficent;
	int irq;
	int shut_temp;
	int sensor_cnt;
	int combine_cnt;
	int alarm_low_temp;
	int alarm_high_temp;
	int alarm_temp_hysteresis;
	int cp_ft_flag; /*00 cp, 01 ft */
	const char *pvt_mode;
	char pvt_set_mode;
	void *data; /* specific sensor data */
	struct pvt_info_attr *ths_info_attrs;
	struct semidrive_pvt_sensor **comb_sensor;
};

/**
 * to recored the single pvt sensor data about name and temp
 */
struct thermal_sensor_info {
	int id;
	char *ths_name;
	int temp;
	int alarm_irq_type;
	bool irq_enabled;
};


struct thermal_reg {
	char name[32];
	u32 offset;
	u32 value;
	u32 init_type;
};

struct normal_temp_para {
	u32 MUL_PARA;
	u32 DIV_PARA;
	u32 MINU_PARA;
};

struct split_temp_point {
	long split_temp;
	u32 split_reg;
};




/**
 * temperature = ( MINUPA - reg * MULPA) / DIVPA
 */
struct temp_calculate_coefficent {
	struct split_temp_point down_limit;
	struct split_temp_point up_limit;
	struct normal_temp_para nt_para[MAX_PVT_SENSOR_NUM];
};

/**
 * this struct include all of the sensor driver init para
 */
struct thermal_sensor_coefficent {
	struct temp_calculate_coefficent *calcular_para;
	struct thermal_reg *reg_para;
};

extern int pvt_driver_init_reg(struct semidrive_pvt_data *);
extern int pvt_driver_disable_reg(struct semidrive_pvt_data *);
extern int pvt_driver_get_temp(struct semidrive_pvt_data *,
				int);
extern int pvt_driver_startup(struct semidrive_pvt_data *,
				struct device *);
extern void pvt_drvier_remove_trip_attrs(struct semidrive_pvt_data *);
extern int pvt_driver_create_sensor_info_attrs(struct semidrive_pvt_data *,
				struct thermal_sensor_info *);

#endif
