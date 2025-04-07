#include "atm90e32.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cmath>

namespace esphome {
namespace atm90e32 {

static const char *const TAG = "atm90e32";
void ATM90E32Component::loop() {
  if (this->get_publish_interval_flag_()) {
    this->set_publish_interval_flag_(false);
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].voltage_sensor_ != nullptr)
        this->phase_[phase].voltage_ = this->get_phase_voltage_(phase);

      if (this->phase_[phase].current_sensor_ != nullptr)
        this->phase_[phase].current_ = this->get_phase_current_(phase);

      if (this->phase_[phase].power_sensor_ != nullptr)
        this->phase_[phase].active_power_ = this->get_phase_active_power_(phase);

      if (this->phase_[phase].power_factor_sensor_ != nullptr)
        this->phase_[phase].power_factor_ = this->get_phase_power_factor_(phase);

      if (this->phase_[phase].reactive_power_sensor_ != nullptr)
        this->phase_[phase].reactive_power_ = this->get_phase_reactive_power_(phase);

      if (this->phase_[phase].apparent_power_sensor_ != nullptr)
        this->phase_[phase].apparent_power_ = this->get_phase_apparent_power_(phase);

      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr)
        this->phase_[phase].forward_active_energy_ = this->get_phase_forward_active_energy_(phase);

      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr)
        this->phase_[phase].reverse_active_energy_ = this->get_phase_reverse_active_energy_(phase);

      if (this->phase_[phase].phase_angle_sensor_ != nullptr)
        this->phase_[phase].phase_angle_ = this->get_phase_angle_(phase);

      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr)
        this->phase_[phase].harmonic_active_power_ = this->get_phase_harmonic_active_power_(phase);

      if (this->phase_[phase].peak_current_sensor_ != nullptr)
        this->phase_[phase].peak_current_ = this->get_phase_peak_current_(phase);

      // After the local store is collected we can publish them trusting they are within +-1 hardware sampling
      if (this->phase_[phase].voltage_sensor_ != nullptr)
        this->phase_[phase].voltage_sensor_->publish_state(this->get_local_phase_voltage_(phase));

      if (this->phase_[phase].current_sensor_ != nullptr)
        this->phase_[phase].current_sensor_->publish_state(this->get_local_phase_current_(phase));

      if (this->phase_[phase].power_sensor_ != nullptr)
        this->phase_[phase].power_sensor_->publish_state(this->get_local_phase_active_power_(phase));

      if (this->phase_[phase].power_factor_sensor_ != nullptr)
        this->phase_[phase].power_factor_sensor_->publish_state(this->get_local_phase_power_factor_(phase));

      if (this->phase_[phase].reactive_power_sensor_ != nullptr)
        this->phase_[phase].reactive_power_sensor_->publish_state(this->get_local_phase_reactive_power_(phase));

      if (this->phase_[phase].apparent_power_sensor_ != nullptr)
        this->phase_[phase].apparent_power_sensor_->publish_state(this->get_local_phase_apparent_power_(phase));

      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr)
        this->phase_[phase].forward_active_energy_sensor_->publish_state(
            this->get_local_phase_forward_active_energy_(phase));

      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr)
        this->phase_[phase].reverse_active_energy_sensor_->publish_state(
            this->get_local_phase_reverse_active_energy_(phase));

      if (this->phase_[phase].phase_angle_sensor_ != nullptr)
        this->phase_[phase].phase_angle_sensor_->publish_state(this->get_local_phase_angle_(phase));

      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr)
        this->phase_[phase].harmonic_active_power_sensor_->publish_state(
            this->get_local_phase_harmonic_active_power_(phase));

      if (this->phase_[phase].peak_current_sensor_ != nullptr)
        this->phase_[phase].peak_current_sensor_->publish_state(this->get_local_phase_peak_current_(phase));
    }
    if (this->freq_sensor_ != nullptr)
      this->freq_sensor_->publish_state(this->get_frequency_());

    if (this->chip_temperature_sensor_ != nullptr)
      this->chip_temperature_sensor_->publish_state(this->get_chip_temperature_());
  }
}

void ATM90E32Component::update() {
  if (this->read16_(ATM90E32_REGISTER_METEREN) != 1) {
    this->status_set_warning();
    return;
  }
  this->set_publish_interval_flag_(true);
  this->status_clear_warning();

  this->check_phase_status();
  this->check_over_current();
  this->check_freq_status();
}

