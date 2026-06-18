/**
 * @file    sc8815.cpp
 * @brief   SC8815 库实现 — 全部寄存器 I2C 操作
 */

#include "sc8815.h"

#ifdef SC8815_DEBUG
static void SC8815_printBin(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        Serial.print((val >> i) & 1);
        if (i == 4) Serial.print(' ');
    }
}
#endif

SC8815::SC8815(uint8_t i2c_addr, TwoWire *wire)
    : _i2c_addr(i2c_addr), _wire(wire),
      _cache_ratio(0x38),   // POR: 0011_1000 (bit5/bit4=1, IBUS_RATIO=10=3x, others 0)
      _cache_ctrl0(0x04),   // POR: FREQ=01 (300kHz)
      _cache_ctrl1(0x01),   // POR: bit0=1 (reserved)
      _cache_ctrl2(0x01),   // POR: SLEW=01 (2mV/µs)
      _cache_ctrl3(0x02),   // POR: EOC=1, others 0
      _cache_mask(0x80)     // POR: bit7=1 (reserved)
{
}

SC8815::~SC8815()
{
}

// 初始化

bool SC8815::begin()
{
    // 仅检测设备是否存在 (发送地址 + 检查 ACK)
    // 不调用 _wire->begin() — 由应用层负责 Wire 初始化
    _wire->beginTransmission(_i2c_addr);
    uint8_t error = _wire->endTransmission();
    return (error == 0);
}

// 底层 I2C 操作

bool SC8815::writeByte(SC8815_REG reg, uint8_t data)
{
#ifdef SC8815_DEBUG

    Serial.print(F("[HAL_SC8815] set  reg at 0x"));
    if ((uint8_t)reg < 0x10) Serial.print('0');
    Serial.print((uint8_t)reg, HEX);
    Serial.print(F(" <- data: "));
    SC8815_printBin(data);
    Serial.print(F(" [0x"));
    if (data < 0x10) Serial.print('0');
    Serial.print(data, HEX);
    Serial.println(F("]"));

#endif
#ifndef SC8815_FAKE_DATA
    _wire->beginTransmission(_i2c_addr);
    _wire->write((uint8_t)reg);
    _wire->write(data);
    uint8_t error = _wire->endTransmission();
    return (error == 0);

#else
    return true;
#endif
}

bool SC8815::readByte(SC8815_REG reg, uint8_t &data)
{
#ifndef SC8815_FAKE_DATA
    _wire->beginTransmission(_i2c_addr);
    _wire->write((uint8_t)reg);
    if (_wire->endTransmission(false) != 0) return false;
    if (_wire->requestFrom(_i2c_addr, (uint8_t)1) != 1) return false;
    data = _wire->read();

#else
    data = 0x55;
#endif

#ifdef SC8815_DEBUG

    Serial.print(F("[HAL_SC8815] read reg at 0x"));
    if ((uint8_t)reg < 0x10) Serial.print('0');
    Serial.print((uint8_t)reg, HEX);
    Serial.print(F(" -> data: "));
    SC8815_printBin(data);
    Serial.print(F(" [0x"));
    if (data < 0x10) Serial.print('0');
    Serial.print(data, HEX);
    Serial.println(F("]"));
#endif

    return true;
}

// 公开的寄存器读接口

bool SC8815::readRegister(SC8815_REG reg, uint8_t &value)
{
    return readByte(reg, value);
}

// 0x00 — VBAT_SET

bool SC8815::setVBATSet(SC8815_VBATSet_Config_t config)
{
    // 寄存器位布局 (Table 1):
    //   [7:6] IRCOMP    (00=0mΩ, 01=20mΩ, 10=40mΩ, 11=80mΩ)
    //   [5]   VBAT_SEL  (0=内部设定, 1=外部设定)
    //   [4:3] CSEL      (00=1S, 01=2S, 10=3S, 11=4S)
    //   [2:0] VCELL_SET (000=4.10V ... 111=4.50V)
    uint8_t reg = ((uint8_t)config.ircomp  << 6) |
                  ((uint8_t)config.sel     << 5) |
                  ((uint8_t)config.csel    << 3) |
                  ((uint8_t)config.vcell   );
    return writeByte(SC8815_REG::VBAT_SET, reg);
}

// 0x01 & 0x02 — VBUSREF_I (内部 VBUS 参考, FB_SEL=0)

