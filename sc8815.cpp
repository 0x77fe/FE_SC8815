/**
 * @file    sc8815.cpp
 * @brief   SC8815 库实现 — 全部寄存器 I2C 操作
 *
 * 修复记录 (与原版对比):
 *   - setCTRL2(): 添加 FACTORY bit 参数 (默认=1), 满足数据手册强制要求
 *   - setCTRL1(): 参数 trickle_set_70 重命名为 trickle_set_60 (更准确)
 *   - setCTRL0/setCTRL1/setRatio/setMask: 采用 read-modify-write 保护 reserved bits
 *   - begin(): 移除 Wire.begin() 调用, 避免重置应用层 I2C 配置
 *   - 全部函数添加详细行为注释与寄存器公式引用
 */

#include "sc8815.h"

// ============================================================
// 构造 / 析构
// ============================================================

SC8815::SC8815(uint8_t i2c_addr, TwoWire *wire)
    : _i2c_addr(i2c_addr), _wire(wire)
{
    // 仅保存参数, 不执行 I2C 操作
}

SC8815::~SC8815()
{
}

// ============================================================
// 初始化
// ============================================================

bool SC8815::begin()
{
    // 仅检测设备是否存在 (发送地址 + 检查 ACK)
    // 不调用 _wire->begin() — 由应用层负责 Wire 初始化
    _wire->beginTransmission(_i2c_addr);
    uint8_t error = _wire->endTransmission();
    return (error == 0);
}

// ============================================================
// 底层 I2C 操作
// ============================================================

bool SC8815::writeByte(uint8_t reg, uint8_t data)
{
    _wire->beginTransmission(_i2c_addr);
    _wire->write(reg);
    _wire->write(data);
    uint8_t error = _wire->endTransmission();
    return (error == 0);
}

