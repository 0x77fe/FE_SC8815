/**
 * @file    sc8815.h
 * @brief   SC8815 双向升降压充电控制器 — I2C 驱动库
 *
 * SC8815 是 Southchip 的高效率同步双向 Buck-Boost 控制器, 支持:
 *   - 1~6 节锂电池充电管理 (涓流/恒流/恒压/终止)
 *   - 反向放电 (OTG) 模式, 输出可调电压
 *   - I2C 可编程: 充电电流/电压, 放电输出电压, 输入/输出电流限制, 开关频率
 *   - 10-bit ADC 监控 VBUS/VBAT 电压及 IBUS/IBAT 电流
 *   - 完善的保护: UVP, OVP, OCP, SCP, OTP
 *
 * I2C 地址: 7-bit 0x74 (8-bit 写 0xE8, 读 0xE9)
 * 封装: QFN-32 (4mm×4mm)
 *
 * 使用注意事项:
 *   - 所有 set*() 函数采用 read-modify-write 保护 reserved bits
 *   - begin() 不重置 Wire 总线, 调用前需自行初始化 Wire
 *   - setCTRL2() 默认设置 FACTORY bit=1 (数据手册强制要求)
 *   - 电流限制计算假设 RSNS1=RSNS2=10mΩ
 *   - ADC 读数仅在 AD_START=1 时有效
 */

#ifndef SC8815_H
#define SC8815_H

#include <Arduino.h>
#include <Wire.h>

// ============================================================
// I2C 地址
// ============================================================
#define SC8815_I2C_ADDRESS  0x74   ///< 7-bit I2C 地址

// ============================================================
// 寄存器地址 (共 26 个寄存器, 地址 0x00~0x1B, 0x18 保留)
// ============================================================
#define SC8815_REG_VBAT_SET            0x00  ///< VBAT 充电目标电压 / 电池配置
#define SC8815_REG_VBUSREF_I_SET       0x01  ///< VBUS 内部参考电压 [7:0] (FB_SEL=0)
#define SC8815_REG_VBUSREF_I_SET2      0x02  ///< VBUS 内部参考电压 [9:8] + Reserved
#define SC8815_REG_VBUSREF_E_SET       0x03  ///< VBUS 外部参考电压 [7:0] (FB_SEL=1)
#define SC8815_REG_VBUSREF_E_SET2      0x04  ///< VBUS 外部参考电压 [9:8] + Reserved
#define SC8815_REG_IBUS_LIM_SET        0x05  ///< IBUS 电流限制 (充/放电均生效)
#define SC8815_REG_IBAT_LIM_SET        0x06  ///< IBAT 电流限制 (充/放电均生效)
#define SC8815_REG_VINREG_SET          0x07  ///< VINREG 动态电源管理参考电压
#define SC8815_REG_RATIO               0x08  ///< 电流/电压比率配置 (含 reserved bits)
#define SC8815_REG_CTRL0_SET           0x09  ///< 控制0: EN_OTG, VINREG_RATIO, FREQ, DT
#define SC8815_REG_CTRL1_SET           0x0A  ///< 控制1: FB_SEL, DIS_OVP, 充电相关
#define SC8815_REG_CTRL2_SET           0x0B  ///< 控制2: FACTORY, EN_DITHER, SLEW
#define SC8815_REG_CTRL3_SET           0x0C  ///< 控制3: AD_START, EN_PFM, 保护配置
#define SC8815_REG_VBUS_FB_VALUE       0x0D  ///< VBUS ADC 值 [7:0]
#define SC8815_REG_VBUS_FB_VALUE2      0x0E  ///< VBUS ADC 值 [9:8] + Reserved
#define SC8815_REG_VBAT_FB_VALUE       0x0F  ///< VBAT ADC 值 [7:0]
#define SC8815_REG_VBAT_FB_VALUE2      0x10  ///< VBAT ADC 值 [9:8] + Reserved
#define SC8815_REG_IBUS_VALUE          0x11  ///< IBUS ADC 值 [7:0]
#define SC8815_REG_IBUS_VALUE2         0x12  ///< IBUS ADC 值 [9:8] + Reserved
#define SC8815_REG_IBAT_VALUE          0x13  ///< IBAT ADC 值 [7:0]
#define SC8815_REG_IBAT_VALUE2         0x14  ///< IBAT ADC 值 [9:8] + Reserved
#define SC8815_REG_ADIN_VALUE          0x15  ///< ADIN ADC 值 [7:0]
#define SC8815_REG_ADIN_VALUE2         0x16  ///< ADIN ADC 值 [9:8] + Reserved
#define SC8815_REG_STATUS              0x17  ///< 中断状态 (AC_OK, INDET, VBUS_SHORT, OTP, EOC)
#define SC8815_REG_MASK                0x19  ///< 中断掩码