bool SC8815::setVBUSRefI(uint16_t ref_mV)
{
    // 数据手册公式 (Table 2):
    //   VBUSREF_I = (4 × VHI + VLO + 1) × 2mV
    //   VBUS = VBUSREF_I × VBUS_RATIO
    //
    // 即:
    //    分辨率为 2mV, 位深为10-bit, 对应 VBUSREF_I=2mV~2048mV
    //    将目标值除以 2 后减 1 即得到寄存器编码值, 再分解为 VHI (高 8 位) 和 VLO (低 2 位)

    // 范围: 2mV ~ 2048mV (VHI=0~255, VLO=0~3)
    if (ref_mV < 2)   ref_mV = 2;
    if (ref_mV > 2048) ref_mV = 2048;

    uint16_t temp = (ref_mV / 2) - 1;
    uint8_t  vhi  = temp >> 2;
    uint8_t  vlo  = temp & 0b11;

    // 写 VHI 到 REG 0x01
    if (!writeByte(SC8815_REG::VBUSREF_I_SET, vhi)) return false;

    // 读 VBUSREF_I_SET2, 保留 bit[5:0] (保留位), 替换 bit[7:6]=VLO
    uint8_t reg2;
    if (!readByte(SC8815_REG::VBUSREF_I_SET2, reg2)) return false;

    reg2 = (reg2 & 0x3F) |
           (vlo << 6);
    return writeByte(SC8815_REG::VBUSREF_I_SET2, reg2);
}

// 0x03 & 0x04 — VBUSREF_E (外部 FB 参考, FB_SEL=1)

bool SC8815::setVBUSRefE(uint16_t ref_mV)
{
    // 数据手册公式 (Table 4):
    //   VBUSREF_E = (4 × VHI + VLO + 1) × 2mV
    //   VBUS = VBUSREF_E × (1 + RUP/RDOWN)
    //
    // 与 setVBUSRefI 使用相同的编码算法
    // 推荐范围: 0.5V ~ 2.048V (寄存器可低至 2mV)

    if (ref_mV < 2)   ref_mV = 2;
    if (ref_mV > 2048) ref_mV = 2048;

    uint16_t temp = (ref_mV / 2) - 1;
    uint8_t  vhi  = temp >> 2;
    uint8_t  vlo  = temp & 0b11;

    if (!writeByte(SC8815_REG::VBUSREF_E_SET, vhi)) return false;

    // 读-改-写: 保留 bit[5:0] (保留位), 仅更新 bit[7:6]
    uint8_t reg2;
    if (!readByte(SC8815_REG::VBUSREF_E_SET2, reg2)) return false;

    reg2 = (reg2 & 0x3F) |
           (vlo << 6);
    return writeByte(SC8815_REG::VBUSREF_E_SET2, reg2);
}

// 0x05 — IBUS_LIM_SET

bool SC8815::setIBUSLimit(double current_A)
{
    // 数据手册公式 (Table 6):
    //   IBUS_LIM(A) = (IBUS_LIM_SET+1)/256 × IBUS_RATIO × 10mΩ/RS1
    //
    // 从缓存获取 IBUS_RATIO
    SC8815_IBUS_RATIO_t ibus_ratio =
        (SC8815_IBUS_RATIO_t)((_cache_ratio >> 2) & 0b11);

    // 公式反解:
    //   reg_val = I_LIM × 256 × RS1 / (IBUS_RATIO × 10) - 1
    float ratio_val = (ibus_ratio == SC8815_IBUS_RATIO_3x) ? 3.0f : 6.0f;
    float reg_float = (current_A * 256.0f * SENSE_R) / (ratio_val * 10.0f) - 1.0f;

    if (reg_float < 0)   reg_float = 0;
    if (reg_float > 255) reg_float = 255;

    uint8_t reg = (uint8_t)(reg_float + 0.5f);  // 四舍五入
    return writeByte(SC8815_REG::IBUS_LIM_SET, reg);
}

// 0x06 — IBAT_LIM_SET

bool SC8815::setIBATLimit(double current_A)
{
    // 数据手册公式 (Table 7):
    //   IBAT_LIM(A) = (IBAT_LIM_SET+1)/256 × IBAT_RATIO × 10mΩ/RS2
    SC8815_IBAT_RATIO_t ibat_ratio =
        (SC8815_IBAT_RATIO_t)((_cache_ratio >> 4) & 0b1);

    float ratio_val = (ibat_ratio == SC8815_IBAT_RATIO_6x) ? 6.0f : 12.0f;
    float reg_float = (current_A * 256.0f * SENSE_R) / (ratio_val * 10.0f) - 1.0f;

    if (reg_float < 0)   reg_float = 0;
    if (reg_float > 255) reg_float = 255;

    uint8_t reg_val = (uint8_t)(reg_float + 0.5f); // 四舍五入
    return writeByte(SC8815_REG::IBAT_LIM_SET, reg_val);
}