bool SC8815::readByte(uint8_t reg, uint8_t *data)
{
    _wire->beginTransmission(_i2c_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return false;
    if (_wire->requestFrom(_i2c_addr, (uint8_t)1) != 1) return false;
    *data = _wire->read();
    return true;
}

// --- 公开的寄存器读写接口 ---

bool SC8815::writeRegister(uint8_t reg, uint8_t value)
{
    return writeByte(reg, value);
}

bool SC8815::readRegister(uint8_t reg, uint8_t *value)
{
    return readByte(reg, value);
}

// ============================================================
// 0x00 — VBAT_SET
// ============================================================

bool SC8815::setVBATSet(SC8815_IRCOMP_t ircomp, 
                        SC8815_VBAT_SEL_t sel,
                        SC8815_CSEL_t csel, 
                        SC8815_VCELL_t vcell)
{
    // 寄存器位布局 (Table 1):
    //   [7:6] IRCOMP    (00=0mΩ, 01=20mΩ, 10=40mΩ, 11=80mΩ)
    //   [5]   VBAT_SEL  (0=内部设定, 1=外部设定)
    //   [4:3] CSEL      (00=1S, 01=2S, 10=3S, 11=4S)
    //   [2:0] VCELL_SET (000=4.10V ... 111=4.50V)
uint8_t reg = (((uint8_t)ircomp & 0b11) << 6) |
              (((uint8_t)sel    & 0b1 ) << 5) |
              (((uint8_t)csel   & 0b11) << 3) |
              ((uint8_t)vcell   & 0b111);
return writeByte(SC8815_REG_VBAT_SET, reg);
}

bool SC8815::getVBATSet(uint8_t *reg_value)
{
    return readByte(SC8815_REG_VBAT_SET, reg_value);
}

// ============================================================
// 0x01 & 0x02 — VBUSREF_I (内部 VBUS 参考, FB_SEL=0)
// ============================================================

bool SC8815::setVBUSRefI(uint16_t ref_mV)
{
    // 数据手册公式 (Table 2):
    //   VBUSREF_I = (4 × VHI + VLO + 1) × 2mV
    //   VBUS = VBUSREF_I × VBUS_RATIO
    //
    // 即:
    //    分辨率为 2mv, 位深为10-bit, 对应 VBUSREF_I=2mV~2048mV   
    //    将目标值除以 2 后减 1 即得到寄存器编码值, 再分解为 VHI (高 8 位) 和 VLO (低 2 位)

    // 范围: 2mV ~ 2048mV (VHI=0~255, VLO=0~3)
    if (ref_mV < 2)   ref_mV = 2;
    if (ref_mV > 2048) ref_mV = 2048;

    uint16_t temp = (ref_mV / 2) - 1;
    uint8_t  vhi  = temp >> 2;
    uint8_t  vlo  = temp & 0b11;

    // 写 VHI 到 REG 0x01
    if (!writeByte(SC8815_REG_VBUSREF_I_SET, vhi)) return false;

    // 读 VBUSREF_I_SET2, 保留 bit[5:0] (保留位), 替换 bit[7:6]=VLO
    uint8_t reg2;
    if (!readByte(SC8815_REG_VBUSREF_I_SET2, &reg2)) return false;

    reg2 = (reg2 & 0x3F) |
           (vlo << 6);  
    return writeByte(SC8815_REG_VBUSREF_I_SET2, reg2);
}

bool SC8815::getVBUSRefI(uint16_t *ref_mV)
{
    // 读入VBUSREF_I_SET1(高8位)和VBUSREF_I_SET2(低2位), 拼接得到10bit量化值, 再乘以分辨率得到 mV

    uint8_t vhi, reg2;
    if (!readByte(SC8815_REG_VBUSREF_I_SET, &vhi))  return false;
    if (!readByte(SC8815_REG_VBUSREF_I_SET2, &reg2)) return false;
    uint16_t temp = (vhi << 2) | ((reg2 >> 6) & 0b11);
    *ref_mV = (temp + 1) * 2;
    return true;
}

// ============================================================
// 0x03 & 0x04 — VBUSREF_E (外部 FB 参考, FB_SEL=1)
// ============================================================

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

    if (!writeByte(SC8815_REG_VBUSREF_E_SET, vhi)) return false;

    // 读-改-写: 保留 bit[5:0] (保留位), 仅更新 bit[7:6]
    uint8_t reg2;
    if (!readByte(SC8815_REG_VBUSREF_E_SET2, &reg2)) return false;

    reg2 = (reg2 & 0x3F) |
           (vlo << 6);
    return writeByte(SC8815_REG_VBUSREF_E_SET2, reg2);
}

bool SC8815::getVBUSRefE(uint16_t *ref_mV)
{
    // 读入VBUSREF_E_SET1(高8位)和VBUSREF_E_SET2(低2位), 拼接得到10bit量化值, 再乘以分辨率得到 mV

    uint8_t vhi, reg2;
    if (!readByte(SC8815_REG_VBUSREF_E_SET, &vhi))  return false;
    if (!readByte(SC8815_REG_VBUSREF_E_SET2, &reg2)) return false;
    uint16_t temp = (vhi << 2) | ((reg2 >> 6) & 0b11);
    *ref_mV = (temp + 1) * 2;
    return true;
}

// ============================================================
// 0x05 — IBUS_LIM_SET
// ============================================================

bool SC8815::setIBUSLimit(uint16_t current_mA)
{
    // 数据手册公式 (Table 6):
    //   IBUS_LIM(A) = (IBUS_LIM_SET+1)/256 × IBUS_RATIO × 10mΩ/RS1
    //
    // 先读取当前 RATIO 配置确定 IBUS_RATIO
    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBUS_RATIO_t ibus_ratio =
        (SC8815_IBUS_RATIO_t)((ratio_reg >> 2) & 0b11);

    // 公式反解:
    //   reg_val = I_LIM × 256 × RS1 / (IBUS_RATIO × 10) - 1
    float ratio_val = (ibus_ratio == SC8815_IBUS_RATIO_3x) ? 3.0f : 6.0f;
    float lim_A     = current_mA / 1000.0f;
    float reg_float = (lim_A * 256.0f * 10) / (ratio_val * 10.0f) - 1.0f;

    if (reg_float < 0)   reg_float = 0;
    if (reg_float > 255) reg_float = 255;

    uint8_t reg = (uint8_t)(reg_float + 0.5f);  // 四舍五入
    return writeByte(SC8815_REG_IBUS_LIM_SET, reg);
}