void ATM90E32Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ATM90E32 Component...");
  this->spi_setup();

  uint16_t mmode0 = 0x87;  // 3P4W 50Hz
  uint16_t high_thresh = 0;
  uint16_t low_thresh = 0;

  if (line_freq_ == 60) {
    mmode0 |= 1 << 12;  // sets 12th bit to 1, 60Hz
    // for freq threshold registers
    high_thresh = 6300;  // 63.00 Hz
    low_thresh = 5700;   // 57.00 Hz
  } else {
    high_thresh = 5300;  // 53.00 Hz
    low_thresh = 4700;   // 47.00 Hz
  }

  if (current_phases_ == 2) {
    mmode0 |= 1 << 8;  // sets 8th bit to 1, 3P3W
    mmode0 |= 0 << 1;  // sets 1st bit to 0, phase b is not counted into the all-phase sum energy/power (P/Q/S)
  }

  this->write16_(ATM90E32_REGISTER_SOFTRESET, 0x789A);    // Perform soft reset
  delay(6);                                               // Wait for the minimum 5ms + 1ms
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);  // enable register config access
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != 0x55AA) {
    ESP_LOGW(TAG, "Could not initialize ATM90E32 IC, check SPI settings");
    this->mark_failed();
    return;
  }

  this->write16_(ATM90E32_REGISTER_METEREN, 0x0001);        // Enable Metering
  this->write16_(ATM90E32_REGISTER_SAGPEAKDETCFG, 0xFF3F);  // Peak Detector time (15:8) 255ms, Sag Period (7:0) 63ms
  this->write16_(ATM90E32_REGISTER_PLCONSTH, 0x0861);       // PL Constant MSB (default) = 140625000
  this->write16_(ATM90E32_REGISTER_PLCONSTL, 0xC468);       // PL Constant LSB (default)
  this->write16_(ATM90E32_REGISTER_ZXCONFIG, 0xD654);       // Zero crossing (ZX2, ZX1, ZX0) pin config
  this->write16_(ATM90E32_REGISTER_MMODE0, mmode0);         // Mode Config (frequency set in main program)
  this->write16_(ATM90E32_REGISTER_MMODE1, pga_gain_);      // PGA Gain Configuration for Current Channels
  this->write16_(ATM90E32_REGISTER_FREQHITH, high_thresh);  // Frequency high threshold
  this->write16_(ATM90E32_REGISTER_FREQLOTH, low_thresh);   // Frequency low threshold
  this->write16_(ATM90E32_REGISTER_PSTARTTH, 0x1D4C);       // All Active Startup Power Threshold - 0.02A/0.00032 = 7500
  this->write16_(ATM90E32_REGISTER_QSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_SSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_PPHASETH, 0x02EE);       // Each Phase Active Phase Threshold - 0.002A/0.00032 = 750
  this->write16_(ATM90E32_REGISTER_QPHASETH, 0x02EE);       // Each phase Reactive Phase Threshold - 10%

  if (this->enable_offset_calibration_) {
    // Initialize flash storage for offset calibrations
    uint32_t o_hash =
        fnv1_hash(App.get_friendly_name() + "_offset_calibration_" + std::to_string((intptr_t) this->cs_));
    this->offset_pref_ = global_preferences->make_preference<OffsetCalibration[3]>(o_hash, true);
    this->restore_offset_calibrations_();

    // Initialize flash storage for power offset calibrations
    uint32_t po_hash =
        fnv1_hash(App.get_friendly_name() + "_power_offset_calibration_" + std::to_string((intptr_t) this->cs_));
    this->power_offset_pref_ = global_preferences->make_preference<PowerOffsetCalibration[3]>(po_hash, true);
    this->restore_power_offset_calibrations_();
  } else {
    ESP_LOGI("CALIBRATION", "Power & Voltage/Current offset calibration is disabled. Using config file values.");
    for (uint8_t phase = 0; phase < 3; ++phase) {
      this->write16_(this->voltage_offset_registers[phase],
                     static_cast<uint16_t>(this->offset_phase_[phase].voltage_offset_));
      this->write16_(this->current_offset_registers[phase],
                     static_cast<uint16_t>(this->offset_phase_[phase].current_offset_));
      this->write16_(this->power_offset_registers[phase],
                     static_cast<uint16_t>(this->power_offset_phase_[phase].active_power_offset));
      this->write16_(this->reactive_power_offset_registers[phase],
                     static_cast<uint16_t>(this->power_offset_phase_[phase].reactive_power_offset));
    }
  }

  if (this->enable_gain_calibration_) {
    // Initialize flash storage for gain calibration
    uint32_t g_hash = fnv1_hash(App.get_friendly_name() + "_gain_calibration_" + std::to_string((intptr_t) this->cs_));
    this->gain_calibration_pref_ = global_preferences->make_preference<GainCalibration[3]>(g_hash, true);
    this->restore_gain_calibrations_();

    if (this->using_saved_calibrations_) {
      ESP_LOGI("CALIBRATION", "Successfully restored gain calibration from memory.");
    } else {
      for (uint8_t phase = 0; phase < 3; ++phase) {
        this->write16_(voltage_gain_registers[phase], this->phase_[phase].voltage_gain_);
        this->write16_(current_gain_registers[phase], this->phase_[phase].ct_gain_);
      }
    }
  } else {
    ESP_LOGI("CALIBRATION", "Gain calibration is disabled. Using config file values.");

    for (uint8_t phase = 0; phase < 3; ++phase) {
      this->write16_(voltage_gain_registers[phase], this->phase_[phase].voltage_gain_);
      this->write16_(current_gain_registers[phase], this->phase_[phase].ct_gain_);
    }
  }

  // Sag threshold (78%)
  uint16_t sagth = calculate_voltage_threshold(line_freq_, this->phase_[0].voltage_gain_, 0.78f);
  // Overvoltage threshold (122%)
  uint16_t ovth = calculate_voltage_threshold(line_freq_, this->phase_[0].voltage_gain_, 1.22f);

  // Write to registers
  this->write16_(ATM90E32_REGISTER_SAGTH, sagth);
  this->write16_(ATM90E32_REGISTER_OVTH, ovth);

  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);  // end configuration
}