// ============================================================
//  VBAT_SET 寄存器配置位定义
// ============================================================

/** @brief IRCOMP: 电池内阻补偿设置, PSTOP=HIGH时设置 (REG_VBAT_SET[7:6]) */
typedef enum {
    SC8815_IRCOMP_0mOhm   = 0b00,  ///< 补偿 0mΩ (默认)
    SC8815_IRCOMP_20mOhm  = 0b01,  ///< 补偿 20mΩ
    SC8815_IRCOMP_40mOhm  = 0b10,  ///< 补偿 40mΩ
    SC8815_IRCOMP_80mOhm  = 0b11   ///< 补偿 80mΩ
} SC8815_IRCOMP_t;

/** @brief VBAT_SEL: VBAT 目标电压设定方式, PSTOP=HIGH时设置 (REG_VBAT_SET[5]) */
typedef enum {
    SC8815_VBAT_SEL_INTERNAL = 0b0,  ///< 内部设定 (通过 CSEL + VCELL_SET)
    SC8815_VBAT_SEL_EXTERNAL = 0b1   ///< 外部电阻分压设定 (VBATS 引脚)
} SC8815_VBAT_SEL_t;

/** @brief CSEL: 电池串联节数, PSTOP=HIGH时设置 (REG_VBAT_SET[4:3]) */
typedef enum {
    SC8815_CSEL_1S = 0b00,  ///< 1 节电池
    SC8815_CSEL_2S = 0b01,  ///< 2 节电池串联
    SC8815_CSEL_3S = 0b10,  ///< 3 节电池串联
    SC8815_CSEL_4S = 0b11   ///< 4 节电池串联
} SC8815_CSEL_t;

/** @brief VCELL: 单节电池目标电压, PSTOP=HIGH时设置 (REG_VBAT_SET[2:0]) */
typedef enum {
    SC8815_VCELL_4V10 = 0b000,  ///< 4.10V/节
    SC8815_VCELL_4V20 = 0b001,  ///< 4.20V/节 (默认)
    SC8815_VCELL_4V25 = 0b010,  ///< 4.25V/节
    SC8815_VCELL_4V30 = 0b011,  ///< 4.30V/节
    SC8815_VCELL_4V35 = 0b100,  ///< 4.35V/节
    SC8815_VCELL_4V40 = 0b101,  ///< 4.40V/节
    SC8815_VCELL_4V45 = 0b110,  ///< 4.45V/节
    SC8815_VCELL_4V50 = 0b111   ///< 4.50V/节
} SC8815_VCELL_t;


// ============================================================
//  RATIO 寄存器配置位定义
// ============================================================

/** @brief IBAT_RATIO: IBAT 电流限制/监控比率, PSTOP=HIGH时设置 (REG_RATIO[4]) */
typedef enum {
    SC8815_IBAT_RATIO_6x  = 0b0,  ///< 6x: 最大电流范围
    SC8815_IBAT_RATIO_12x = 0b1   ///< 12x (默认): 最大电流范围
} SC8815_IBAT_RATIO_t;

/**
 * @brief IBUS_RATIO: IBUS 电流限制/监控比率, PSTOP=HIGH时设置 (REG_RATIO[3:2])
 * @note 值 00b 和 11b 不允许 (数据手册禁止)
 */
typedef enum {
    SC8815_IBUS_RATIO_3x = 0b10,   ///< 3x (默认): 最大电流范围 ~3A
    SC8815_IBUS_RATIO_6x = 0b01    ///< 6x: 最大电流范围 ~6A
} SC8815_IBUS_RATIO_t;

/** @brief VBAT_MON_RATIO: VBAT 电压监控比率, PSTOP=HIGH时设置 (REG_RATIO[1]) */
typedef enum {
    SC8815_VBAT_MON_RATIO_12_5x = 0b0,  ///< 12.5x (默认): VBAT max ~25.6V
    SC8815_VBAT_MON_RATIO_5x    = 0b1   ///< 5x: VBAT max ~10.24V (建议 VBAT<10.24V 时使用)
} SC8815_VBAT_MON_RATIO_t;

/** @brief VBUS_RATIO: VBUS 电压设定/监控比率, PSTOP=HIGH时设置 (REG_RATIO[0]) */
typedef enum {
    SC8815_VBUS_RATIO_12_5x = 0b0,  ///< 12.5x (默认): VBUS max ~25.6V
    SC8815_VBUS_RATIO_5x    = 0b1   ///< 5x: VBUS max ~10.24V (建议 VBUS<10.24V 时使用)
} SC8815_VBUS_RATIO_t;

// ============================================================
//  CTRL0_SET 寄存器配置位定义
// ============================================================