// 0x07 — VINREG_SET

bool SC8815::setVINREG(uint16_t voltage_mV)
{
    // 数据手册公式 (Table 8):
    //   VINREG = (VINREG_SET + 1) × VINREG_RATIO (mV)
    //   VINREG_RATIO = 40x  → 40mV/步,  最大 10.24V
    //   VINREG_RATIO = 100x → 100mV/步, 最大 25.6V
    //
    // 自动读取 CTRL0[4] 确定当前 VINREG_RATIO 配置

    // 从缓存获取 VINREG_RATIO 位 (bit4: 0=100x, 1=40x)
    SC8815_VINREG_RATIO_t vinreg_ratio = (SC8815_VINREG_RATIO_t)((_cache_ctrl0 >> 4) & 0b1);

    uint16_t step = (vinreg_ratio == SC8815_VINREG_RATIO_40x) ? 40 : 100;
    if (voltage_mV < step) voltage_mV = step;  // 最小值为一个 step
    uint16_t reg_val = (voltage_mV / step) - 1;
    if (reg_val > 255) reg_val = 255;
    return writeByte(SC8815_REG::VINREG_SET, (uint8_t)reg_val);
}

// 0x08 — RATIO

bool SC8815::setRatio(SC8815_Ratio_Config_t config)
{
    // 寄存器位布局 (Table 9):
    //   [7:6] Reserved (保持原值)
    //   [5]   Reserved (保持原值, POR=1)
    //   [4]   IBAT_RATIO  (0=6x, 1=12x)
    //   [3:2] IBUS_RATIO  (01=6x, 10=3x; 00/11=禁止)
    //   [1]   VBAT_MON_RATIO (0=12.5x, 1=5x)
    //   [0]   VBUS_RATIO  (0=12.5x, 1=5x)
    //
    // 采用 read-modify-write 保护 reserved bits [7:5]

    // 使用缓存保留 reserved bits [7:5]
    uint8_t reg = _cache_ratio & 0xE0;  // 保留 bit[7:5], 清除 bit[4:0]

    // 写入新的配置位
    reg |= ((uint8_t)config.ibat_ratio      << 4);          // bit4
    reg |= ((uint8_t)config.ibus_ratio      << 2);          // bit3-2
    reg |= ((uint8_t)config.vbat_mon_ratio  << 1);      // bit1
    reg |= (uint8_t)config.vbus_ratio;                 // bit0

    if (!writeByte(SC8815_REG::RATIO, reg)) return false;
    _cache_ratio = reg;
    return true;
}

// 0x09 — CTRL0_SET

bool SC8815::setCTRL0(SC8815_CTRL0_Config_t config)
{
    // 寄存器位布局 (Table 10):
    //   [7]   EN_OTG        (0=充电模式, 1=放电模式)
    //   [6:5] Reserved      (保持原值, 禁止覆写)
    //   [4]   VINREG_RATIO  (0=100x, 1=40x)
    //   [3:2] FREQ_SET      (00=150kHz, 01=300kHz, 10=300kHz, 11=450kHz)
    //   [1:0] DT_SET        (00=20ns, 01=40ns, 10=60ns, 11=80ns)
    //
    // 采用 read-modify-write 保护 reserved bits [6:5]

    uint8_t reg = _cache_ctrl0 & 0x60;  // 保留 bit[6:5] (Reserved)

    // 设置可配置位
    reg |= ((uint8_t)config.en_otg         << 7)|      // bit7
           ((uint8_t)config.vinreg_ratio   << 4)|      // bit4
           ((uint8_t)config.freq           << 2)|      // bit3-2
           ((uint8_t)config.dt                 );      // bit1-0

    if (!writeByte(SC8815_REG::CTRL0_SET, reg)) return false;
    _cache_ctrl0 = reg;
    return true;
}

// 0x0A — CTRL1_SET