void ATM90E32Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E32:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ATM90E32 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage A", this->phase_[PHASEA].voltage_sensor_);
  LOG_SENSOR("  ", "Current A", this->phase_[PHASEA].current_sensor_);
  LOG_SENSOR("  ", "Power A", this->phase_[PHASEA].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power A", this->phase_[PHASEA].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power A", this->phase_[PHASEA].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF A", this->phase_[PHASEA].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy A", this->phase_[PHASEA].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy A", this->phase_[PHASEA].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power A", this->phase_[PHASEA].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle A", this->phase_[PHASEA].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current A", this->phase_[PHASEA].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage B", this->phase_[PHASEB].voltage_sensor_);
  LOG_SENSOR("  ", "Current B", this->phase_[PHASEB].current_sensor_);
  LOG_SENSOR("  ", "Power B", this->phase_[PHASEB].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power B", this->phase_[PHASEB].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power B", this->phase_[PHASEB].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF B", this->phase_[PHASEB].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy B", this->phase_[PHASEB].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy B", this->phase_[PHASEB].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power B", this->phase_[PHASEB].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle B", this->phase_[PHASEB].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current B", this->phase_[PHASEB].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage C", this->phase_[PHASEC].voltage_sensor_);
  LOG_SENSOR("  ", "Current C", this->phase_[PHASEC].current_sensor_);
  LOG_SENSOR("  ", "Power C", this->phase_[PHASEC].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power C", this->phase_[PHASEC].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power C", this->phase_[PHASEC].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF C", this->phase_[PHASEC].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy C", this->phase_[PHASEC].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy C", this->phase_[PHASEC].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power C", this->phase_[PHASEC].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle C", this->phase_[PHASEC].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current C", this->phase_[PHASEC].peak_current_sensor_);
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_);
  LOG_SENSOR("  ", "Chip Temp", this->chip_temperature_sensor_);
}

float ATM90E32Component::get_setup_priority() const { return setup_priority::IO; }

// R/C registers can conly be cleared after the LastSPIData register is updated (register 78H)
// Peakdetect period: 05H. Bit 15:8 are PeakDet_period in ms. 7:0 are Sag_period
// Default is 143FH (20ms, 63ms)
uint16_t ATM90E32Component::read16_(uint16_t /*a_register*/) {
  uint8_t addrh = (1 << 7) | ((a_register >> 8) & 0x03);
  uint8_t addrl = (a_register & 0xFF);
  uint8_t data[2];
  uint16_t output;
  this->enable();
  delay_microseconds_safe(1);  // min delay between CS low and first SCK is 200ns - 1ms is plenty
  this->write_byte(addrh);
  this->write_byte(addrl);
  this->read_array(data, 2);
  this->disable();

  output = (uint16_t(data[0] & 0xFF) << 8) | (data[1] & 0xFF);
  ESP_LOGVV(TAG, "read16_ 0x%04" PRIX16 " output 0x%04" PRIX16, a_register, output);
  return output;
}

int ATM90E32Component::read32_(uint16_t /*addr_h*/, uint16_t /*addr_l*/) {
  const uint16_t val_h = this->read16_(addr_h);
  const uint16_t val_l = this->read16_(addr_l);
  const int32_t val = (val_h << 16) | val_l;

  ESP_LOGVV(TAG,
            "read32_ addr_h 0x%04" PRIX16 " val_h 0x%04" PRIX16 " addr_l 0x%04" PRIX16 " val_l 0x%04" PRIX16
            " = %" PRId32,
            addr_h, val_h, addr_l, val_l, val);

  return val;
}