/** @brief EN_OTG: OTG 放电模式使能 (REG_CTRL0[7]) */
typedef enum {
    SC8815_EN_OTG_DISABLE = 0b0,  ///< 禁用 OTG 放电模式
    SC8815_EN_OTG_ENABLE  = 0b1   ///< 启用 OTG 放电模式
} SC8815_EN_OTG_t;

/** @brief VINREG_RATIO: VINREG 电压设定/监控比率 (REG_CTRL0[4], 1-bit field) */
typedef enum {
    SC8815_VINREG_RATIO_100x = 0b0,  ///< 100x, 100mV/步, 最大 25.6V (默认, POR=0)
    SC8815_VINREG_RATIO_40x  = 0b1   ///< 40x,  40mV/步, 最大 10.24V (建议 VBUS < 12V 时使用)
} SC8815_VINREG_RATIO_t;

/** @brief FREQ: 开关频率选择 (REG_CTRL0[3:2]) */
typedef enum {
    SC8815_FREQ_150kHz = 0b00,  ///< 150kHz
    SC8815_FREQ_300kHz = 0b01,  ///< 300kHz (默认, 值 01 或 10 均为 300kHz)
    SC8815_FREQ_450kHz = 0b11   ///< 450kHz
} SC8815_FREQ_t;

/** @brief DT: 开关死区时间 (REG_CTRL0[1:0]) */
typedef enum {
    SC8815_DT_20ns = 0b00,  ///< 20ns (默认)
    SC8815_DT_40ns = 0b01,  ///< 40ns
    SC8815_DT_60ns = 0b10,  ///< 60ns
    SC8815_DT_80ns = 0b11   ///< 80ns
} SC8815_DT_t;

// ============================================================
//  CTRL1_SET 寄存器配置位定义
// ============================================================

/** @brief ICHAR_SEL: 充电电流选择 (REG_CTRL1[7]) */
typedef enum {
    SC8815_ICHAR_SEL_IBUS = 0b0,  ///< 充电电流由 IBUS 电流值设定 (默认)
    SC8815_ICHAR_SEL_IBAT = 0b1   ///< 充电电流由 IBAT 电流值设定
} SC8815_ICHAR_SEL_t;

/** @brief DIS_TRICKLE: 禁用涓流充电 (REG_CTRL1[6]) */
typedef enum {
    SC8815_DIS_TRICKLE_ENABLE = 0b0,  ///< 启用涓流充电 (默认)
    SC8815_DIS_TRICKLE_DISABLE = 0b1   ///< 禁用涓流充电
} SC8815_DIS_TRICKLE_t;

/** @brief DIS_TERM: 禁用自动充电终止 (REG_CTRL1[5]) */
typedef enum {
    SC8815_DIS_TERM_ENABLE = 0b0,  ///< 启用自动充电终止 (默认)
    SC8815_DIS_TERM_DISABLE = 0b1   ///< 禁用自动充电终止
} SC8815_DIS_TERM_t;

/** @brief FB_SEL: VBUS 参考电压选择 (REG_CTRL1[4]) */
typedef enum {
    SC8815_FB_SEL_INTERNAL = 0b0,  ///< 使用内部 VBUS 参考电压 (默认)
    SC8815_FB_SEL_EXTERNAL = 0b1   ///< 使用外部 FB 引脚参考电压
} SC8815_FB_SEL_t;

/** @brief TRICKLE_SET: 涓流充电阈值设置 (REG_CTRL1[3]) */
typedef enum {
    SC8815_TRICKLE_SET_70 = 0b0,  ///< VBAT 电压的 70% (默认)
    SC8815_TRICKLE_SET_60 = 0b1   ///< VBAT 电压的 60%
} SC8815_TRICKLE_SET_t;

/** @brief DIS_OVP: 禁用过压保护 (REG_CTRL1[2]) */
typedef enum {
    SC8815_DIS_OVP_ENABLE = 0b0,  ///< 启用过压保护 (默认)
    SC8815_DIS_OVP_DISABLE = 0b1   ///< 禁用过压保护
} SC8815_DIS_OVP_t;

// ============================================================
//  CTRL2_SET 寄存器配置位定义
// ============================================================

/** @brief FACTORY: 工厂配置 (REG_CTRL2[3]) */
typedef enum {
    SC8815_FACTORY_DEFAULT = 0b0,  ///< (默认)
    SC8815_FACTORY_POWERUP = 0b1  ///< 上电后必须写为1
} SC8815_FACTORY_t;

/** @brief EN_DITHER: 扩频抖动使能 (REG_CTRL2[2]) */
typedef enum {
    SC8815_EN_DITHER_DISABLE = 0b0,  ///< 禁用扩频抖动 (默认)
    SC8815_EN_DITHER_ENABLE = 0b1   ///< 启用扩频抖动
} SC8815_EN_DITHER_t;