bool SC8815::getIBUSLimit(uint16_t *current_mA)
{
    // 先读取 IBUS_LIM_SET 寄存器值, 再根据当前 RATIO 配置计算实际电流限制

    uint8_t reg;
    if (!readByte(SC8815_REG_IBUS_LIM_SET, &reg)) return false;

    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBUS_RATIO_t ibus_ratio =
        (SC8815_IBUS_RATIO_t)((ratio_reg >> 2) & 0b11);

    // 正向公式:
    //   I_LIM = (reg_val+1)/256 × IBUS_RATIO × 10mΩ/RS1
    float ratio_val = (ibus_ratio == SC8815_IBUS_RATIO_3x) ? 3.0f : 6.0f;
    float lim_A = ((reg + 1) / 256.0f) * ratio_val * 10.0f / 10.0f;
    *current_mA = (uint16_t)(lim_A * 1000.0f + 0.5f);
    return true;
}

// ============================================================
// 0x06 — IBAT_LIM_SET
// ============================================================

bool SC8815::setIBATLimit(uint16_t current_mA)
{
    // 数据手册公式 (Table 7):
    //   IBAT_LIM(A) = (IBAT_LIM_SET+1)/256 × IBAT_RATIO × 10mΩ/RS2
    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBAT_RATIO_t ibat_ratio =
        (SC8815_IBAT_RATIO_t)((ratio_reg >> 4) & 0x01);

    float ratio_val = (ibat_ratio == SC8815_IBAT_RATIO_6x) ? 6.0f : 12.0f;
    float lim_A     = current_mA / 1000.0f;
    float reg_float = (lim_A * 256.0f * 10.0f) / (ratio_val * 10.0f) - 1.0f;

    if (reg_float < 0)   reg_float = 0;
    if (reg_float > 255) reg_float = 255;

    uint8_t reg_val = (uint8_t)(reg_float + 0.5f); // 四舍五入
    return writeByte(SC8815_REG_IBAT_LIM_SET, reg_val);
}

bool SC8815::getIBATLimit(uint16_t *current_mA)
{
    // 先读取 IBAT_LIM_SET 寄存器值, 再根据当前 RATIO 配置计算实际电流限制

    uint8_t reg_val;
    if (!readByte(SC8815_REG_IBAT_LIM_SET, &reg_val)) return false;

    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBAT_RATIO_t ibat_ratio =
        (SC8815_IBAT_RATIO_t)((ratio_reg >> 4) & 0x01);

    float ratio_val = (ibat_ratio == SC8815_IBAT_RATIO_6x) ? 6.0f : 12.0f;
    float lim_A = ((reg_val + 1) / 256.0f) * ratio_val * 10.0f / 10.0f;
    *current_mA = (uint16_t)(lim_A * 1000.0f + 0.5f);
    return true;
}

// ============================================================
// 0x07 — VINREG_SET
// ============================================================