void ATM90E32Component::write16_(uint16_t /*a_register*/, uint16_t /*val*/) {
  ESP_LOGVV(TAG, "write16_ 0x%04" PRIX16 " val 0x%04" PRIX16, a_register, val);
  this->enable();
  this->write_byte16(a_register);
  this->write_byte16(val);
  this->disable();
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != val)
    ESP_LOGW(TAG, "SPI write error 0x%04X val 0x%04X", a_register, val);
}

float ATM90E32Component::get_local_phase_voltage_(uint8_t /*phase*/) { return this->phase_[phase].voltage_; }

float ATM90E32Component::get_local_phase_current_(uint8_t /*phase*/) { return this->phase_[phase].current_; }

float ATM90E32Component::get_local_phase_active_power_(uint8_t /*phase*/) { return this->phase_[phase].active_power_; }

float ATM90E32Component::get_local_phase_reactive_power_(uint8_t /*phase*/) {
  return this->phase_[phase].reactive_power_;
}

float ATM90E32Component::get_local_phase_apparent_power_(uint8_t /*phase*/) {
  return this->phase_[phase].apparent_power_;
}

float ATM90E32Component::get_local_phase_power_factor_(uint8_t /*phase*/) { return this->phase_[phase].power_factor_; }

float ATM90E32Component::get_local_phase_forward_active_energy_(uint8_t /*phase*/) {
  return this->phase_[phase].forward_active_energy_;
}

float ATM90E32Component::get_local_phase_reverse_active_energy_(uint8_t /*phase*/) {
  return this->phase_[phase].reverse_active_energy_;
}

float ATM90E32Component::get_local_phase_angle_(uint8_t /*phase*/) { return this->phase_[phase].phase_angle_; }

float ATM90E32Component::get_local_phase_harmonic_active_power_(uint8_t /*phase*/) {
  return this->phase_[phase].harmonic_active_power_;
}

float ATM90E32Component::get_local_phase_peak_current_(uint8_t /*phase*/) { return this->phase_[phase].peak_current_; }

float ATM90E32Component::get_phase_voltage_(uint8_t /*phase*/) {
  const uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage)
    ESP_LOGW(TAG, "SPI URMS voltage register read error.");
  return (float) voltage / 100;
}

float ATM90E32Component::get_phase_voltage_avg_(uint8_t /*phase*/) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t voltage = 0;
  for (uint8_t i = 0; i < reads; i++) {
    voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage)
      ESP_LOGW(TAG, "SPI URMS voltage register read error.");
    accumulation += voltage;
  }
  voltage = accumulation / reads;
  this->phase_[phase].voltage_ = (float) voltage / 100;
  return this->phase_[phase].voltage_;
}

float ATM90E32Component::get_phase_current_avg_(uint8_t /*phase*/) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t current = 0;
  for (uint8_t i = 0; i < reads; i++) {
    current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current)
      ESP_LOGW(TAG, "SPI IRMS current register read error.");
    accumulation += current;
  }
  current = accumulation / reads;
  this->phase_[phase].current_ = (float) current / 1000;
  return this->phase_[phase].current_;
}

float ATM90E32Component::get_phase_current_(uint8_t /*phase*/) {
  const uint16_t current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current)
    ESP_LOGW(TAG, "SPI IRMS current register read error.");
  return (float) current / 1000;
}

float ATM90E32Component::get_phase_active_power_(uint8_t /*phase*/) {
  const int val = this->read32_(ATM90E32_REGISTER_PMEAN + phase, ATM90E32_REGISTER_PMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_reactive_power_(uint8_t /*phase*/) {
  const int val = this->read32_(ATM90E32_REGISTER_QMEAN + phase, ATM90E32_REGISTER_QMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_apparent_power_(uint8_t /*phase*/) {
  const int val = this->read32_(ATM90E32_REGISTER_SMEAN + phase, ATM90E32_REGISTER_SMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_power_factor_(uint8_t /*phase*/) {
  uint16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEAN + phase);  // unsigned to compare to lastspidata
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != powerfactor)
    ESP_LOGW(TAG, "SPI power factor read error.");
  return (float) ((int16_t) powerfactor) / 1000;  // make it signed again
}

float ATM90E32Component::get_phase_forward_active_energy_(uint8_t /*phase*/) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGY + phase);
  if ((UINT32_MAX - this->phase_[phase].cumulative_forward_active_energy_) > val) {
    this->phase_[phase].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_forward_active_energy_ = val;
  }
  // 0.01CF resolution = 0.003125 Wh per count
  return ((float) this->phase_[phase].cumulative_forward_active_energy_ * (10.0f / 3200.0f));
}

float ATM90E32Component::get_phase_reverse_active_energy_(uint8_t /*phase*/) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGY + phase);
  if (UINT32_MAX - this->phase_[phase].cumulative_reverse_active_energy_ > val) {
    this->phase_[phase].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_reverse_active_energy_ = val;
  }
  // 0.01CF resolution = 0.003125 Wh per count
  return ((float) this->phase_[phase].cumulative_reverse_active_energy_ * (10.0f / 3200.0f));
}