/** @brief SLEW_SET: VBUS 动态电压变化斜率 (REG_CTRL2[1:0], 仅放电模式) */
typedef enum {
    SC8815_SLEW_SET_1mV_us = 0b00,  ///< 1mV/µs
    SC8815_SLEW_SET_2mV_us = 0b01,  ///< 2mV/µs (默认)
    SC8815_SLEW_SET_4mV_us = 0b10,  ///< 4mV/µs
    SC8815_SLEW_SET_8mV_us = 0b11   ///< 8mV/µs
} SC8815_SLEW_t;


// ============================================================
//  CTRL3_SET 寄存器配置位定义
// ============================================================

/** @brief EN_PGATE: PGATE 引脚控制 (REG_CTRL3[7], 枚举值=寄存器位值) */
typedef enum {
    SC8815_EN_PGATE_OFF = 0b0,  ///< PGATE 输出高电平, 关断外部 PMOS (默认, POR=0)
    SC8815_EN_PGATE_ON  = 0b1   ///< PGATE 输出低电平, 导通外部 PMOS
} SC8815_EN_PGATE_t;

/** @brief GPO_CTRL: GPO 引脚控制 (REG_CTRL3[6], 枚举值=寄存器位值) */
typedef enum {
    SC8815_GPO_CTRL_OD = 0b0,  ///< 开漏输出, 需外部上拉 (默认, POR=0)
    SC8815_GPO_CTRL_LO = 0b1   ///< 内部下拉输出低电平
} SC8815_GPO_CTRL_t;

/** @brief AD_START: AD转换启动 (REG_CTRL3[5]) */
typedef enum {
    SC8815_AD_START_DISABLE = 0b0,  ///< 禁用AD转换 (默认)
    SC8815_AD_START_ENABLE = 0b1   ///< 启用AD转换
} SC8815_AD_START_t;

/** @brief ILIM_BW_SEL: ILIM 环路带宽选择 (REG_CTRL3[4]) */
typedef enum {
    SC8815_ILIM_BW_SEL_5K    = 0b0,  ///< 5kHz 带宽 (默认, POR=0)
    SC8815_ILIM_BW_SEL_1_25K = 0b1   ///< 1.25kHz 带宽
} SC8815_ILIM_BW_SEL_t;

/** @brief LOOP_SET: 环路响应控制 (REG_CTRL3[3]) */
typedef enum {
    SC8815_LOOP_SET_NORMAL = 0b0,  ///< 正常响应 (默认)
    SC8815_LOOP_SET_IMPROVE = 0b1   ///< 改进响应
} SC8815_LOOP_SET_t;

/** @brief DIS_ShortFoldBack: 禁用短路电流折返保护 (REG_CTRL3[2]) */
typedef enum {
    SC8815_DIS_SHORT_FOLDBACK_ENABLE  = 0b0,  ///< 启用短路电流折返保护 (默认)
    SC8815_DIS_SHORT_FOLDBACK_DISABLE = 0b1   ///< 禁用短路电流折返保护
} SC8815_DIS_SHORT_FOLDBACK_t;

/** @brief EOC_SET: 充电终止电流阈值 (REG_CTRL3[1]) */
typedef enum {
    SC8815_EOC_SET_1_25 = 0b0,  ///< 充电电流的 1/25
    SC8815_EOC_SET_1_10 = 0b1   ///< 充电电流的 1/10 (默认)
} SC8815_EOC_t;

/** @brief EN_PFM: 启用 PFM 模式 (REG_CTRL3[0]) */
typedef enum {
    SC8815_EN_PFM_DISABLE = 0b0,  ///< 禁用 PFM 模式 (默认)
    SC8815_EN_PFM_ENABLE = 0b1   ///< 启用 PFM 模式
} SC8815_EN_PFM_t;


// ============================================================
// SC8815 类 — I2C 驱动及全部寄存器操作
// ============================================================

class SC8815 {
public:
    /**
     * @brief 构造函数
     * @param i2c_addr  7-bit I2C 地址, 默认 0x74
     * @param wire      指向 TwoWire 实例的指针, 默认 &Wire
     * @note 不会初始化 I2C 总线, 调用 begin() 前需自行 Wire.begin()
     */
    SC8815(uint8_t i2c_addr = SC8815_I2C_ADDRESS, TwoWire *wire = &Wire);

    ~SC8815();

    // ============================================================
    // 初始化与基础 I/O
    // ============================================================

    /**
     * @brief   检测 I2C 总线上是否存在 SC8815 设备
     * @return  true=设备响应 ACK, false=通信失败
     * @note    不会调用 Wire.begin(), 不会重置 I2C 配置
     * @warning 调用前必须已初始化 Wire (Wire.begin() + Wire.setPins() + Wire.setClock())
     */
    bool begin();