bool SC8815::setVINREG(uint16_t voltage_mV)
{
    // 数据手册公式 (Table 8):
    //   VINREG = (VINREG_SET + 1) × VINREG_RATIO (mV)
    //   VINREG_RATIO = 40x  → 40mV/步,  最大 10.24V
    //   VINREG_RATIO = 100x → 100mV/步, 最大 25.6V
    //
    // 自动读取 CTRL0[4] 确定当前 VINREG_RATIO 配置

    // 读取 CTRL0 中的 VINREG_RATIO 位 (bit4: 0=100x, 1=40x)
    uint8_t ctrl0;
    if (!readByte(SC8815_REG_CTRL0_SET, &ctrl0)) return false;
    SC8815_VINREG_RATIO_t vinreg_ratio = (SC8815_VINREG_RATIO_t)((ctrl0 >> 4) & 0x01);

    uint16_t step = (vinreg_ratio == SC8815_VINREG_RATIO_40x) ? 40 : 100;
    uint16_t reg_val = (voltage_mV / step) - 1;
    if (reg_val > 255) reg_val = 255;
    return writeByte(SC8815_REG_VINREG_SET, reg_val);
}

bool SC8815::getVINREG(uint16_t *voltage_mV)
{
    uint8_t reg_val;
    if (!readByte(SC8815_REG_VINREG_SET, &reg_val)) return false;

    // 读取 CTRL0 中的 VINREG_RATIO 位 (bit4: 0=100x, 1=40x)
    uint8_t ctrl0;
    if (!readByte(SC8815_REG_CTRL0_SET, &ctrl0)) return false;
    SC8815_VINREG_RATIO_t vinreg_ratio = (SC8815_VINREG_RATIO_t)((ctrl0 >> 4) & 0x01);

    uint16_t step = (vinreg_ratio == SC8815_VINREG_RATIO_40x) ? 40 : 100;
    *voltage_mV = (uint16_t)(reg_val + 1) * step;
    return true;
}

// ============================================================
// 0x08 — RATIO
// ============================================================

bool SC8815::setRatio(SC8815_IBUS_RATIO_t ibus_ratio, SC8815_IBAT_RATIO_t ibat_ratio,
                      SC8815_VBUS_RATIO_t vbus_ratio, SC8815_VBAT_MON_RATIO_t vbat_mon_ratio)
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

    // 先读取当前值以保留 reserved bits
    uint8_t reg;
    if (!readByte(SC8815_REG_RATIO, &reg)) {
        // 读取失败时回退: 仅保留 reserved bits (bit5=1), 配置位由后续 OR 填充
        reg = 0x20;  // bit5=1 (POR 保留位), bits 7-6=0, bits 4-0 清零待 OR 写入
    } else {
        reg &= 0xE0;  // 保留 bit[7:5], 清除 bit[4:0]
    }

    // 写入新的配置位
    reg |= ((uint8_t)ibat_ratio << 4);          // bit4
    reg |= ((uint8_t)ibus_ratio << 2);          // bit3-2
    reg |= ((uint8_t)vbat_mon_ratio << 1);      // bit1
    reg |= (uint8_t)vbus_ratio;                 // bit0

    return writeByte(SC8815_REG_RATIO, reg);
}

bool SC8815::getRatio(uint8_t *reg_value)
{
    return readByte(SC8815_REG_RATIO, reg_value);
}

// ============================================================
// 0x09 — CTRL0_SET
// ============================================================

bool SC8815::setCTRL0(SC8815_EN_OTG_t en_otg, SC8815_VINREG_RATIO_t vinreg_ratio,
                      SC8815_FREQ_t freq, SC8815_DT_t dt)
{
    // 寄存器位布局 (Table 10):
    //   [7]   EN_OTG        (0=充电模式, 1=放电模式)
    //   [6:5] Reserved      (保持原值, 禁止覆写)
    //   [4]   VINREG_RATIO  (0=100x, 1=40x)
    //   [3:2] FREQ_SET      (00=150kHz, 01=300kHz, 10=300kHz, 11=450kHz)
    //   [1:0] DT_SET        (00=20ns, 01=40ns, 10=60ns, 11=80ns)
    //
    // 采用 read-modify-write 保护 reserved bits [6:5]

    uint8_t reg;
    if (!readByte(SC8815_REG_CTRL0_SET, &reg)) {
        reg = 0x04;  // POR 默认值: FREQ=01(300kHz), Reserved 均为 0
    }
    // 保留 bit[6:5] (Reserved), 清除其余待配置位
    reg &= 0x60;  // 0b01100000

    // 设置可配置位
    reg |= ((uint8_t)en_otg << 7);  // bit7
    reg |= ((uint8_t)vinreg_ratio << 4);     // bit4
    reg |= ((uint8_t)freq & 0x03)     << 2;   // bit3-2
    reg |= ((uint8_t)dt   & 0x03);            // bit1-0

    return writeByte(SC8815_REG_CTRL0_SET, reg);
}