float ATM90E32Component::get_phase_harmonic_active_power_(uint8_t /*phase*/) {
  int val = this->read32_(ATM90E32_REGISTER_PMEANH + phase, ATM90E32_REGISTER_PMEANHLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_angle_(uint8_t /*phase*/) {
  uint16_t val = this->read16_(ATM90E32_REGISTER_PANGLE + phase) / 10.0;
  return (float) (val > 180) ? val - 360.0 : val;
}

float ATM90E32Component::get_phase_peak_current_(uint8_t /*phase*/) {
  int16_t val = (float) this->read16_(ATM90E32_REGISTER_IPEAK + phase);
  if (!this->peak_current_signed_)
    val = abs(val);
  // phase register * phase current gain value  / 1000 * 2^13
  return (float) (val * this->phase_[phase].ct_gain_ / 8192000.0);
}

float ATM90E32Component::get_frequency_() {
  const uint16_t freq = this->read16_(ATM90E32_REGISTER_FREQ);
  return (float) freq / 100;
}

float ATM90E32Component::get_chip_temperature_() {
  const uint16_t ctemp = this->read16_(ATM90E32_REGISTER_TEMP);
  return (float) ctemp;
}

void ATM90E32Component::run_gain_calibrations() {
  if (!this->get_enable_gain_calibration()) {
    ESP_LOGW("CALIBRATION", "Gain calibration is disabled! Enable it first with enable_gain_calibration: true");
    return;
  }

  float ref_voltages[3] = {
      this->get_reference_voltage(0),
      this->get_reference_voltage(1),
      this->get_reference_voltage(2),
  };
  float ref_currents[3] = {this->get_reference_current(0), this->get_reference_current(1),
                           this->get_reference_current(2)};

  ESP_LOGI("CALIBRATION", "");
  ESP_LOGI("CALIBRATION", "========================= Gain Calibration  =========================");
  ESP_LOGI("CALIBRATION", "---------------------------------------------------------------------");
  ESP_LOGI("CALIBRATION", "| Phase | V_meas (V) | I_meas (A) | V_ref | I_ref  | V_gain (old→new) | I_gain (old→new) |");
  ESP_LOGI("CALIBRATION", "---------------------------------------------------------------------");

  for (uint8_t phase = 0; phase < 3; phase++) {
    uint16_t current_voltage_gain = this->read16_(voltage_gain_registers[phase]);
    uint16_t current_current_gain = this->read16_(current_gain_registers[phase]);

    float measured_voltage = this->get_phase_voltage_avg_(phase);
    float measured_current = this->get_phase_current_avg_(phase);

    bool voltage_valid = ref_voltages[phase] > 0.0f && measured_voltage > 0.0f;
    bool current_valid = ref_currents[phase] > 0.0f && measured_current > 0.0f;

    uint32_t new_voltage_gain = current_voltage_gain;
    uint32_t new_current_gain = current_current_gain;

    if (!voltage_valid && !current_valid) {
      ESP_LOGW("CALIBRATION", "Phase %s - Skipping: No valid reference values for voltage or current.",
               phase_labels[phase]);
      continue;
    }

    if (voltage_valid) {
      new_voltage_gain = static_cast<uint16_t>((ref_voltages[phase] / measured_voltage) * current_voltage_gain);
      if (new_voltage_gain == 0) {
        ESP_LOGW("CALIBRATION", "Phase %s - Voltage Gain would be 0. Check reference voltage.", phase_labels[phase]);
        continue;
      }
      if (new_voltage_gain > 65535) {
        ESP_LOGW("CALIBRATION", "Phase %s - Voltage Gain exceeds 65535, limiting to max.", phase_labels[phase]);
        new_voltage_gain = 65535;
      }
      this->gain_phase_[phase].voltage_gain = new_voltage_gain;
    }

    if (current_valid) {
      new_current_gain = static_cast<uint16_t>((ref_currents[phase] / measured_current) * current_current_gain);
      if (new_current_gain == 0) {
        ESP_LOGW("CALIBRATION", "Phase %s - Current Gain would be 0. Check reference current.", phase_labels[phase]);
        continue;
      }
      if (new_current_gain > 65535) {
        ESP_LOGW("CALIBRATION", "Phase %s - Current Gain exceeds 65535, limiting to max.", phase_labels[phase]);
        new_current_gain = 65535;
      }
      this->gain_phase_[phase].current_gain = new_current_gain;
    }

    this->gain_phase_[phase].voltage_gain = static_cast<uint16_t>(new_voltage_gain);
    this->gain_phase_[phase].current_gain = static_cast<uint16_t>(new_current_gain);

    // Final row output
    ESP_LOGI("CALIBRATION", "|   %c   |  %9.2f |  %9.4f | %5.2f | %6.4f |  %5u → %-5u  |  %5u → %-5u  |", 'A' + phase,
             measured_voltage, measured_current, ref_voltages[phase], ref_currents[phase], current_voltage_gain,
             this->gain_phase_[phase].voltage_gain, current_current_gain, this->gain_phase_[phase].current_gain);
  }

  ESP_LOGI("CALIBRATION", "=====================================================================\n");

  this->save_gain_calibration_to_memory_();
  this->write_gains_to_registers_();
  this->verify_gain_writes_();
}

void ATM90E32Component::save_gain_calibration_to_memory_() {
  bool success = this->gain_calibration_pref_.save(&this->gain_phase_);
  if (success) {
    this->using_saved_calibrations_ = true;
    ESP_LOGI("CALIBRATION", "Gain calibration saved to memory.");
  } else {
    this->using_saved_calibrations_ = false;
    ESP_LOGE("CALIBRATION", "Failed to save gain calibration to memory!");
  }
}

void ATM90E32Component::run_offset_calibrations() {
  for (uint8_t phase = 0; phase < 3; phase++) {
    int16_t voltage_offset = calibrate_offset(phase, true);
    int16_t current_offset = calibrate_offset(phase, false);

    this->write_offsets_to_registers_(phase, voltage_offset, current_offset);

    ESP_LOGI("CALIBRATION", "Phase %c - offset_voltage: %d, offset_current: %d", 'A' + phase, voltage_offset,
             current_offset);
  }

  this->offset_pref_.save(&this->offset_phase_);  // Save to flash
}

void ATM90E32Component::run_power_offset_calibrations() {
  for (uint8_t phase = 0; phase < 3; ++phase) {
    int16_t active_offset = calibrate_power_offset(phase, false);
    int16_t reactive_offset = calibrate_power_offset(phase, true);

    this->write_power_offsets_to_registers_(phase, active_offset, reactive_offset);

    ESP_LOGI("CALIBRATION", "Phase %c - offset_active_power: %d, offset_reactive_power: %d", 'A' + phase, active_offset,
             reactive_offset);
  }

  this->power_offset_pref_.save(&this->power_offset_phase_);  // Save to flash
}

void ATM90E32Component::write_gains_to_registers_() {
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);

  for (int phase = 0; phase < 3; phase++) {
    this->write16_(voltage_gain_registers[phase], this->gain_phase_[phase].voltage_gain);
    this->write16_(current_gain_registers[phase], this->gain_phase_[phase].current_gain);
  }

  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);
}