    /**
     * @brief 写单字节到指定寄存器
     * @param reg   寄存器地址 (0x00~0x1B)
     * @param value 写入值
     * @return true=写入成功 (收到 ACK)
     */
    bool writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief 读单字节寄存器
     * @param reg   寄存器地址
     * @param value 输出: 读取值
     * @return true=读取成功
     */
    bool readRegister(uint8_t reg, uint8_t *value);

    // ============================================================
    // 0x00 — VBAT_SET: 电池充电配置
    // ============================================================

    /**
     * @brief   设置 VBAT_SET 寄存器 (电池充电目标电压及相关配置)
     * @param   ircomp  电池内阻补偿值
     * @param   sel     VBAT 设定方式 (内部/外部)
     * @param   csel    电池串联节数 (仅 sel=INTERNAL 时有效)
     * @param   vcell   单节目标电压 (仅 sel=INTERNAL 时有效)
     * @return  true=写入成功
     * @note    在放电模式下此寄存器配置不影响输出, 但仍建议正确设置
     */
    bool setVBATSet(SC8815_IRCOMP_t ircomp, SC8815_VBAT_SEL_t sel,
                    SC8815_CSEL_t csel, SC8815_VCELL_t vcell);

    /**
     * @brief 读取 VBAT_SET 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getVBATSet(uint8_t *reg_value);

    // ============================================================
    // 0x01 & 0x02 — VBUSREF_I: 内部 VBUS 参考电压 (FB_SEL=0)
    // ============================================================

    /**
     * @brief   设置内部 VBUS 参考电压 (10-bit)
     * @param   ref_mV  参考电压 (mV), 范围 2~2048
     * @return  true=写入成功
     * @note    仅在 FB_SEL=0 (内部设定) 时生效
     * @note    公式: VBUSREF_I = (4×VHI + VLO + 1) × 2mV
     * @note    VBUS 输出电压 = VBUSREF_I × VBUS_RATIO
     * @warning 写入前确认 CTRL1_SET 的 FB_SEL=0
     */
    bool setVBUSRefI(uint16_t ref_mV);

    /**
     * @brief   读取内部 VBUS 参考电压
     * @param   ref_mV  输出: 参考电压 (mV)
     * @return  true=读取成功
     */
    bool getVBUSRefI(uint16_t *ref_mV);

    // ============================================================
    // 0x03 & 0x04 — VBUSREF_E: 外部 VBUS 参考电压 (FB_SEL=1)
    // ============================================================

    /**
     * @brief   设置外部 FB 引脚参考电压 (10-bit)
     * @param   ref_mV  参考电压 (mV), 范围 2~2048, 推荐 0.5V~2.048V
     * @return  true=写入成功
     * @note    仅在 FB_SEL=1 (外部电阻设定) 时生效
     * @note    公式: VBUSREF_E = (4×VHI + VLO + 1) × 2mV
     * @note    VBUS 输出电压 = VBUSREF_E × (1 + RUP/RDOWN)
     * @warning 写入前确认 CTRL1_SET 的 FB_SEL=1
     * @warning FB_SEL=1 时仍需配置 VBUS_RATIO (仅影响 ADC 监控量程)
     */
    bool setVBUSRefE(uint16_t ref_mV);

    /**
     * @brief   读取外部 FB 参考电压
     * @param   ref_mV  输出: 参考电压 (mV)
     * @return  true=读取成功
     */
    bool getVBUSRefE(uint16_t *ref_mV);

    // ============================================================
    // 0x05 — IBUS_LIM_SET: IBUS 电流限制
    // ============================================================

    /**
     * @brief   设置 IBUS 电流限制 (充/放电模式均生效)
     * @param   current_mA  目标电流限制 (mA)
     * @return  true=写入成功
     * @note    自动读取当前 IBUS_RATIO 和 RS1=10mΩ 计算寄存器值
     * @note    公式: IBUS_LIM(A) = (IBUS_LIM_SET+1)/256 × IBUS_RATIO × 10mΩ/RS1
     * @warning 实际限流精度取决于采样电阻温度系数
     * @warning 最小值不低于 0.3A (数据手册要求)
     */
    bool setIBUSLimit(uint16_t current_mA);

    /**
     * @brief   读取当前 IBUS 电流限制
     * @param   current_mA  输出: 电流限制 (mA)
     * @return  true=读取成功
     */
    bool getIBUSLimit(uint16_t *current_mA);

    // ============================================================
    // 0x06 — IBAT_LIM_SET: IBAT 电流限制
    // ============================================================

    /**
     * @brief   设置 IBAT 电流限制 (充/放电模式均生效)
     * @param   current_mA  目标电流限制 (mA)
     * @return  true=写入成功
     * @note    自动读取当前 IBAT_RATIO 和 RS2=10mΩ 计算寄存器值
     * @note    公式: IBAT_LIM(A) = (IBAT_LIM_SET+1)/256 × IBAT_RATIO × 10mΩ/RS2
     * @warning 最小值不低于 0.3A (数据手册要求)
     */
    bool setIBATLimit(uint16_t current_mA);

