#ifndef SC8815_REG_H
#define SC8815_REG_H

#include <Arduino.h>

// 寄存器地址 (共 25 个寄存器, 地址 0x00~0x19, 0x18 保留)
enum class SC8815_REG : uint8_t {
    VBAT_SET            = 0x00,  ///< VBAT 充电目标电压 / 电池配置
    VBUSREF_I_SET       = 0x01,  ///< VBUS 内部参考电压 [7:0] (FB_SEL=0)
    VBUSREF_I_SET2      = 0x02,  ///< VBUS 内部参考电压 [9:8] + Reserved
    VBUSREF_E_SET       = 0x03,  ///< VBUS 外部参考电压 [7:0] (FB_SEL=1)
    VBUSREF_E_SET2      = 0x04,  ///< VBUS 外部参考电压 [9:8] + Reserved
    IBUS_LIM_SET        = 0x05,  ///< IBUS 电流限制 (充/放电均生效)
    IBAT_LIM_SET        = 0x06,  ///< IBAT 电流限制 (充/放电均生效)
    VINREG_SET          = 0x07,  ///< VINREG 动态电源管理参考电压
    RATIO               = 0x08,  ///< 电流/电压比率配置 (含 reserved bits)
    CTRL0_SET           = 0x09,  ///< 控制0: EN_OTG, VINREG_RATIO, FREQ, DT
    CTRL1_SET           = 0x0A,  ///< 控制1: FB_SEL, DIS_OVP, 充电相关
    CTRL2_SET           = 0x0B,  ///< 控制2: FACTORY, EN_DITHER, SLEW
    CTRL3_SET           = 0x0C,  ///< 控制3: AD_START, EN_PFM, 保护配置
    VBUS_FB_VALUE       = 0x0D,  ///< VBUS ADC 值 [7:0]
    VBUS_FB_VALUE2      = 0x0E,  ///< VBUS ADC 值 [9:8] + Reserved
    VBAT_FB_VALUE       = 0x0F,  ///< VBAT ADC 值 [7:0]
    VBAT_FB_VALUE2      = 0x10,  ///< VBAT ADC 值 [9:8] + Reserved
    IBUS_VALUE          = 0x11,  ///< IBUS ADC 值 [7:0]
    IBUS_VALUE2         = 0x12,  ///< IBUS ADC 值 [9:8] + Reserved
    IBAT_VALUE          = 0x13,  ///< IBAT ADC 值 [7:0]
    IBAT_VALUE2         = 0x14,  ///< IBAT ADC 值 [9:8] + Reserved
    ADIN_VALUE          = 0x15,  ///< ADIN ADC 值 [7:0]
    ADIN_VALUE2         = 0x16,  ///< ADIN ADC 值 [9:8] + Reserved
    STATUS              = 0x17,  ///< 中断状态 (AC_OK, INDET, VBUS_SHORT, OTP, EOC)
    MASK                = 0x19   ///< 中断掩码
};

//  VBAT_SET 寄存器配置位定义

/** @brief IRCOMP: 电池内阻补偿设置, PSTOP=HIGH时设置 (REG_VBAT_SET[7:6]) */
enum SC8815_IRCOMP_t : uint8_t {
    SC8815_IRCOMP_0mOhm   = 0b00,  ///< 补偿 0mΩ (默认)
    SC8815_IRCOMP_20mOhm  = 0b01,  ///< 补偿 20mΩ
    SC8815_IRCOMP_40mOhm  = 0b10,  ///< 补偿 40mΩ
    SC8815_IRCOMP_80mOhm  = 0b11   ///< 补偿 80mΩ
};

/** @brief VBAT_SEL: VBAT 目标电压设定方式, PSTOP=HIGH时设置 (REG_VBAT_SET[5]) */
enum SC8815_VBAT_SEL_t : uint8_t {
    SC8815_VBAT_SEL_INTERNAL = 0b0,  ///< 内部设定 (通过 CSEL + VCELL_SET)
    SC8815_VBAT_SEL_EXTERNAL = 0b1   ///< 外部电阻分压设定 (VBATS 引脚)
};

/** @brief CSEL: 电池串联节数, PSTOP=HIGH时设置 (REG_VBAT_SET[4:3]) */
enum SC8815_CSEL_t : uint8_t {
    SC8815_CSEL_1S = 0b00,  ///< 1 节电池
    SC8815_CSEL_2S = 0b01,  ///< 2 节电池串联
    SC8815_CSEL_3S = 0b10,  ///< 3 节电池串联
    SC8815_CSEL_4S = 0b11   ///< 4 节电池串联
};