bool SC8815::getCTRL0(uint8_t *reg_value)
{
    return readByte(SC8815_REG_CTRL0_SET, reg_value);
}

// ============================================================
// 0x0A — CTRL1_SET
// ============================================================

bool SC8815::setCTRL1(SC8815_ICHAR_SEL_t ichar_sel, SC8815_DIS_TRICKLE_t dis_trickle, SC8815_DIS_TERM_t dis_term,
                      SC8815_FB_SEL_t fb_sel, SC8815_TRICKLE_SET_t trickle_set_60, SC8815_DIS_OVP_t dis_ovp)
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

    uint8_t reg;
    if (!readByte(SC8815_REG_CTRL1_SET, &reg)) {
        reg = 0x01;  // 读取失败回退: POR 默认 bit0=1
    }
    // 保留 bit[1:0], 清除其余待配置位
    reg &= 0x03;  // 0b00000011

    // 设置可配置位
    reg |= ((uint8_t)ichar_sel    << 7);  // bit7
    reg |= ((uint8_t)dis_trickle  << 6);  // bit6
    reg |= ((uint8_t)dis_term     << 5);  // bit5
    reg |= ((uint8_t)fb_sel       << 4);  // bit4
    // trickle_set_60: true→60% (bit=1), false→70% (bit=0)
    reg |= ((uint8_t)trickle_set_60 << 3);  // bit3
    reg |= ((uint8_t)dis_ovp      << 2);  // bit2

    return writeByte(SC8815_REG_CTRL1_SET, reg);
}

bool SC8815::getCTRL1(uint8_t *reg_value)
{
    return readByte(SC8815_REG_CTRL1_SET, reg_value);
}

// ============================================================
// 0x0B — CTRL2_SET
// ============================================================

bool SC8815::setCTRL2(SC8815_EN_DITHER_t en_dither, SC8815_SLEW_t slew, SC8815_FACTORY_t factory_bit = SC8815_FACTORY_POWERUP)
{
    // 寄存器位布局 (Table 12):
    //   [7:4] Reserved  (保持原值, 禁止覆写, POR=0000)
    //   [3]   FACTORY   (0=默认, 1=上电后 MCU 必须置 1)
    //   [2]   EN_DITHER (0=禁用频率抖动/PGATE 正常功能, 1=使能频率抖动)
    //   [1:0] SLEW_SET  (00=1mV/µs, 01=2mV/µs, 10=4mV/µs, 11=8mV/µs)
    //
    // 采用 read-modify-write 保护 reserved bits [7:4]

    uint8_t reg;
    if (!readByte(SC8815_REG_CTRL2_SET, &reg)) {
        reg = 0x01;  // POR 默认值: SLEW=01(2mV/µs), FACTORY=0, Reserved[7:4]=0
    }
    // 保留 bit[7:4] (Reserved), 清除 bit[3:0]
    reg &= 0xF0;

    // 设置可配置位
    reg |= ((uint8_t)factory_bit << 3);  // bit3: FACTORY (默认=1)
    reg |= ((uint8_t)en_dither << 2);  // bit2: EN_DITHER
    reg |= ((uint8_t)slew & 0x03);       // bit1-0: SLEW_SET

    return writeByte(SC8815_REG_CTRL2_SET, reg);
}