    /**
     * @brief   读取当前 IBAT 电流限制
     * @param   current_mA  输出: 电流限制 (mA)
     * @return  true=读取成功
     */
    bool getIBATLimit(uint16_t *current_mA);

    // ============================================================
    // 0x07 — VINREG_SET: 动态电源管理
    // ============================================================

    /**
     * @brief   设置 VINREG 参考电压 (充电模式动态电源管理)
     * @param   voltage_mV  目标电压 (mV)
     * @return  true=写入成功
     * @note    自动读取 CTRL0 中的 VINREG_RATIO 位确定步进值 (40mV 或 100mV)
     * @note    公式: VINREG = (VINREG_SET+1) × VINREG_RATIO (mV)
     * @note    当 VBUS 电压降至 VINREG 时, 自动减小充电电流以保护适配器
     */
    bool setVINREG(uint16_t voltage_mV);

    /**
     * @brief   读取 VINREG 设定值
     * @param   voltage_mV  输出: VINREG 电压 (mV)
     * @return  true=读取成功
     */
    bool getVINREG(uint16_t *voltage_mV);

    // ============================================================
    // 0x08 — RATIO: 电流/电压比率配置
    // ============================================================

    /**
     * @brief   设置 RATIO 寄存器 (电流/电压比率)
     * @param   ibus_ratio     SC8815_IBUS_RATIO_t: IBUS 电流比率 (3x 或 6x, 00/11 禁止)
     * @param   ibat_ratio     SC8815_IBAT_RATIO_t: IBAT 电流比率 (6x 或 12x)
     * @param   vbus_ratio     SC8815_VBUS_RATIO_t: VBUS 电压比率 (12.5x 或 5x)
     * @param   vbat_mon_ratio SC8815_VBAT_MON_RATIO_t: VBAT 监控比率 (12.5x 或 5x)
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [7:5]
     * @note    VBUS<10.24V 时建议 vbus_ratio=5x (步进 10mV)
     * @note    VBUS≥10.24V 时建议 vbus_ratio=12.5x (步进 25mV)
     * @note    VBAT<9V (1S/2S) 时建议 vbat_mon_ratio=5x
     * @warning IBUS_RATIO 位组合 00 和 11 无效 (数据手册禁止)
     */
    bool setRatio(SC8815_IBUS_RATIO_t ibus_ratio, SC8815_IBAT_RATIO_t ibat_ratio,
                  SC8815_VBUS_RATIO_t vbus_ratio, SC8815_VBAT_MON_RATIO_t vbat_mon_ratio);

