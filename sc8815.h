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

//#define SC8815_FAKE_DATA
//#define SC8815_DEBUG

#include <Arduino.h>
#include <Wire.h>

#include "sc8815_reg.h"

// I2C 地址
constexpr uint8_t SC8815_I2C_ADDRESS = 0x74;   ///< 7-bit I2C 地址
constexpr uint8_t SENSE_R = 10; // 检流电阻 mOhm

// SC8815 类 — I2C 驱动及全部寄存器操作

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

    // 初始化与基础 I/O

    /**
     * @brief   检测 I2C 总线上是否存在 SC8815 设备
     * @return  true=设备响应 ACK, false=通信失败
     * @note    不会调用 Wire.begin(), 不会重置 I2C 配置
     * @warning 调用前必须已初始化 Wire (Wire.begin() + Wire.setPins() + Wire.setClock())
     */
    bool begin();

    /**
     * @brief 读单字节寄存器
     * @param reg   寄存器地址
     * @param value 输出: 读取值
     * @return true=读取成功
     */
    bool readRegister(SC8815_REG reg, uint8_t &value);

    // 0x00 — VBAT_SET: 电池充电配置

    /**
     * @brief   设置 VBAT_SET 寄存器 (电池充电目标电压及相关配置)
     * @param   config  VBAT_SET 配置结构体, 包含 ircomp/sel/csel/vcell 字段
     * @return  true=写入成功
     * @note    在放电模式下此寄存器配置不影响输出, 但仍建议正确设置
     */
    bool setVBATSet(SC8815_VBATSet_Config_t config);

    // 0x01 & 0x02 — VBUSREF_I: 内部 VBUS 参考电压 (FB_SEL=0)

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

    // 0x03 & 0x04 — VBUSREF_E: 外部 VBUS 参考电压 (FB_SEL=1)

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

    // 0x05 — IBUS_LIM_SET: IBUS 电流限制

    /**
     * @brief   设置 IBUS 电流限制 (充/放电模式均生效)
     * @param   current_A  目标电流限制 (A)
     * @return  true=写入成功
     * @note    自动读取当前 IBUS_RATIO 和 RS1=10mΩ 计算寄存器值
     * @note    公式: IBUS_LIM(A) = (IBUS_LIM_SET+1)/256 × IBUS_RATIO × 10mΩ/RS1
     * @warning 实际限流精度取决于采样电阻温度系数
     * @warning 最小值不低于 0.3A (数据手册要求)
     */
    bool setIBUSLimit(double current_A);

    // 0x06 — IBAT_LIM_SET: IBAT 电流限制

    /**
     * @brief   设置 IBAT 电流限制 (充/放电模式均生效)
     * @param   current_A  目标电流限制 (A)
     * @return  true=写入成功
     * @note    自动读取当前 IBAT_RATIO 和 RS2=10mΩ 计算寄存器值
     * @note    公式: IBAT_LIM(A) = (IBAT_LIM_SET+1)/256 × IBAT_RATIO × 10mΩ/RS2
     * @warning 最小值不低于 0.3A (数据手册要求)
     */
    bool setIBATLimit(double current_A);

    // 0x07 — VINREG_SET: 动态电源管理

    /**
     * @brief   设置 VINREG 参考电压 (充电模式动态电源管理)
     * @param   voltage_mV  目标电压 (mV)
     * @return  true=写入成功
     * @note    自动读取 CTRL0 中的 VINREG_RATIO 位确定步进值 (40mV 或 100mV)
     * @note    公式: VINREG = (VINREG_SET+1) × VINREG_RATIO (mV)
     * @note    当 VBUS 电压降至 VINREG 时, 自动减小充电电流以保护适配器
     */
    bool setVINREG(uint16_t voltage_mV);

    // 0x08 — RATIO: 电流/电压比率配置

    /**
     * @brief   设置 RATIO 寄存器 (电流/电压比率)
     * @param   config  RATIO 配置结构体, 包含 ibus_ratio/ibat_ratio/vbus_ratio/vbat_mon_ratio 字段
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [7:5]
     * @note    VBUS_RATIO: VBUS<10.24V 时建议 5x (步进 10mV); VBUS≥10.24V 时建议 12.5x (步进 25mV)
     * @note    VBAT_MON_RATIO: VBAT<9V (1S/2S) 时建议 5x
     * @warning IBUS_RATIO 位组合 00 和 11 无效 (数据手册禁止)
     */
    bool setRatio(SC8815_Ratio_Config_t config);

    // 0x09 — CTRL0_SET: 充/放电模式, 频率, 死区

    /**
     * @brief   设置 CTRL0_SET 寄存器
     * @param   config  CTRL0 配置结构体, 包含 en_otg/vinreg_ratio/freq/dt 字段
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [6:5]
     * @note    FREQ_SET 值 01 和 10 均为 300kHz
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL0(SC8815_CTRL0_Config_t config);

    // 0x0A — CTRL1_SET: VBUS 设定方式, OVP, 充电选项

    /**
     * @brief   设置 CTRL1_SET 寄存器
     * @param   config  CTRL1 配置结构体, 包含 ichar_sel/dis_trickle/dis_term/fb_sel/trickle_set/dis_ovp 字段
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [1:0]
     * @note    放电模式通常设置: fb_sel=EXTERNAL, dis_ovp=ENABLE
     * @note    TRICKLE_SET: 0=70%VBAT 阈值, 1=60%VBAT 阈值 (数据手册)
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL1(SC8815_CTRL1_Config_t config);

    // 0x0B — CTRL2_SET: 频率抖动, 电压斜率

    /**
     * @brief   设置 CTRL2_SET 寄存器
     * @param   config  CTRL2 配置结构体, 包含 en_dither/slew/factory_bit 字段
     * @return  true=写入成功
     * @note    数据手册明确要求: MCU 上电后应将 FACTORY bit 置 1
     * @note    EN_DITHER=1 时 PGATE 引脚功能禁用, 改为频率抖动控制
     * @note    频率抖动范围约 ±5%, 抖动周期由 PGATE/DITHER 引脚电容决定
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL2(SC8815_CTRL2_Config_t config);

    // 0x0C — CTRL3_SET: ADC, PFM, 保护, 环路

    /**
     * @brief   设置 CTRL3_SET 寄存器
     * @param   config  CTRL3 配置结构体, 包含 en_pgate/gpo_ctrl/ad_start/ilim_bw/loop_set/
     *                  dis_short_foldback/eoc_set/en_pfm 字段
     * @return  true=写入成功
     * @note    读 ADC 之前必须设置 ad_start=ENABLE
     * @note    放电模式低纹波应用建议 en_pfm=DISABLE (强制 PWM)
     * @note    短路折返: 启用时 VBUS<1V 电流折返至 22%(IBUS)/10%(IBAT)
     * @warning 仅在 PSTOP=H (待机) 时修改此寄存器
     */
    bool setCTRL3(SC8815_CTRL3_Config_t config);

    // ADC 读取 — 返回实际物理量

    /**
     * @brief   读取 VBUS 电压 (通过内部 ADC)
     * @param   voltage_mV  输出: VBUS 电压 (mV)
     * @return  true=读取成功
     * @note    公式: VBUS = (4×VHI + VLO + 1) × VBUS_RATIO × 2mV
     * @note    自动读取 RATIO 寄存器确定当前 VBUS_RATIO
     * @warning 需要先设置 AD_START=1
     * @warning VBUS_RATIO=12.5x 时最大可测 ~25.6V, 超出饱和
     */
    bool readVBUSVoltage(uint16_t &voltage_mV);

    /**
     * @brief   读取 VBAT 电压 (通过内部 ADC)
     * @param   voltage_mV  输出: VBAT 电压 (mV)
     * @return  true=读取成功
     * @note    公式: VBAT = (4×VHI + VLO + 1) × VBAT_MON_RATIO × 2mV
     * @note    自动读取 RATIO 寄存器确定当前 VBAT_MON_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readVBATVoltage(uint16_t &voltage_mV);

    /**
     * @brief   读取 IBUS 电流 (通过内部 ADC)
     * @param   current_A  输出: IBUS 电流 (A)
     * @return  true=读取成功
     * @note    公式: IBUS = raw × 2mV × IBUS_RATIO × 10mΩ / (1200 × RS1)
     * @note    默认 RS1=10mΩ, 自动读取 IBUS_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readIBUSCurrent(double &current_A);

    /**
     * @brief   读取 IBAT 电流 (通过内部 ADC)
     * @param   current_A  输出: IBAT 电流 (A)
     * @return  true=读取成功
     * @note    公式: IBAT = raw × 2mV × IBAT_RATIO × 10mΩ / (1200 × RS2)
     * @note    默认 RS2=10mΩ, 自动读取 IBAT_RATIO
     * @warning 需要先设置 AD_START=1
     */
    bool readIBATCurrent(double &current_A);

    /**
     * @brief   读取 ADIN 引脚电压 (通过内部 ADC)
     * @param   voltage_mV  输出: ADIN 电压 (mV), 范围 0~2048
     * @return  true=读取成功
     * @note    分辨率 2mV/步, 最大输入 2.048V
     * @warning 需要先设置 AD_START=1
     */
    bool readADINVoltage(uint16_t &voltage_mV);

    // 0x17 — STATUS: 中断状态

    /** @brief 读取 STATUS 寄存器原始值 */
    bool getStatus(SC8815_Status_t &status);


    // 0x19 — MASK: 中断掩码

    /**
     * @brief   设置中断掩码
     * @param   config  IRQ 掩码配置结构体, 包含 ac_ok_mask/indet_mask/vbus_short_mask/
     *                  otp_mask/eoc_mask 字段
     * @return  true=写入成功
     * @note    采用 read-modify-write 保护 reserved bits [7],[4],[0]
     */
    bool setIRQMask(SC8815_IRQMASK_Config_t config);

    // 快捷函数

    /**
     * @brief   设置芯片工作状态
     * @param   mode  输出模式, 充电或者放电
     * @return  true=设置成功
     */
    bool setPowerMode(SC8815_EN_OTG_t mode);

    /**
     * @brief   设置芯片PGATE
     * @param   mode  PGATE电平
     * @return  true=设置成功
     */
    bool setPGATE(SC8815_EN_PGATE_t mode);

    /**
     * @brief   设置芯片PGATE
     * @param   mode  GPO电平, GPO为开漏输出
     * @return  true=设置成功
     */
    bool setGPO(SC8815_GPO_CTRL_t mode);

private:
    uint8_t _i2c_addr;
    TwoWire *_wire;

    // 寄存器缓存
    uint8_t _cache_ratio;   // RATIO (0x08)
    uint8_t _cache_ctrl0;   // CTRL0_SET (0x09)
    uint8_t _cache_ctrl1;   // CTRL1_SET (0x0A)
    uint8_t _cache_ctrl2;   // CTRL2_SET (0x0B)
    uint8_t _cache_ctrl3;   // CTRL3_SET (0x0C)
    uint8_t _cache_mask;    // MASK (0x19)

    // 底层 I2C 操作
    bool writeByte(SC8815_REG reg, const uint8_t data);
    bool readByte(SC8815_REG reg, uint8_t &data);
};

#endif // SC8815_H