void ATM90E32Component::write_offsets_to_registers_(uint8_t /*phase*/, int16_t /*voltage_offset*/,
                                                    int16_t /*current_offset*/) {
  // Save to runtime
  this->offset_phase_[phase].voltage_offset_ = voltage_offset;
  this->phase_[phase].voltage_offset_ = voltage_offset;

  // Save to flash-storable struct
  this->offset_phase_[phase].current_offset_ = current_offset;
  this->phase_[phase].current_offset_ = current_offset;

  // Write to registers
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);
  this->write16_(voltage_offset_registers[phase], static_cast<uint16_t>(voltage_offset));
  this->write16_(current_offset_registers[phase], static_cast<uint16_t>(current_offset));
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);
}

void ATM90E32Component::write_power_offsets_to_registers_(uint8_t /*phase*/, int16_t /*p_offset*/,
                                                          int16_t /*q_offset*/) {
  // Save to runtime
  this->phase_[phase].active_power_offset_ = p_offset;
  this->phase_[phase].reactive_power_offset_ = q_offset;

  // Save to flash-storable struct
  this->power_offset_phase_[phase].active_power_offset = p_offset;
  this->power_offset_phase_[phase].reactive_power_offset = q_offset;

  // Write to registers
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);
  this->write16_(this->power_offset_registers[phase], static_cast<uint16_t>(p_offset));
  this->write16_(this->reactive_power_offset_registers[phase], static_cast<uint16_t>(q_offset));
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);
}