/** @brief VCELL: 单节电池目标电压, PSTOP=HIGH时设置 (REG_VBAT_SET[2:0]) */
enum SC8815_VCELL_t : uint8_t {
    SC8815_VCELL_4V10 = 0b000,  ///< 4.10V/节
    SC8815_VCELL_4V20 = 0b001,  ///< 4.20V/节 (默认)
    SC8815_VCELL_4V25 = 0b010,  ///< 4.25V/节
    SC8815_VCELL_4V30 = 0b011,  ///< 4.30V/节
    SC8815_VCELL_4V35 = 0b100,  ///< 4.35V/节
    SC8815_VCELL_4V40 = 0b101,  ///< 4.40V/节
    SC8815_VCELL_4V45 = 0b110,  ///< 4.45V/节
    SC8815_VCELL_4V50 = 0b111   ///< 4.50V/节
};

//  RATIO 寄存器配置位定义

/** @brief IBAT_RATIO: IBAT 电流限制/监控比率, PSTOP=HIGH时设置 (REG_RATIO[4]) */
enum SC8815_IBAT_RATIO_t : uint8_t {
    SC8815_IBAT_RATIO_6x  = 0b0,  ///< 6x: 最大电流范围 ~6A
    SC8815_IBAT_RATIO_12x = 0b1   ///< 12x (默认): 最大电流范围 ~12A (建议 IBAT<10A 时使用)
};

/**
 * @brief IBUS_RATIO: IBUS 电流限制/监控比率, PSTOP=HIGH时设置 (REG_RATIO[3:2])
 * @note 值 00b 和 11b 不允许 (数据手册禁止)
 */
enum SC8815_IBUS_RATIO_t : uint8_t {
    SC8815_IBUS_RATIO_3x = 0b10,   ///< 3x (默认): 最大电流范围 ~3A
    SC8815_IBUS_RATIO_6x = 0b01    ///< 6x: 最大电流范围 ~6A
};

/** @brief VBAT_MON_RATIO: VBAT 电压监控比率, PSTOP=HIGH时设置 (REG_RATIO[1]) */
enum SC8815_VBAT_MON_RATIO_t : uint8_t {
    SC8815_VBAT_MON_RATIO_12_5x = 0b0,  ///< 12.5x (默认): VBAT max ~25.6V
    SC8815_VBAT_MON_RATIO_5x    = 0b1   ///< 5x: VBAT max ~10.24V (建议 VBAT<10.24V 时使用)
};

/** @brief VBUS_RATIO: VBUS 电压设定/监控比率, PSTOP=HIGH时设置 (REG_RATIO[0]) */
enum SC8815_VBUS_RATIO_t : uint8_t {
    SC8815_VBUS_RATIO_12_5x = 0b0,  ///< 12.5x (默认): VBUS max ~25.6V
    SC8815_VBUS_RATIO_5x    = 0b1   ///< 5x: VBUS max ~10.24V (建议 VBUS<10.24V 时使用)
};

//  CTRL0_SET 寄存器配置位定义

/** @brief EN_OTG: OTG 放电模式使能 (REG_CTRL0[7]) */
enum SC8815_EN_OTG_t : uint8_t {
    SC8815_EN_OTG_DISABLE = 0b0,  ///< 禁用 OTG 放电模式
    SC8815_EN_OTG_ENABLE  = 0b1   ///< 启用 OTG 放电模式
};

/** @brief VINREG_RATIO: VINREG 电压设定/监控比率 (REG_CTRL0[4], 1-bit field) */
enum SC8815_VINREG_RATIO_t : uint8_t {
    SC8815_VINREG_RATIO_100x = 0b0,  ///< 100x, 100mV/步, 最大 25.6V (默认, POR=0)
    SC8815_VINREG_RATIO_40x  = 0b1   ///< 40x,  40mV/步, 最大 10.24V (建议 VBUS < 12V 时使用)
};

/** @brief FREQ: 开关频率选择 (REG_CTRL0[3:2]) */
enum SC8815_FREQ_t : uint8_t {
    SC8815_FREQ_150kHz = 0b00,  ///< 150kHz
    SC8815_FREQ_300kHz = 0b01,  ///< 300kHz (默认, 值 01 或 10 均为 300kHz)
    SC8815_FREQ_450kHz = 0b11   ///< 450kHz
};

