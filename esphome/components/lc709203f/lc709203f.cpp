#include "esphome/core/log.h"
#include "lc709203f.h"

namespace esphome {
namespace lc709203f {

static const char *TAG = "lc709203f.sensor";

// Device I2C address. This address is fixed.
static const uint8_t LC709203F_I2C_ADDR_DEFAULT = 0x0B;

// Device registers
static const uint8_t LC709203F_BEFORE_RSOC = 0x04;
static const uint8_t LC709203F_THERMISTOR_B = 0x06;
static const uint8_t LC709203F_INITIAL_RSOC = 0x07;
static const uint8_t LC709203F_CELL_TEMPERATURE = 0x08;
static const uint8_t LC709203F_CELL_VOLTAGE = 0x09;
static const uint8_t LC709203F_CURRENT_DIRECTION = 0x0A;
static const uint8_t LC709203F_APA = 0x0B;
static const uint8_t LC709203F_APT = 0x0C;
static const uint8_t LC709203F_RSOC = 0x0D;
static const uint8_t LC709203F_ITE = 0x0F;
static const uint8_t LC709203F_IC_VERSION = 0x11;
static const uint8_t LC709203F_CHANGE_OF_THE_PARAMETER = 0x12;
static const uint8_t LC709203F_ALARM_LOW_RSOC = 0x13;
static const uint8_t LC709203F_ALARM_LOW_CELL_VOLTAGE = 0x14;
static const uint8_t LC709203F_IC_POWER_MODE = 0x15;
static const uint8_t LC709203F_STATUS_BIT = 0x16;
static const uint8_t LC709203F_NUMBER_OF_THE_PARAMETER = 0x1A;

static const uint8_t LC709203F_POWER_MODE_ON = 0x0001;
static const uint8_t LC709203F_POWER_MODE_SLEEP = 0x0002;

static const uint8_t LC709203F_STATE_INIT = 0x01;
static const uint8_t LC709203F_STATE_RSOC = 0x02;
static const uint8_t LC709203F_STATE_TEMP_SETUP = 0x03;
static const uint8_t LC709203F_STATE_NORMAL = 0x00;

// The number of times to retry an I2C transaction before giving up. In my experience,
//  10 is a good number here that will take care of most bus issues that require retry.
static const uint8_t LC709203F_I2C_RETRY_COUNT = 10;

void lc709203f::setup() {
  // Note: The setup implements a small state machine. This is because we want to have
  //  delays before and after sending the RSOC command. The full init process should be:
  //       INIT->RSOC->TEMP_SETUP->NORMAL
  //  The setup() function will only perform the first part of the initialization process.
  //  Assuming no errors, the whole process should occur during the setup() function and
  //  the first two calls to update(). After that, the part should remain in normal mode
  //  until a device reset.
  //
  //  This device can be picky about I2C communication and can error out occasionally. The
  //  get/set register functions impelment retry logic to retry the I2C transactions. The
  //  initialization code checks the return code from those functions. If they don't return
  //  NO_ERROR (0x00), that part of the initialization aborts and will be retried on the next
  //  call to update().
  ESP_LOGCONFIG(TAG, "Setting up LC709203F...");
  uint16_t buffer;

  this->state_ = LC709203F_STATE_INIT;
  // Note: Here I am tempted to read the LC709203F_IC_VERSION register and verify that it is
  //  correct. However, I don't actually know what the expected value of that register is. The
  //  device I am using to test has 0x2AFF in that register, but I don't know if they will all
  //  have that value (and the datasheet does not say). The code below could be used to do this
  //  check.
  /*
  if (this->get_register(LC709203F_IC_VERSION, &buffer) != i2c::NO_ERROR)
  {
      // I2C bus error, init will be retried on the next call to update()
      return;
  }
  else
  {
      if (buffer != 0x2AFF)
      {
          // A register was read from a device at the correct address, but it does not match
          //  what we expect for the LC709203F
          ESP_LOGE(TAG, "Device does not appear to be a LC709203F");
          this->status_set_error("unrecognised");
          this->mark_failed();
          return;
      }
  }
  */

  // Set power mode to on. Note that, unlike some other similar devices, in sleep mode the IC
  //  does not record power usage. If there is significant power consumption during sleep mode,
  //  the pack RSOC will likely no longer be correct. Because of that, I do not implement
  //  sleep mode on this device.
  if (this->set_register(LC709203F_IC_POWER_MODE, LC709203F_POWER_MODE_ON) == i2c::NO_ERROR)
  {
      if (this->set_register(LC709203F_APA, this->APA_) == i2c::NO_ERROR)
      {
          if (this->set_register(LC709203F_CHANGE_OF_THE_PARAMETER, this->pack_voltage_) == i2c::NO_ERROR)
          {
              this->state_ = LC709203F_STATE_RSOC;
          }
      }
  }
  // Note: Initialization continues in the update() function.
}

void lc709203f::update(){
  uint16_t buffer;

  if (this->state_ == LC709203F_STATE_NORMAL)
  {
      // Note: If we fail to read from the data registers, we do not report any sensor reading.
      if (this->voltage_sensor_ != nullptr)
      {
          if (this->get_register(LC709203F_CELL_VOLTAGE, &buffer) == i2c::NO_ERROR)
          {
              // Raw units are mV
              this->voltage_sensor_->publish_state(static_cast<float>(buffer)/1000.0);
              this->status_clear_warning();
          }
      }
      if (this->battery_remaining_sensor_ != nullptr)
      {
          if (this->get_register(LC709203F_ITE, &buffer) == i2c::NO_ERROR)
          {
              // Raw units are .1%
              this->battery_remaining_sensor_->publish_state(static_cast<float>(buffer)/10.0);
              this->status_clear_warning();
          }
      }
      if (this->temperature_sensor_ != nullptr)
      {
          // I can't test this with a real thermistor because I don't have a device with
          //  an attached thermistor. I have turned on the sensor and made sure that it
          //  sets up the registers properly.
          if (this->get_register(LC709203F_CELL_TEMPERATURE, &buffer) == i2c::NO_ERROR)
          {
              // Raw units are .1 K
              this->temperature_sensor_->publish_state( (static_cast<float>(buffer)/10.0) - 273.15 );
              this->status_clear_warning();
          }
      }
  }
  else if (this->state_ == LC709203F_STATE_INIT)
  {
      // We should only get here if the init sequence failed during the setup() function.
      //  This would likely occur because of a repeated failure on the I2C bus.
      if (this->set_register(LC709203F_IC_POWER_MODE, LC709203F_POWER_MODE_ON) == i2c::NO_ERROR)
      {
          if (this->set_register(LC709203F_APA, this->APA_) == i2c::NO_ERROR)
          {
              if (this->set_register(LC709203F_CHANGE_OF_THE_PARAMETER, this->pack_voltage_) == i2c::NO_ERROR)
              {
                  this->state_ = LC709203F_STATE_RSOC;
              }
          }
      }
  }
  else if (this->state_ == LC709203F_STATE_RSOC)
  {
      // We implement a delay here to send the initial RSOC command.
      //  This should run once on the first update() after initialization.
      if (this->set_register(LC709203F_INITIAL_RSOC, 0xAA55) == i2c::NO_ERROR)
      {
          this->state_ = LC709203F_STATE_TEMP_SETUP;
      }
  }
  else if (this->state_ == LC709203F_STATE_TEMP_SETUP)
  {
      // This should run once on the second update() after initialization.
      if (this->temperature_sensor_ != nullptr)
      {
          // This assumes that a thermistor is attached to the device as shown in the datahseet.
          if (this->set_register(LC709203F_STATUS_BIT, 0x0001) == i2c::NO_ERROR)
          {
              if (this->set_register(LC709203F_THERMISTOR_B, this->B_constant_) == i2c::NO_ERROR)
              {
                  this->state_ = LC709203F_STATE_NORMAL;
              }
          }
      }
      else
      {
          // The device expects to get updates to the temperature in this mode.
          //  I am not doing that now. The temperature register defaults to 25C.
          //  In theory, we could have another temperature sensor and have ESPHome
          //  send updated temperature to the device occasionally, but I have no idea
          //  how to make that happen.
          if (this->set_register(LC709203F_STATUS_BIT, 0x0000) == i2c::NO_ERROR)
          {
              this->state_ = LC709203F_STATE_NORMAL;
          }
      }
  }
}

void lc709203f::dump_config(){
  ESP_LOGCONFIG(TAG, "LC709203F:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
  {
      // Nothing in this code actually sets the device to failed, so this should never trigger.
      //  I am leaving this in incase I want to implement a check during init to verify the
      //  correct device is on the bus.
      ESP_LOGCONFIG(TAG, "  I2C Device at 0x0B does not appear to be a LC709203F");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Pack Size: %d mAH", this->pack_size_);
  ESP_LOGCONFIG(TAG, "  Pack APA: 0x%02X", this->APA_);
  if (this->pack_voltage_ == 0x0000)
  {
      ESP_LOGCONFIG(TAG, "  Pack Rated Voltage: 3.8V");
  }
  else
  {
      // This is only true if the pack_voltage_ is 0x0001. The config validator should
      //  have already made sure that this value can be only 0 or 1.
      ESP_LOGCONFIG(TAG, "  Pack Rated Voltage: 3.7V");
  }

  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Remaining", this->battery_remaining_sensor_);

  if (this->temperature_sensor_ != nullptr)
  {
      LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
      ESP_LOGCONFIG(TAG, "    B_Constant: %d", this->B_constant_);
  }
  else
  {
      ESP_LOGCONFIG(TAG, "  No Temperature Sensor");
  }
}

uint8_t lc709203f::get_register(uint8_t register_to_read, uint16_t *register_value)
{
  i2c::ErrorCode return_code;
  uint8_t read_buffer[6];

  read_buffer[0] = (this->address_) << 1;
  read_buffer[1] = register_to_read;
  read_buffer[2] = ((this->address_) << 1) | 0x01;

  for (int i = 0; i <= LC709203F_I2C_RETRY_COUNT; i++)
  {
      return_code = i2c::NO_ERROR;

      // Note: the read_register() function does not send a stop between the write and
      //  the read portions of the I2C transation when you set the last variable to 'false'
      //  as we do below. Some of the other I2C read functions such as the generic read()
      //  function will send a stop between the read and the write portion of the I2C
      //  transaction. This is bad in this case and will result in reading nothing but 0xFFFF
      //  from the registers.
      return_code = this->read_register(register_to_read, &read_buffer[3], 3, false);
      if (return_code != i2c::NO_ERROR)
      {
          // Error on the i2c bus
          snprintf(this->error_code_buffer, 50,
            "Error code %d when reading from register 0x%02X", return_code, register_to_read);
          this->status_set_warning(this->error_code_buffer);
      }
      else
      {
          if(this->CRC8(read_buffer, 5) != read_buffer[5])
          {
              // I2C indicated OK, but the CRC of the data does not matcth.
              snprintf(this->error_code_buffer, 50,
                "CRC error reading from register 0x%02X", register_to_read);
              this->status_set_warning(this->error_code_buffer);
          }
          else
          {
              *register_value = ((uint16_t)read_buffer[4] << 8) | (uint16_t)read_buffer[3];
              return i2c::NO_ERROR;
          }
      }
  }

  // If we get here, we tried LC709203F_I2C_RETRY_COUNT times to read the register and
  //  failed each time. Set the register value to 0 and return the I2C error code or 0xFF
  //  to indicate a CRC failure. It will be up to the higher level code what to do when
  //  this happens.
  *register_value = 0x0000;
  if (return_code != i2c::NO_ERROR)
  {
      return return_code;
  }
  else
  {
      return 0xFF;
  }
}

uint8_t lc709203f::set_register(uint8_t register_to_set, uint16_t value_to_set)
{
  i2c::ErrorCode return_code;
  uint8_t write_buffer[5];

  // Note: We don't actually send byte[0] of the buffer. We include it because it is
  //  part of the CRC calculation.
  write_buffer[0] = (this->address_) << 1;
  write_buffer[1] = register_to_set;
  write_buffer[2] = value_to_set&0xFF;           // Low byte
  write_buffer[3] = (value_to_set >> 8) & 0xFF;  // High byte
  write_buffer[4] = this->CRC8(write_buffer, 4);


  for (int i = 0; i <= LC709203F_I2C_RETRY_COUNT; i++)
  {
      return_code = i2c::NO_ERROR;

      // Note: we don't write the first byte of the write buffer to the device.
      //  This is done automatically by the write() function.
      return_code = this->write(&write_buffer[1], 4, true);
      if (return_code == i2c::NO_ERROR)
      {
          return return_code;
      }
      else
      {
          snprintf(this->error_code_buffer, 50,
            "Error code %d when writing to register 0x%02X", return_code, register_to_set);
          this->status_set_warning(this->error_code_buffer);
      }
  }

  // If we get here, we tried to send the data LC709203F_I2C_RETRY_COUNT times and failed.
  //  We return the I2C error code, it is up to the higher level code what to do about it.
  return return_code;
}

uint8_t lc709203f::CRC8(uint8_t *byte_buffer, uint8_t length_of_CRC)
{
  uint8_t crc = 0x00;
  const uint8_t POLYNOMIAL(0x07);

  for (int j = length_of_CRC; j; --j)
  {
      crc ^= *byte_buffer++;

      for (int i = 8; i; --i)
      {
          crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
      }
  }

return crc;
}

void lc709203f::set_pack_size(uint16_t pack_size)
{
  uint16_t pack_size_array[6] = {100, 200, 500, 1000, 2000, 3000};
  uint16_t APA_array[6] = {0x08, 0x0B, 0x10, 0x19, 0x2D, 0x36};
  float slope;
  float intercept;

  this->pack_size_ = pack_size;    // Pack size in mAH

  // The size is used to calculate the 'Adjustment Pack Application' number.
  // Here we assume a type 01 or type 03 battery and do a linear curve fit to find the APA.
  for (uint8_t i = 0; i < sizeof(pack_size_array)/sizeof(pack_size_array[0]); i++)
  {
      if (pack_size_array[i] == pack_size)
      {
          // If the pack size is exactly one of the values in the array.
          this->APA_ = APA_array[i];
          return;
      }
      else if((i > 0) && (pack_size_array[i] > pack_size) && (pack_size_array[i-1] < pack_size))
      {
          // If the pack size is between the current array element and the previous. Do a linear
          //  Curve fit to determine the APA value.

          // Type casting is required here to avoid interger division
          slope = static_cast<float>(APA_array[i] - APA_array[i-1]) /
                  static_cast<float>(pack_size_array[i] - pack_size_array[i-1]);

          // Type casting might not be needed here.
          intercept = static_cast<float>(APA_array[i])-slope *
                      static_cast<float>(pack_size_array[i]);

          this->APA_ = static_cast<uint8_t>(slope*pack_size+intercept);
          return;
      }
  }
  // We should never get here. If we do, it means we never set the pack APA. This should
  //  not be possible because of the config validation. However, if it does happen, the
  //  consequence is that the RSOC values will likley not be as accurate. However, it should
  //  not cause an error or crash, so I am not doing any additional checking here.
  return;
}

void lc709203f::set_thermistor_B_constant(uint16_t B_constant)
{
  this->B_constant_ = B_constant;
  return;
}

void lc709203f::set_pack_voltage(LC709203FBatteryVoltage pack_voltage)
{
  this->pack_voltage_ = pack_voltage;
  return;
}

}  // namespace lc709203f
}  // namespace esphome