void ATM90E32Component::restore_gain_calibrations_() {
  if (this->gain_calibration_pref_.load(&this->gain_phase_)) {
    this->write_gains_to_registers_();

    if (this->verify_gain_writes()) {
      this->using_saved_calibrations_ = true;
      ESP_LOGI("CALIBRATION", "Gain calibration loaded and verified successfully.");
    } else {
      this->using_saved_calibrations_ = false;
      ESP_LOGE("CALIBRATION", "Gain verification failed! Calibration may not be applied correctly.");
    }
  } else {
    this->using_saved_calibrations_ = false;
    ESP_LOGW("CALIBRATION", "No stored gain calibrations found. Using config file values.");
  }
}

void ATM90E32Component::restore_offset_calibrations_() {
  if (this->offset_pref_.load(&this->offset_phase_)) {
    ESP_LOGI("CALIBRATION", "Successfully restored offset calibration from memory.");

    for (uint8_t phase = 0; phase < 3; phase++) {
      auto &offset = this->offset_phase_[phase];
      write_offsets_to_registers_(phase, offset.voltage_offset_, offset.current_offset_);
      ESP_LOGI("CALIBRATION", "Phase %c - offset_voltage:: %d, offset_current: %d", 'A' + phase, offset.voltage_offset_,
               offset.current_offset_);
    }
  } else {
    ESP_LOGW("CALIBRATION", "No stored offset calibrations found. Using default values.");
  }
}

void ATM90E32Component::restore_power_offset_calibrations_() {
  if (this->power_offset_pref_.load(&this->power_offset_phase_)) {
    ESP_LOGI("CALIBRATION", "Successfully restored power offset calibration from memory.");

    for (uint8_t phase = 0; phase < 3; ++phase) {
      auto &offset = this->power_offset_phase_[phase];
      write_power_offsets_to_registers_(phase, offset.active_power_offset, offset.reactive_power_offset);
      ESP_LOGI("CALIBRATION", "Phase %c - offset_active_power: %d, offset_reactive_power: %d", 'A' + phase,
               offset.active_power_offset, offset.reactive_power_offset);
    }
  } else {
    ESP_LOGW("CALIBRATION", "No stored power offsets found. Using default values.");
  }
}

void ATM90E32Component::clear_gain_calibrations() {
  ESP_LOGI("CALIBRATION", "Clearing stored gain calibrations and restoring config-defined values...");

  for (int phase = 0; phase < 3; phase++) {
    gain_phase_[phase].voltage_gain = this->phase_[phase].voltage_gain_;
    gain_phase_[phase].current_gain = this->phase_[phase].ct_gain_;
  }

  bool success = this->gain_calibration_pref_.save(&this->gain_phase_);
  this->using_saved_calibrations_ = false;

  if (success) {
    ESP_LOGI("CALIBRATION", "Gain calibrations cleared. Config values restored:");
    for (int phase = 0; phase < 3; phase++) {
      ESP_LOGI("CALIBRATION", "  Phase %c - Voltage Gain: %u, Current Gain: %u", 'A' + phase,
               gain_phase_[phase].voltage_gain, gain_phase_[phase].current_gain);
    }
  } else {
    ESP_LOGE("CALIBRATION", "Failed to clear gain calibrations!");
  }

  this->write_gains_to_registers_();  // Apply them to the chip immediately
}

void ATM90E32Component::clear_offset_calibrations() {
  for (uint8_t phase = 0; phase < 3; phase++) {
    this->write_offsets_to_registers_(phase, 0, 0);
  }

  this->offset_pref_.save(&this->offset_phase_);  // Save cleared values to flash memory

  ESP_LOGI("CALIBRATION", "Offsets cleared.");
}

void ATM90E32Component::clear_power_offset_calibrations() {
  for (uint8_t phase = 0; phase < 3; phase++) {
    this->write_power_offsets_to_registers_(phase, 0, 0);
  }

  this->power_offset_pref_.save(&this->power_offset_phase_);

  ESP_LOGI("CALIBRATION", "Power offsets cleared.");
}

int16_t ATM90E32Component::calibrate_offset(uint8_t /*phase*/, bool voltage) {
  const uint8_t num_reads = 5;
  uint64_t total_value = 0;

  for (uint8_t i = 0; i < num_reads; ++i) {
    uint32_t reading = voltage ? this->read32_(ATM90E32_REGISTER_URMS + phase, ATM90E32_REGISTER_URMSLSB + phase)
                               : this->read32_(ATM90E32_REGISTER_IRMS + phase, ATM90E32_REGISTER_IRMSLSB + phase);
    total_value += reading;
  }

  const uint32_t average_value = total_value / num_reads;
  const uint32_t shifted = average_value >> 7;
  const uint32_t offset = ~shifted + 1;
  return static_cast<int16_t>(offset);  // Takes lower 16 bits
}