    /**
     * @brief 读取 RATIO 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getRatio(uint8_t *reg_value);

    // ============================================================
    // 0x09 — CTRL0_SET: 充/放电模式, 频率, 死区
    // ============================================================

    /**
     * @brief   设置 CTRL0_SET 寄存器
     * @param   en_otg         SC8815_EN_OTG_t: 放电模式使能 (ENABLE=放电, DISABLE=充电)
     * @param   vinreg_ratio   SC8815_VINREG_RATIO_t: VINREG 比率 (100x 或 40x)
     * @param   freq           SC8815_FREQ_t: 开关频率 (150/300/450kHz)
     * @param   dt             SC8815_DT_t: 死区时间 (20/40/60/80ns)
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [6:5]
     * @note    FREQ_SET 值 01 和 10 均为 300kHz
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL0(SC8815_EN_OTG_t en_otg, SC8815_VINREG_RATIO_t vinreg_ratio,
                  SC8815_FREQ_t freq, SC8815_DT_t dt);

    /**
     * @brief 读取 CTRL0_SET 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getCTRL0(uint8_t *reg_value);

    // ============================================================
    // 0x0A — CTRL1_SET: VBUS 设定方式, OVP, 充电选项
    // ============================================================

    /**
     * @brief   设置 CTRL1_SET 寄存器
     * @param   ichar_sel      SC8815_ICHAR_SEL_t: 充电电流基准选择 (IBUS 或 IBAT)
     * @param   dis_trickle    SC8815_DIS_TRICKLE_t: 涓流充电控制 (ENABLE=启用, DISABLE=禁用)
     * @param   dis_term       SC8815_DIS_TERM_t: 自动充电终止控制 (ENABLE=启用, DISABLE=禁用)
     * @param   fb_sel         SC8815_FB_SEL_t: VBUS 电压设定方式 (INTERNAL=I2C, EXTERNAL=FB电阻)
     * @param   trickle_set    SC8815_TRICKLE_SET_t: 涓流充电阈值 (70% 或 60% VBAT)
     * @param   dis_ovp        SC8815_DIS_OVP_t: OVP 保护控制 (ENABLE=启用, DISABLE=禁用)
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [1:0]
     * @note    放电模式通常设置: fb_sel=EXTERNAL, dis_ovp=ENABLE
     * @note    TRICKLE_SET: 0=70%VBAT 阈值, 1=60%VBAT 阈值 (数据手册)
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL1(SC8815_ICHAR_SEL_t ichar_sel, SC8815_DIS_TRICKLE_t dis_trickle, SC8815_DIS_TERM_t dis_term,
                  SC8815_FB_SEL_t fb_sel, SC8815_TRICKLE_SET_t trickle_set_60, SC8815_DIS_OVP_t dis_ovp);

    /**
     * @brief 读取 CTRL1_SET 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getCTRL1(uint8_t *reg_value);

    // ============================================================
    // 0x0B — CTRL2_SET: 频率抖动, 电压斜率
    // ============================================================

    /**
     * @brief   设置 CTRL2_SET 寄存器
     * @param   en_dither   SC8815_EN_DITHER_t: 频率抖动使能 (ENABLE=使能, 改善EMI)
     * @param   slew        SC8815_SLEW_t: VBUS 动态电压变化斜率
     * @param   factory_bit SC8815_FACTORY_t: 工厂配置位 (POWERUP=1, 数据手册强制要求)
     * @return  true=写入成功
     * @note    数据手册明确要求: MCU 上电后应将 FACTORY bit 置 1
     * @note    EN_DITHER=1 时 PGATE 引脚功能禁用, 改为频率抖动控制
     * @note    频率抖动范围约 ±5%, 抖动周期由 PGATE/DITHER 引脚电容决定
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL2(SC8815_EN_DITHER_t en_dither, SC8815_SLEW_t slew, SC8815_FACTORY_t factory_bit = SC8815_FACTORY_POWERUP);

    /**
     * @brief 读取 CTRL2_SET 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getCTRL2(uint8_t *reg_value);

    // ============================================================
    // 0x0C — CTRL3_SET: ADC, PFM, 保护, 环路
    // ============================================================

    /**
     * @brief   设置 CTRL3_SET 寄存器
     * @param   en_pgate            SC8815_EN_PGATE_t: PGATE 引脚控制
     * @param   gpo_ctrl            SC8815_GPO_CTRL_t: GPO 引脚控制
     * @param   ad_start            SC8815_AD_START_t: ADC 转换控制 (ENABLE=启动)
     * @param   ilim_bw             SC8815_ILIM_BW_SEL_t: 电流限制环路带宽 (5K=5kHz, 1_25K=1.25kHz)
     * @param   loop_set            SC8815_LOOP_SET_t: 环路响应控制
     * @param   dis_short_foldback  SC8815_DIS_SHORT_FOLDBACK_t: 短路电流折返保护
     * @param   eoc_set             SC8815_EOC_t: 充电终止电流阈值
     * @param   en_pfm              SC8815_EN_PFM_t: PFM 模式控制
     * @return  true=写入成功
     * @note    读 ADC 之前必须设置 ad_start=ENABLE
     * @note    放电模式低纹波应用建议 en_pfm=DISABLE (强制 PWM)
     * @note    短路折返: 启用时 VBUS<1V 电流折返至 22%(IBUS)/10%(IBAT)
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL3(SC8815_EN_PGATE_t en_pgate, SC8815_GPO_CTRL_t gpo_ctrl, SC8815_AD_START_t ad_start,
                  SC8815_ILIM_BW_SEL_t ilim_bw, SC8815_LOOP_SET_t loop_set,
                  SC8815_DIS_SHORT_FOLDBACK_t dis_short_foldback, SC8815_EOC_t eoc_set, SC8815_EN_PFM_t en_pfm);

    /**
     * @brief 读取 CTRL3_SET 寄存器原始值
     * @param reg_value 输出: 寄存器值
     * @return true=读取成功
     */
    bool getCTRL3(uint8_t *reg_value);

    // ============================================================
    // 0x17 — STATUS: 中断状态
    // ============================================================

    /** @brief 读取 STATUS 寄存器原始值 */
    bool getStatus(uint8_t *status);

    /** @brief 检测适配器是否插入 (ACIN 引脚 >3V) */
    bool isACOK();

    /** @brief 检测是否检测到负载插入 (INDET 引脚) */
    bool isINDET();

    /** @brief 检测 VBUS 是否短路 (VBUS < 1V) */
    bool isVBUSShort();

    /** @brief 检测是否发生过温保护 (Tj > 165°C) */
    bool isOTP();

    /** @brief 检测充电是否终止 (EOC) */
    bool isEOC();

    // ============================================================
    // ADC 读取 — 返回实际物理量
    // ============================================================