/** @brief DT: 开关死区时间 (REG_CTRL0[1:0]) */
enum SC8815_DT_t : uint8_t {
    SC8815_DT_20ns = 0b00,  ///< 20ns (默认)
    SC8815_DT_40ns = 0b01,  ///< 40ns
    SC8815_DT_60ns = 0b10,  ///< 60ns
    SC8815_DT_80ns = 0b11   ///< 80ns
};

//  CTRL1_SET 寄存器配置位定义

/** @brief ICHAR_SEL: 充电电流选择 (REG_CTRL1[7]) */
enum SC8815_ICHAR_SEL_t : uint8_t {
    SC8815_ICHAR_SEL_IBUS = 0b0,  ///< 充电电流由 IBUS 电流值设定 (默认)
    SC8815_ICHAR_SEL_IBAT = 0b1   ///< 充电电流由 IBAT 电流值设定
};

/** @brief DIS_TRICKLE: 禁用涓流充电 (REG_CTRL1[6]) */
enum SC8815_DIS_TRICKLE_t : uint8_t {
    SC8815_DIS_TRICKLE_ENABLE = 0b1,  ///< 禁用涓流充电
    SC8815_DIS_TRICKLE_DISABLE = 0b0   ///< 启用涓流充电 (默认)
};

/** @brief DIS_TERM: 禁用自动充电终止 (REG_CTRL1[5]) */
enum SC8815_DIS_TERM_t : uint8_t {
    SC8815_DIS_TERM_ENABLE = 0b1,  ///< 禁用自动充电终止
    SC8815_DIS_TERM_DISABLE = 0b0   ///< 启用自动充电终止 (默认)
};

/** @brief FB_SEL: VBUS 参考电压选择 (REG_CTRL1[4]) */
enum SC8815_FB_SEL_t : uint8_t {
    SC8815_FB_SEL_INTERNAL = 0b0,  ///< 使用内部 VBUS 参考电压 (默认)
    SC8815_FB_SEL_EXTERNAL = 0b1   ///< 使用外部 FB 引脚参考电压
};

/** @brief TRICKLE_SET: 涓流充电阈值设置 (REG_CTRL1[3]) */
enum SC8815_TRICKLE_SET_t : uint8_t {
    SC8815_TRICKLE_SET_70 = 0b0,  ///< VBAT 电压的 70% (默认)
    SC8815_TRICKLE_SET_60 = 0b1   ///< VBAT 电压的 60%
};

/** @brief DIS_OVP: 禁用过压保护 (REG_CTRL1[2]) */
enum SC8815_DIS_OVP_t : uint8_t {
    SC8815_DIS_OVP_ENABLE = 0b1,  ///< 禁用过压保护
    SC8815_DIS_OVP_DISABLE = 0b0   ///< 启用过压保护 (默认)
};

//  CTRL2_SET 寄存器配置位定义

/** @brief FACTORY: 工厂配置 (REG_CTRL2[3]) */
enum SC8815_FACTORY_t : uint8_t {
    SC8815_FACTORY_DEFAULT = 0b0,  ///< (默认)
    SC8815_FACTORY_POWERUP = 0b1  ///< 上电后必须写为1
};

/** @brief EN_DITHER: 扩频抖动使能 (REG_CTRL2[2]) */
enum SC8815_EN_DITHER_t : uint8_t {
    SC8815_EN_DITHER_DISABLE = 0b0,  ///< 禁用扩频抖动 (默认)
    SC8815_EN_DITHER_ENABLE = 0b1   ///< 启用扩频抖动
};

/** @brief SLEW_SET: VBUS 动态电压变化斜率 (REG_CTRL2[1:0], 仅放电模式) */
enum SC8815_SLEW_t : uint8_t {
    SC8815_SLEW_SET_1mV_us = 0b00,  ///< 1mV/µs
    SC8815_SLEW_SET_2mV_us = 0b01,  ///< 2mV/µs (默认)
    SC8815_SLEW_SET_4mV_us = 0b10,  ///< 4mV/µs
    SC8815_SLEW_SET_8mV_us = 0b11   ///< 8mV/µs
};

//  CTRL3_SET 寄存器配置位定义