int16_t ATM90E32Component::calibrate_power_offset(uint8_t /*phase*/, bool reactive) {
  const uint8_t num_reads = 5;
  uint64_t total_value = 0;

  for (uint8_t i = 0; i < num_reads; ++i) {
    uint32_t reading = reactive ? this->read32_(ATM90E32_REGISTER_QMEAN + phase, ATM90E32_REGISTER_QMEANLSB + phase)
                                : this->read32_(ATM90E32_REGISTER_PMEAN + phase, ATM90E32_REGISTER_PMEANLSB + phase);
    total_value += reading;
  }

  const uint32_t average_value = total_value / num_reads;
  const uint32_t power_offset = ~average_value + 1;
  return static_cast<int16_t>(power_offset);  // Takes the lower 16 bits
}

bool ATM90E32Component::verify_gain_writes_() {
  bool success = true;
  for (uint8_t phase = 0; phase < 3; phase++) {
    uint16_t read_voltage = this->read16_(voltage_gain_registers[phase]);
    uint16_t read_current = this->read16_(current_gain_registers[phase]);

    if (read_voltage != this->gain_phase_[phase].voltage_gain ||
        read_current != this->gain_phase_[phase].current_gain) {
      ESP_LOGE("CALIBRATION", "Mismatch detected for Phase %s!", phase_labels[phase]);
      success = false;
    }
  }
  return success;  // Return true if all writes were successful, false otherwise
}

void ATM90E32Component::check_phase_status() {
  uint16_t state0 = this->read16_(ATM90E32_REGISTER_EMMSTATE0);
  uint16_t state1 = this->read16_(ATM90E32_REGISTER_EMMSTATE1);

  for (int phase = 0; phase < 3; phase++) {
    std::string status;

    if (state0 & over_voltage_flags[phase])
      status += "Over Voltage; ";
    if (state1 & voltage_sag_flags[phase])
      status += "Voltage Sag; ";
    if (state1 & phase_loss_flags[phase])
      status += "Phase Loss; ";

    auto *sensor = this->phase_status_text_sensor_[phase];
    const char *phase_name = sensor ? sensor->get_name().c_str() : "Unknown Phase";
    if (!status.empty()) {
      status.pop_back();  // remove space
      status.pop_back();  // remove semicolon
      ESP_LOGW(TAG, "%s: %s", phase_name, status.c_str());
      if (sensor != nullptr)
        sensor->publish_state(status);
    } else {
      if (sensor != nullptr)
        sensor->publish_state("Okay");
    }
  }
}

void ATM90E32Component::check_freq_status() {
  uint16_t state1 = this->read16_(ATM90E32_REGISTER_EMMSTATE1);

  std::string freq_status;

  if (state1 & ATM90E32_STATUS_S1_FREQHIST)
    freq_status = "HIGH";
  else if (state1 & ATM90E32_STATUS_S1_FREQLOST)
    freq_status = "LOW";
  else
    freq_status = "Normal";

  ESP_LOGW(TAG, "Frequency status: %s", freq_status.c_str());

  if (this->freq_status_text_sensor_ != nullptr) {
    this->freq_status_text_sensor_->publish_state(freq_status);
  }
}

void ATM90E32Component::check_over_current() {
  constexpr float MAX_CURRENT_THRESHOLD = 65.53f;

  for (uint8_t phase = 0; phase < 3; phase++) {
    float current_val =
        this->phase_[phase].current_sensor_ != nullptr ? this->phase_[phase].current_sensor_->state : 0.0f;

    if (current_val > MAX_CURRENT_THRESHOLD) {
      ESP_LOGW(TAG, "Over current detected on Phase %c: %.2f A", 'A' + phase, current_val);
      ESP_LOGW(TAG, "You may need to half your gain_ct: value & multiply the current and power values by 2");
      if (this->phase_status_text_sensor_[phase] != nullptr) {
        this->phase_status_text_sensor_[phase]->publish_state("Over Current; ");
      }
    }
  }
}

uint16_t ATM90E32Component::calculate_voltage_threshold(int line_freq, uint16_t /*ugain*/, float multiplier) {
  // this assumes that 60Hz electrical systems use 120V mains,
  // which is usually, but not always the case
  float nominal_voltage = (line_freq == 60) ? 120.0f : 220.0f;
  float target_voltage = nominal_voltage * multiplier;

  float peak_01v = target_voltage * 100.0f * std::sqrt(2.0f);  // convert RMS → peak, scale to 0.01V
  float divider = (2.0f * ugain) / 32768.0f;

  float threshold = peak_01v / divider;

  return static_cast<uint16_t>(threshold);
}

}  // namespace atm90e32
}  // namespace esphome