bool SC8815::getCTRL2(uint8_t *reg_value)
{
    return readByte(SC8815_REG_CTRL2_SET, reg_value);
}

// ============================================================
// 0x0C — CTRL3_SET
// ============================================================

bool SC8815::setCTRL3(SC8815_EN_PGATE_t en_pgate, SC8815_GPO_CTRL_t gpo_ctrl, SC8815_AD_START_t ad_start,
                      SC8815_ILIM_BW_SEL_t ilim_bw, SC8815_LOOP_SET_t loop_set,
                      SC8815_DIS_SHORT_FOLDBACK_t dis_short_foldback, SC8815_EOC_t eoc_set, SC8815_EN_PFM_t en_pfm)
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
    reg |= ((uint8_t)en_pgate            << 7);
    reg |= ((uint8_t)gpo_ctrl            << 6);
    reg |= ((uint8_t)ad_start            << 5);
    reg |= ((uint8_t)ilim_bw             << 4);
    reg |= ((uint8_t)loop_set            << 3);
    reg |= ((uint8_t)dis_short_foldback  << 2);
    reg |= ((uint8_t)eoc_set & 0x01)     << 1;
    reg |= ((uint8_t)en_pfm              << 0);
    return writeByte(SC8815_REG_CTRL3_SET, reg);
}

bool SC8815::getCTRL3(uint8_t *reg_value)
{
    return readByte(SC8815_REG_CTRL3_SET, reg_value);
}

// ============================================================
// 0x17 — STATUS (只读)
// ============================================================

bool SC8815::getStatus(uint8_t *status)
{
    return readByte(SC8815_REG_STATUS, status);
}

bool SC8815::isACOK()
{
    uint8_t status;
    if (!readByte(SC8815_REG_STATUS, &status)) return false;
    return (status & 0x40) != 0;  // bit6
}

bool SC8815::isINDET()
{
    uint8_t status;
    if (!readByte(SC8815_REG_STATUS, &status)) return false;
    return (status & 0x20) != 0;  // bit5
}

bool SC8815::isVBUSShort()
{
    uint8_t status;
    if (!readByte(SC8815_REG_STATUS, &status)) return false;
    return (status & 0x08) != 0;  // bit3
}

bool SC8815::isOTP()
{
    uint8_t status;
    if (!readByte(SC8815_REG_STATUS, &status)) return false;
    return (status & 0x04) != 0;  // bit2
}

bool SC8815::isEOC()
{
    uint8_t status;
    if (!readByte(SC8815_REG_STATUS, &status)) return false;
    return (status & 0x02) != 0;  // bit1
}

// ============================================================
// ADC 读取
// ============================================================