/** @brief EN_PGATE: PGATE 引脚控制 (REG_CTRL3[7], 枚举值=寄存器位值) */
enum SC8815_EN_PGATE_t : uint8_t {
    SC8815_EN_PGATE_OFF = 0b0,  ///< PGATE 输出高电平, 关断外部 PMOS (默认, POR=0)
    SC8815_EN_PGATE_ON  = 0b1   ///< PGATE 输出低电平, 导通外部 PMOS
};

/** @brief GPO_CTRL: GPO 引脚控制 (REG_CTRL3[6], 枚举值=寄存器位值) */
enum SC8815_GPO_CTRL_t : uint8_t {
    SC8815_GPO_CTRL_OD = 0b0,  ///< 开漏输出, 需外部上拉 (默认, POR=0)
    SC8815_GPO_CTRL_LO = 0b1   ///< 内部下拉输出低电平
};

/** @brief AD_START: AD转换启动 (REG_CTRL3[5]) */
enum SC8815_AD_START_t : uint8_t {
    SC8815_AD_START_DISABLE = 0b0,  ///< 禁用AD转换 (默认)
    SC8815_AD_START_ENABLE = 0b1   ///< 启用AD转换
};

/** @brief ILIM_BW_SEL: ILIM 环路带宽选择 (REG_CTRL3[4]) */
enum SC8815_ILIM_BW_SEL_t : uint8_t {
    SC8815_ILIM_BW_SEL_5K    = 0b0,  ///< 5kHz 带宽 (默认, POR=0)
    SC8815_ILIM_BW_SEL_1_25K = 0b1   ///< 1.25kHz 带宽
};

/** @brief LOOP_SET: 环路响应控制 (REG_CTRL3[3]) */
enum SC8815_LOOP_SET_t : uint8_t {
    SC8815_LOOP_SET_NORMAL = 0b0,  ///< 正常响应 (默认)
    SC8815_LOOP_SET_IMPROVE = 0b1   ///< 改进响应
};

/** @brief DIS_ShortFoldBack: 禁用短路电流折返保护 (REG_CTRL3[2]) */
enum SC8815_DIS_SHORT_FOLDBACK_t : uint8_t {
    SC8815_DIS_SHORT_FOLDBACK_ENABLE  = 0b1,  ///< 禁用短路电流折返保护
    SC8815_DIS_SHORT_FOLDBACK_DISABLE = 0b0   ///< 启用短路电流折返保护 (默认)
};

/** @brief EOC_SET: 充电终止电流阈值 (REG_CTRL3[1]) */
enum SC8815_EOC_t : uint8_t {
    SC8815_EOC_SET_1_25 = 0b0,  ///< 充电电流的 1/25
    SC8815_EOC_SET_1_10 = 0b1   ///< 充电电流的 1/10 (默认)
};

/** @brief EN_PFM: 启用 PFM 模式 (REG_CTRL3[0]) */
enum SC8815_EN_PFM_t : uint8_t {
    SC8815_EN_PFM_DISABLE = 0b0,  ///< 禁用 PFM 模式 (默认)
    SC8815_EN_PFM_ENABLE = 0b1   ///< 启用 PFM 模式
};

/** @brief VBAT_SET 寄存器配置结构体 (地址 0x00, PSTOP=HIGH 时可写) */
struct SC8815_VBATSet_Config_t
{
    SC8815_IRCOMP_t ircomp;     /**< [7:6] 电池内阻补偿 (IRCOMP) */
    SC8815_VBAT_SEL_t sel;      /**< [5]   VBAT 目标电压设定方式 (VBAT_SEL) */
    SC8815_CSEL_t csel;         /**< [4:3] 电池串联节数 (CSEL) */
    SC8815_VCELL_t vcell;       /**< [2:0] 单节电池目标电压 (VCELL) */
};

/** @brief RATIO 寄存器配置结构体 (地址 0x08, PSTOP=HIGH 时可写) */
struct  SC8815_Ratio_Config_t
{
    SC8815_IBAT_RATIO_t ibat_ratio;             /**< [4]   IBAT 电流限制/监控比率 (IBAT_RATIO) */
    SC8815_IBUS_RATIO_t ibus_ratio;             /**< [3:2] IBUS 电流限制/监控比率 (IBUS_RATIO), 00/11 禁止 */
    SC8815_VBAT_MON_RATIO_t vbat_mon_ratio;     /**< [1]   VBAT 电压监控比率 (VBAT_MON_RATIO) */
    SC8815_VBUS_RATIO_t vbus_ratio;             /**< [0]   VBUS 电压设定/监控比率 (VBUS_RATIO) */
};