bool SC8815::setCTRL1(SC8815_CTRL1_Config_t config)
{
    // 寄存器位布局 (Table 11):
    //   [7]   ICHAR_SEL     (0=IBUS 电流基准, 1=IBAT 电流基准)
    //   [6]   DIS_TRICKLE   (0=启用涓流充电, 1=禁用涓流充电)
    //   [5]   DIS_TERM      (0=启用自动终止, 1=禁用自动终止)
    //   [4]   FB_SEL        (0=内部 I2C 设定 VBUS, 1=外部 FB 电阻设定)
    //   [3]   TRICKLE_SET   (0=70% VBAT 阈值, 1=60% VBAT 阈值)
    //   [2]   DIS_OVP       (0=启用 OVP 保护, 1=禁用 OVP 保护)
    //   [1:0] Reserved      (保持原值, bit0 POR=1)
    //
    // 采用 read-modify-write 保护 reserved bits [1:0]

    uint8_t reg = _cache_ctrl1 & 0x03;  // 保留 bit[1:0] (Reserved)

    // 设置可配置位
    reg |= ((uint8_t)config.ichar_sel      << 7)|      // bit7
           ((uint8_t)config.dis_trickle    << 6)|      // bit6
           ((uint8_t)config.dis_term       << 5)|      // bit5
           ((uint8_t)config.fb_sel         << 4)|      // bit4
           ((uint8_t)config.trickle_set    << 3)|      // bit3
           ((uint8_t)config.dis_ovp        << 2);      // bit2

    if (!writeByte(SC8815_REG::CTRL1_SET, reg)) return false;
    _cache_ctrl1 = reg;
    return true;
}

// 0x0B — CTRL2_SET

bool SC8815::setCTRL2(SC8815_CTRL2_Config_t config)
{
    // 寄存器位布局 (Table 12):
    //   [7:4] Reserved  (保持原值, 禁止覆写, POR=0000)
    //   [3]   FACTORY   (0=默认, 1=上电后 MCU 必须置 1)
    //   [2]   EN_DITHER (0=禁用频率抖动/PGATE 正常功能, 1=使能频率抖动)
    //   [1:0] SLEW_SET  (00=1mV/µs, 01=2mV/µs, 10=4mV/µs, 11=8mV/µs)
    //
    // 采用 read-modify-write 保护 reserved bits [7:4]

    uint8_t reg = _cache_ctrl2 & 0xF0;  // 保留 bit[7:4] (Reserved)

    // 设置可配置位
    reg |= ((uint8_t)config.factory_bit << 3)|      // bit3: FACTORY (默认=1)
           ((uint8_t)config.en_dither   << 2)|      // bit2: EN_DITHER
           ((uint8_t)config.slew            );      // bit1-0: SLEW_SET

    if (!writeByte(SC8815_REG::CTRL2_SET, reg)) return false;
    _cache_ctrl2 = reg;
    return true;
}

// 0x0C — CTRL3_SET

bool SC8815::setCTRL3(SC8815_CTRL3_Config_t config)
{
    // 寄存器位布局 (Table 13):
    //   [7] EN_PGATE           (0=PGATE输出高电平关断PMOS, 1=PGATE输出低电平导通PMOS)
    //   [6] GPO_CTRL           (0=开漏输出需外部上拉, 1=内部下拉输出低电平)
    //   [5] AD_START           (0=停止ADC, 1=启动ADC)
    //   [4] ILIM_BW_SEL        (0=5kHz带宽, 1=1.25kHz带宽)
    //   [3] LOOP_SET           (0=正常环路响应, 1=改善环路响应)
    //   [2] DIS_ShortFoldBack  (0=启用短路电流折返, 1=禁用短路电流折返)
    //   [1] EOC_SET            (0=充电电流1/25终止, 1=充电电流1/10终止)
    //   [0] EN_PFM             (0=强制PWM模式, 1=使能PFM模式)
    //
    // 该寄存器无 reserved bits, 全部 8 位均可直接写入
    uint8_t reg = 0;
    reg |= ((uint8_t)config.en_pgate            << 7)|      // bit7
           ((uint8_t)config.gpo_ctrl            << 6)|      // bit6
           ((uint8_t)config.ad_start            << 5)|      // bit5
           ((uint8_t)config.ilim_bw             << 4)|      // bit4
           ((uint8_t)config.loop_set            << 3)|      // bit3
           ((uint8_t)config.dis_short_foldback  << 2)|      // bit2
           ((uint8_t)config.eoc_set             << 1)|      // bit1
           ((uint8_t)config.en_pfm              << 0);      // bit0
    if (!writeByte(SC8815_REG::CTRL3_SET, reg)) return false;
    _cache_ctrl3 = reg;
    return true;
}