    /**
     * @brief   读取 VBUS 电压 (通过内部 ADC)
     * @param   voltage_mV  输出: VBUS 电压 (mV)
     * @return  true=读取成功
     * @note    公式: VBUS = (4×VHI + VLO + 1) × VBUS_RATIO × 2mV
     * @note    自动读取 RATIO 寄存器确定当前 VBUS_RATIO
     * @warning 需要先设置 AD_START=1
     * @warning VBUS_RATIO=12.5x 时最大可测 ~25.6V, 超出饱和
     */
    bool readVBUSVoltage(uint16_t *voltage_mV);

    /**
     * @brief   读取 VBAT 电压 (通过内部 ADC)
     * @param   voltage_mV  输出: VBAT 电压 (mV)
     * @return  true=读取成功
     * @note    公式: VBAT = (4×VHI + VLO + 1) × VBAT_MON_RATIO × 2mV
     * @note    自动读取 RATIO 寄存器确定当前 VBAT_MON_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readVBATVoltage(uint16_t *voltage_mV);

    /**
     * @brief   读取 IBUS 电流 (通过内部 ADC)
     * @param   current_mA  输出: IBUS 电流 (mA)
     * @return  true=读取成功
     * @note    公式: IBUS = raw × 2mV × IBUS_RATIO × 10mΩ / (1200 × RS1)
     * @note    默认 RS1=10mΩ, 自动读取 IBUS_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readIBUSCurrent(uint16_t *current_mA);

    /**
     * @brief   读取 IBAT 电流 (通过内部 ADC)
     * @param   current_mA  输出: IBAT 电流 (mA)
     * @return  true=读取成功
     * @note    公式: IBAT = raw × 2mV × IBAT_RATIO × 10mΩ / (1200 × RS2)
     * @note    默认 RS2=10mΩ, 自动读取 IBAT_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readIBATCurrent(uint16_t *current_mA);

    /**
     * @brief   读取 ADIN 引脚电压 (通过内部 ADC)
     * @param   voltage_mV  输出: ADIN 电压 (mV), 范围 0~2048
     * @return  true=读取成功
     * @note    分辨率 2mV/步, 最大输入 2.048V
     * @warning 需要先设置 AD_START=1
     */
    bool readADINVoltage(uint16_t *voltage_mV);

    // ============================================================
    // 0x19 — MASK: 中断掩码
    // ============================================================

    /**
     * @brief   设置中断掩码
     * @param   ac_ok_mask       true=屏蔽 AC_OK 中断
     * @param   indet_mask       true=屏蔽 INDET 中断
     * @param   vbus_short_mask  true=屏蔽 VBUS_SHORT 中断
     * @param   otp_mask         true=屏蔽 OTP 中断
     * @param   eoc_mask         true=屏蔽 EOC 中断
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits 7,4,0
     */
    bool setMask(bool ac_ok_mask, bool indet_mask, bool vbus_short_mask,
                 bool otp_mask, bool eoc_mask);

    /** @brief 读取 MASK 寄存器原始值 */
    bool getMask(uint8_t *mask);

    // ============================================================
    // 便捷高级函数 — 封装常用操作序列
    // ============================================================

    /**
     * @brief   便捷设置充电目标电压 (内部设定方式)
     * @param   cell_count       电池串联节数 (1~4)
     * @param   vcell_per_cell   单节目标电压
     * @return  true=全部写入成功
     * @note    自动设置 VBAT_SEL=INTERNAL, IRCOMP=0
     */
    bool setChargeVoltage(uint8_t cell_count, SC8815_VCELL_t vcell_per_cell);

    /**
     * @brief   便捷设置充电电流限制
     * @param   ibus_limit_mA  IBUS 电流限制 (mA)
     * @param   ibat_limit_mA  IBAT 电流限制 (mA)
     * @return  true=全部写入成功
     */
    bool setChargeCurrent(uint16_t ibus_limit_mA, uint16_t ibat_limit_mA);

    /**
     * @brief   切换到充电模式 (EN_OTG=0)
     * @return  true=写入成功
     * @note    read-modify-write CTRL0[7], 不影响其他位
     */
    bool enableChargingMode();

    /**
     * @brief   切换到放电模式并设置 VBUS 目标电压 (内部 FB 方式)
     * @param   vbus_target_mV  VBUS 目标电压 (mV)
     * @return  true=全部写入成功
     * @note    自动设置 FB_SEL=0 (内部参考), EN_OTG=1
     * @note    VBUS_RATIO 根据电压自动选择 (10.24V 阈值为 5x/12.5x)
     * @warning 此函数强制使用内部 FB 方式, 不适合外部 FB 电阻分压应用
     */
    bool enableDischargingMode(uint16_t vbus_target_mV);

private:
    uint8_t _i2c_addr;
    TwoWire *_wire;

    // --- 底层 I2C 操作 ---
    bool writeByte(uint8_t reg, uint8_t data);
    bool readByte(uint8_t reg, uint8_t *data);
};

#endif // SC8815_H