/** @brief CTRL0_SET 寄存器配置结构体 (地址 0x09) */
struct SC8815_CTRL0_Config_t
{
    SC8815_EN_OTG_t en_otg;                 /**< [7]   OTG 放电模式使能 (EN_OTG) */
    SC8815_VINREG_RATIO_t vinreg_ratio;     /**< [4]   VINREG 电压设定/监控比率 (VINREG_RATIO) */
    SC8815_FREQ_t freq;                     /**< [3:2] 开关频率选择 (FREQ) */
    SC8815_DT_t dt;                         /**< [1:0] 开关死区时间 (DT) */
};

/** @brief CTRL1_SET 寄存器配置结构体 (地址 0x0A) */
struct SC8815_CTRL1_Config_t
{
    SC8815_ICHAR_SEL_t ichar_sel;           /**< [7] 充电电流选择 (ICHAR_SEL) */
    SC8815_DIS_TRICKLE_t dis_trickle;       /**< [6] 禁用涓流充电 (DIS_TRICKLE) */
    SC8815_DIS_TERM_t dis_term;            /**< [5] 禁用自动充电终止 (DIS_TERM) */
    SC8815_FB_SEL_t fb_sel;                /**< [4] VBUS 参考电压选择 (FB_SEL) */
    SC8815_TRICKLE_SET_t trickle_set;      /**< [3] 涓流充电阈值 (TRICKLE_SET) */
    SC8815_DIS_OVP_t dis_ovp;              /**< [2] 禁用过压保护 (DIS_OVP) */
};

/** @brief CTRL2_SET 寄存器配置结构体 (地址 0x0B) */
struct SC8815_CTRL2_Config_t
{
    SC8815_FACTORY_t factory_bit = SC8815_FACTORY_POWERUP;  /**< [3]   工厂配置, 上电后必须写1 (FACTORY) */
    SC8815_EN_DITHER_t en_dither;                           /**< [2]   扩频抖动使能 (EN_DITHER) */
    SC8815_SLEW_t slew;                                     /**< [1:0] VBUS 动态电压变化斜率, 仅放电模式 (SLEW_SET) */
};

/** @brief CTRL3_SET 寄存器配置结构体 (地址 0x0C) */
struct SC8815_CTRL3_Config_t
{
    SC8815_EN_PGATE_t en_pgate;                         /**< [7] PGATE 引脚控制 (EN_PGATE) */
    SC8815_GPO_CTRL_t gpo_ctrl;                         /**< [6] GPO 引脚输出模式 (GPO_CTRL) */
    SC8815_AD_START_t ad_start;                         /**< [5] AD 转换启动 (AD_START) */
    SC8815_ILIM_BW_SEL_t ilim_bw;                       /**< [4] ILIM 环路带宽选择 (ILIM_BW_SEL) */
    SC8815_LOOP_SET_t loop_set;                         /**< [3] 环路响应控制 (LOOP_SET) */
    SC8815_DIS_SHORT_FOLDBACK_t dis_short_foldback;     /**< [2] 禁用短路电流折返保护 (DIS_ShortFoldBack) */
    SC8815_EOC_t eoc_set;                               /**< [1] 充电终止电流阈值 (EOC_SET) */
    SC8815_EN_PFM_t en_pfm;                             /**< [0] PFM 模式使能 (EN_PFM) */
};

/** @brief 状态列表 */
struct SC8815_Status_t
{
    bool ACOK;
    bool INDET;
    bool VBUSShort;
    bool OTP;
    bool EOC;
};

/** @brief MASK 中断掩码寄存器配置结构体 (地址 0x19) */
struct SC8815_IRQMASK_Config_t
{
    bool ac_ok_mask;        /**< [4] AC 适配器接入中断掩码 (AC_OK) */
    bool indet_mask;        /**< [3] VBUS 插入检测中断掩码 (VBUS_INSERT) */
    bool vbus_short_mask;   /**< [2] VBUS 短路保护中断掩码 (VBUS_SHORT) */
    bool otp_mask;          /**< [1] 过温保护中断掩码 (OTP) */
    bool eoc_mask;          /**< [0] 充电终止中断掩码 (EOC) */
};

#endif