bool SC8815::readVBUSVoltage(uint16_t *voltage_mV)
{
    // 数据手册公式 (Table 14/15):
    //   VBUS = (4 × VBUS_FB_VALUE + VBUS_FB_VALUE2 + 1) × VBUS_RATIO × 2mV
    //
    // VBUS_RATIO: 12.5x → 25mV/步, 5x → 10mV/步
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG_VBUS_FB_VALUE,  &vhi))     return false;
    if (!readByte(SC8815_REG_VBUS_FB_VALUE2, &vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    // 读取 VBUS_RATIO
    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_VBUS_RATIO_t ratio = (SC8815_VBUS_RATIO_t)(ratio_reg & 0x01);

    uint16_t raw = (4 * vhi + vlo + 1);
    if (ratio == SC8815_VBUS_RATIO_12_5x) {
        *voltage_mV = raw * 25;  // 2mV × 12.5 = 25mV
    } else {
        *voltage_mV = raw * 10;  // 2mV × 5 = 10mV
    }
    return true;
}

bool SC8815::readVBATVoltage(uint16_t *voltage_mV)
{
    // 数据手册公式 (Table 16/17):
    //   VBAT = (4 × VBAT_FB_VALUE + VBAT_FB_VALUE2 + 1) × VBAT_MON_RATIO × 2mV
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG_VBAT_FB_VALUE,  &vhi))     return false;
    if (!readByte(SC8815_REG_VBAT_FB_VALUE2, &vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    bool mon_5x = (ratio_reg & 0x02) != 0;  // bit1

    uint16_t raw  = (4 * vhi + vlo + 1);
    uint16_t step = mon_5x ? 10 : 25;  // 2mV×5=10mV, 2mV×12.5=25mV
    *voltage_mV = raw * step;
    return true;
}

bool SC8815::readIBUSCurrent(uint16_t *current_mA)
{
    // 数据手册公式 (Table 18/19):
    //   IBUS(A) = (4 × IBUS_VALUE + IBUS_VALUE2 + 1) × 2mV × IBUS_RATIO × 10mΩ / (1200 × RS1)
    //   当 RS1=10mΩ 时简化为: raw × IBUS_RATIO / 600
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG_IBUS_VALUE,  &vhi))     return false;
    if (!readByte(SC8815_REG_IBUS_VALUE2, &vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBUS_RATIO_t ibus_ratio =
        (SC8815_IBUS_RATIO_t)((ratio_reg >> 2) & 0x03);

    uint16_t raw       = (4 * vhi + vlo + 1);
    uint16_t ratio_val = (ibus_ratio == SC8815_IBUS_RATIO_3x) ? 3 : 6;
    // IBUS(A) = raw × (2/1200) × ratio_val × 10/RS1
    // 当 RS1=10mΩ: IBUS(A) = raw × ratio_val / 600
    float current_A = raw * (2.0f / 1200.0f) * ratio_val * 10.0f / 10.0f;
    *current_mA = (uint16_t)(current_A * 1000.0f + 0.5f);
    return true;
}

bool SC8815::readIBATCurrent(uint16_t *current_mA)
{
    // 数据手册公式 (Table 20/21):
    //   IBAT(A) = (4 × IBAT_VALUE + IBAT_VALUE2 + 1) × 2mV × IBAT_RATIO × 10mΩ / (1200 × RS2)
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG_IBAT_VALUE,  &vhi))     return false;
    if (!readByte(SC8815_REG_IBAT_VALUE2, &vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_IBAT_RATIO_t ibat_ratio =
        (SC8815_IBAT_RATIO_t)((ratio_reg >> 4) & 0x01);

    uint16_t raw       = (4 * vhi + vlo + 1);
    uint16_t ratio_val = (ibat_ratio == SC8815_IBAT_RATIO_6x) ? 6 : 12;
    float current_A = raw * (2.0f / 1200.0f) * ratio_val * 10.0f / 10.0f;
    *current_mA = (uint16_t)(current_A * 1000.0f + 0.5f);
    return true;
}

bool SC8815::readADINVoltage(uint16_t *voltage_mV)
{
    // 数据手册公式 (Table 22/23):
    //   ADIN = (4 × ADIN_VALUE + ADIN_VALUE2 + 1) × 2mV
    uint8_t vhi, vlo_reg;
    if (!readByte(SC8815_REG_ADIN_VALUE,  &vhi))     return false;
    if (!readByte(SC8815_REG_ADIN_VALUE2, &vlo_reg)) return false;
    uint8_t vlo = (vlo_reg >> 6) & 0x03;  // 数据手册: VALUE2 有效位为 [7:6]

    uint16_t raw = (4 * vhi + vlo + 1);
    *voltage_mV = raw * 2;  // 2mV/步
    return true;
}

// ============================================================
// 0x19 — MASK
// ============================================================

bool SC8815::setMask(bool ac_ok_mask, bool indet_mask, bool vbus_short_mask,
                     bool otp_mask, bool eoc_mask)
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

    uint8_t reg;
    if (!readByte(SC8815_REG_MASK, &reg)) {
        reg = 0x80;  // 读取失败回退: bit7=1 (POR)
    }
    // 保留 bit[7],[4], 清除可配置位
    reg &= 0x90;  // 0b10010000

    reg |= (ac_ok_mask      ? 1 : 0) << 6;  // bit6
    reg |= (indet_mask      ? 1 : 0) << 5;  // bit5
    reg |= (vbus_short_mask ? 1 : 0) << 3;  // bit3
    reg |= (otp_mask        ? 1 : 0) << 2;  // bit2
    reg |= (eoc_mask        ? 1 : 0) << 1;  // bit1
    reg |= 1;                                // bit0: 数据手册强制要求写 1

    return writeByte(SC8815_REG_MASK, reg);
}