// ADC 读取

bool SC8815::readVBUSVoltage(uint16_t &voltage_mV)
{
    // 数据手册公式 (Table 14/15):
    //   VBUS = (4 × VBUS_FB_VALUE + VBUS_FB_VALUE2 + 1) × VBUS_RATIO × 2mV
    //
    // VBUS_RATIO: 12.5x → 25mV/步, 5x → 10mV/步
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG::VBUS_FB_VALUE,  vhi))     return false;
    if (!readByte(SC8815_REG::VBUS_FB_VALUE2, vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    // 从缓存获取 VBUS_RATIO
    SC8815_VBUS_RATIO_t ratio = (SC8815_VBUS_RATIO_t)(_cache_ratio & 0x01);

    uint16_t raw = (vhi << 2) + vlo + 1;
    if (ratio == SC8815_VBUS_RATIO_12_5x) {
        voltage_mV = raw * 25;  // 2mV × 12.5 = 25mV
    } else {
        voltage_mV = raw * 10;  // 2mV × 5 = 10mV
    }
    return true;
}

bool SC8815::readVBATVoltage(uint16_t &voltage_mV)
{
    // 数据手册公式 (Table 16/17):
    //   VBAT = (4 × VBAT_FB_VALUE + VBAT_FB_VALUE2 + 1) × VBAT_MON_RATIO × 2mV
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG::VBAT_FB_VALUE,  vhi))     return false;
    if (!readByte(SC8815_REG::VBAT_FB_VALUE2, vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    bool mon_5x = (_cache_ratio & 0x02) != 0;  // 从缓存获取 bit1 (VBAT_MON_RATIO)

    uint16_t raw  = (vhi << 2) + vlo + 1;
    uint16_t step = mon_5x ? 10 : 25;  // 2mV×5=10mV, 2mV×12.5=25mV
    voltage_mV = raw * step;
    return true;
}

bool SC8815::readIBUSCurrent(double &current_A)
{
    // 数据手册公式 (Table 18/19):
    //   IBUS(A) = (4 × IBUS_VALUE + IBUS_VALUE2 + 1) × 2mV × IBUS_RATIO × 10mΩ / (1200 × RS1)
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG::IBUS_VALUE,  vhi))     return false;
    if (!readByte(SC8815_REG::IBUS_VALUE2, vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    SC8815_IBUS_RATIO_t ibus_ratio =
        (SC8815_IBUS_RATIO_t)((_cache_ratio >> 2) & 0x03);

    uint16_t raw       = (vhi << 2) + vlo + 1;
    uint16_t ratio_val = (ibus_ratio == SC8815_IBUS_RATIO_3x) ? 3 : 6;

    // IBUS(A) = raw × (2/1200) × ratio_val × 10/RS1
    current_A = raw * (2.0f / 1200.0f) * ratio_val * 10.0f / SENSE_R;
    return true;
}

bool SC8815::readIBATCurrent(double &current_A)
{
    // 数据手册公式 (Table 20/21):
    //   IBAT(A) = (4 × IBAT_VALUE + IBAT_VALUE2 + 1) × 2mV × IBAT_RATIO × 10mΩ / (1200 × RS2)
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG::IBAT_VALUE,  vhi))     return false;
    if (!readByte(SC8815_REG::IBAT_VALUE2, vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    SC8815_IBAT_RATIO_t ibat_ratio =
        (SC8815_IBAT_RATIO_t)((_cache_ratio >> 4) & 0x01);

    uint16_t raw       = (vhi << 2) + vlo + 1;
    uint16_t ratio_val = (ibat_ratio == SC8815_IBAT_RATIO_6x) ? 6 : 12;
    current_A = raw * (2.0f / 1200.0f) * ratio_val * 10.0f / SENSE_R;
    return true;
}

bool SC8815::readADINVoltage(uint16_t &voltage_mV)
{
    // 数据手册公式 (Table 22/23):
    //   ADIN = (4 × ADIN_VALUE + ADIN_VALUE2 + 1) × 2mV
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG::ADIN_VALUE,  vhi))     return false;
    if (!readByte(SC8815_REG::ADIN_VALUE2, vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    uint16_t raw = (vhi << 2) + vlo + 1;
    voltage_mV = raw * 2;  // 2mV/步
    return true;
}

// 0x17 — STATUS (只读)

bool SC8815::getStatus(SC8815_Status_t &status)
{
    uint8_t reg;
    if (!readByte(SC8815_REG::STATUS, reg)) return false;
    status.ACOK      = (reg & 0x40) != 0;
    status.INDET     = (reg & 0x20) != 0;
    status.VBUSShort = (reg & 0x08) != 0;
    status.OTP       = (reg & 0x04) != 0;
    status.EOC       = (reg & 0x02) != 0;
    return true;
}

// 0x19 — MASK

bool SC8815::setIRQMask(SC8815_IRQMASK_Config_t config)
{
    // 寄存器位布局:
    //   [7] Reserved  (保持原值, POR=1)
    //   [6] AC_OK_Mask
    //   [5] INDET_Mask
    //   [4] Reserved  (保持原值)
    //   [3] VBUS_SHORT_Mask
    //   [2] OTP_Mask
    //   [1] EOC_Mask
    //   [0] Reserved  (保持原值, POR=0, 数据手册: "Write this bit to 1 after power up")
    //
    // 采用 read-modify-write 保护 reserved bits [7],[4]
    // bit[0] 强制写 1 (数据手册要求)

    uint8_t reg = _cache_mask & 0x90;  // 保留 bit[7],[4] (Reserved)

    reg |= ((config.ac_ok_mask      ? 1 : 0) << 6)|  // bit6
           ((config.indet_mask      ? 1 : 0) << 5)|  // bit5
           ((config.vbus_short_mask ? 1 : 0) << 3)|  // bit3
           ((config.otp_mask        ? 1 : 0) << 2)|  // bit2
           ((config.eoc_mask        ? 1 : 0) << 1)|  // bit1
             1;                                      // bit0

    if (!writeByte(SC8815_REG::MASK, reg)) return false;
    _cache_mask = reg;
    return true;
}

// 辅助函数

bool SC8815::setPowerMode(SC8815_EN_OTG_t mode)
{
    // 寄存器位布局 (Table 10):
    //   [7]   EN_OTG        (0=充电模式, 1=放电模式)
    //   [6:5] Reserved      (保持原值, 禁止覆写)
    //   [4]   VINREG_RATIO  (0=100x, 1=40x)
    //   [3:2] FREQ_SET      (00=150kHz, 01=300kHz, 10=300kHz, 11=450kHz)
    //   [1:0] DT_SET        (00=20ns, 01=40ns, 10=60ns, 11=80ns)

    uint8_t reg = (_cache_ctrl0 & 0xEF) | ((uint8_t)mode << 7);  // 保留 bit[6:0]
    if (!writeByte(SC8815_REG::CTRL0_SET, reg)) return false;
    _cache_ctrl0 = reg;
    return true;
}

bool SC8815::setPGATE(SC8815_EN_PGATE_t mode)
{
    // 寄存器位布局 (Table 13):
    //   [7] EN_PGATE           (0=PGATE输出高电平关断PMOS, 1=PGATE输出低电平导通PMOS)
    //   [6] GPO_CTRL           (0=开漏输出需外部上拉, 1=内部下拉输出低电平)
    //   [5] AD_START           (0=停止ADC, 1=启动ADC)
    //   [4] ILIM_BW_SEL        (0=5kHz带宽, 1=1.25kHz带宽)
    //   [3] LOOP_SET           (0=正常环路响应, 1=改善环路响应)
    //   [2] DIS_ShortFoldBack  (0=启用短路电流折返, 1=禁用短路电流折返)
    //   [1] EOC_SET            (0=充电电流1/25终止, 1=充电电流1/10终止)
    //   [0] EN_PFM             (0=强制PWM模式, 1=使能PFM模式)

    uint8_t reg = (_cache_ctrl3 & 0x7F) | ((uint8_t)mode << 7);  // 保留 bit[6:0]
    if (!writeByte(SC8815_REG::CTRL3_SET, reg)) return false;
    _cache_ctrl3 = reg;
    return true;
}

bool SC8815::setGPO(SC8815_GPO_CTRL_t mode)
{
    // 寄存器同上

    uint8_t reg = (_cache_ctrl3 & 0xBF) | ((uint8_t)mode << 6);  // 保留 [7] [5:0]
    if (!writeByte(SC8815_REG::CTRL3_SET, reg)) return false;
    _cache_ctrl3 = reg;
    return true;
}