bool SC8815::getMask(uint8_t *mask)
{
    return readByte(SC8815_REG_MASK, mask);
}

// ============================================================
// 便捷高级函数
// ============================================================

bool SC8815::setChargeVoltage(uint8_t cell_count, SC8815_VCELL_t vcell_per_cell)
{
    if (cell_count < 1 || cell_count > 4) return false;
    SC8815_CSEL_t csel = (SC8815_CSEL_t)(cell_count - 1);
    // 使用内部设定方式: VBAT_SEL=INTERNAL, IRCOMP=0
    return setVBATSet(SC8815_IRCOMP_0mOhm, SC8815_VBAT_SEL_INTERNAL,
                      csel, vcell_per_cell);
}

bool SC8815::setChargeCurrent(uint16_t ibus_limit_mA, uint16_t ibat_limit_mA)
{
    if (!setIBUSLimit(ibus_limit_mA)) return false;
    if (!setIBATLimit(ibat_limit_mA)) return false;
    return true;
}

bool SC8815::enableChargingMode()
{
    // read-modify-write: 仅修改 EN_OTG bit (bit7)
    uint8_t ctrl0;
    if (!readByte(SC8815_REG_CTRL0_SET, &ctrl0)) return false;
    ctrl0 &= ~(1 << 7);  // EN_OTG=0
    return writeByte(SC8815_REG_CTRL0_SET, ctrl0);
}

bool SC8815::enableDischargingMode(uint16_t vbus_target_mV)
{
    // 注意: 此函数使用内部 FB 方式 (FB_SEL=0)
    // 不适配外部 FB 电阻分压应用

    // 1. 设置 FB_SEL=0 (内部参考)
    uint8_t ctrl1;
    if (!readByte(SC8815_REG_CTRL1_SET, &ctrl1)) return false;
    ctrl1 &= ~(1 << 4);  // FB_SEL=0
    if (!writeByte(SC8815_REG_CTRL1_SET, ctrl1)) return false;

    // 2. 读取 VBUS_RATIO, 计算 VBUSREF_I
    uint8_t ratio_reg;
    if (!readByte(SC8815_REG_RATIO, &ratio_reg)) return false;
    SC8815_VBUS_RATIO_t vbus_ratio = (SC8815_VBUS_RATIO_t)(ratio_reg & 0x01);

    uint16_t ref_mV;
    if (vbus_ratio == SC8815_VBUS_RATIO_5x) {
        ref_mV = vbus_target_mV / 5;
    } else {
        // 12.5x 比率, 目标电压 ÷ 12.5 = 参考电压
        ref_mV = (uint16_t)(vbus_target_mV / 12.5f + 0.5f);
    }
    if (ref_mV < 2)    ref_mV = 2;
    if (ref_mV > 2048) ref_mV = 2048;

    if (!setVBUSRefI(ref_mV)) return false;

    // 3. 设置 EN_OTG=1 (放电模式)
    uint8_t ctrl0;
    if (!readByte(SC8815_REG_CTRL0_SET, &ctrl0)) return false;
    ctrl0 |= (1 << 7);  // EN_OTG=1
    return writeByte(SC8815_REG_CTRL0_SET, ctrl0);
}
