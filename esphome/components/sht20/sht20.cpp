// SHT20 temperature + humidity sensor component

// Default 12/14 bit resolution and 29/85 ms sample time

// Based on my custom component, which has been stable for quite some time now
// (After I read the datasheet again and implemented the OTP reload procedure)
// https://github.com/MeisterSchlaueLampe/esphome-components

// The maximum sensor update frequency using this implementation is <= 1 Hz,
// which should be okay for an ambient sensor.

// There are no optional extra features available because this implementation uses
// the Soft Reset + OTP Reload procedure for maximum safety and stability.
// This means every measurement will take place with the default settings in effect.
// The default settings are totally fine though: heater off and maximum resolution.

// SHT20 datasheet used:
// https://sensirion.com/media/documents/CCDE1377/635000A2/Sensirion_Datasheet_Humidity_Sensor_SHT20.pdf
// ( Version 5 - October 2022 )

#include "sht20.h"

#include "esphome/core/log.h"

namespace esphome {
namespace sht20 {

static const char * const TAG = "sht20";

static const uint8_t SHT20_CMD_RESET           = 0xFE;
static const uint8_t SHT20_CMD_TEMPERATURE     = 0xF3; // no hold master mode (section 5.4)
static const uint8_t SHT20_CMD_HUMIDITY        = 0xF5; // no hold master mode
static const uint8_t SHT20_CMD_WRITE_USER_REG  = 0xE6;
static const uint8_t SHT20_CMD_READ_USER_REG   = 0xE7;

// For now just wait 100ms after each command.
// This should be safe, since the maximum expected exec delay for the SHT20 is < 90ms.
static const uint8_t SHT20_DELAY = 100;

bool SHT20Component::send_command_( uint8_t cmd )
{
  return this->write( &cmd, 1 ) == i2c::ERROR_OK;
  // The SHT20 actually requires a ~ 20 µS delay between the command byte and the
  // stop condition. Because it is not possible to ensure the timings here, the
  // OTP Reload procedure has to be used. ( see SHT20 datasheet section 5.8 )
}

void SHT20Component::setup()
{
  if ( this->send_command_( SHT20_CMD_RESET ) )
    ESP_LOGV( TAG, "SHT20 sensor initialization complete!" );
  else
    ESP_LOGE( TAG, "SHT20 sensor initialization failed!" );
}

void SHT20Component::update()
{
  if ( this->measurement_progress_ )
  {
    ESP_LOGE( TAG, "SHT20 update requested while a measurement is still in progress!" );
    return;
  }

  this->measurement_progress_ = 1;
  this->advance_measurements_();
}

void SHT20Component::advance_measurements_()
{
  bool error = false;

  switch ( this->measurement_progress_ )
  {
    case 1:
      ESP_LOGD( TAG, "SHT20 initiating soft reset" );
      if ( ! this->send_command_( SHT20_CMD_RESET ) )
      {
        ESP_LOGE( TAG, "SHT20 soft reset error" );
        error = true;
      }
      break;
    case 2:
      ESP_LOGD( TAG, "SHT20 temperature measurement" );
      if ( ! this->force_otp_reload_() )
      {
        ESP_LOGE( TAG, "SHT20 communication error!" );
        error = true;
      }
      else
      {
        if ( ! this->send_command_( SHT20_CMD_TEMPERATURE ) )
        {
          ESP_LOGE( TAG, "SHT20 communication error!" );
          error = true;
        }
      }
      break;
    case 3:
      ESP_LOGD( TAG, "SHT20 reading temperature" );
      if ( this->read( & this->measurement_results_[ 0 ], 2 ) != i2c::ERROR_OK )
      {
        ESP_LOGE( TAG, "SHT20 temperature reading not ready yet" );
        error = true;
      }
      break;
    case 4:
      ESP_LOGD( TAG, "SHT20 humidity measurement" );
      if ( ! this->force_otp_reload_() )
      {
        ESP_LOGE( TAG, "SHT20 communication error!" );
        error = true;
      }
      else
      {
        if ( ! this->send_command_( SHT20_CMD_HUMIDITY ) )
        {
          ESP_LOGE( TAG, "SHT20 communication error!" );
          error = true;
        }
      }
      break;
    case 5:
      ESP_LOGD( TAG, "SHT20 reading humidity value" );
      if ( this->read( & this->measurement_results_[ 2 ], 2 ) != i2c::ERROR_OK )
      {
        ESP_LOGE( TAG, "SHT20 humidity reading not ready yet" );
        error = true;
      }
      break;
    default:
      error = true;
      break;
  }

  if ( error )
  { // error detected : abort and notify sensors
    this->temperature_sensor_->publish_state( NAN );
    this->humidity_sensor_->publish_state( NAN );
    this->measurement_progress_ = 0;
    return;
  }

  if ( this->measurement_progress_ == 5 )
  { // all done : publish results
    uint16_t t = measurement_results_[ 0 ];
    t <<= 8;
    t |= measurement_results_[ 1 ] & 0xfc;
    temperature_sensor_->publish_state( 175.72 * t / 65536.0 - 46.85 );

    uint16_t h = measurement_results_[ 2 ];
    h <<= 8;
    h |=  measurement_results_[ 3 ] & 0xfc;
    humidity_sensor_->publish_state( 125.0 * h / 65536.0 - 6.0 );

    this->measurement_progress_ = 0;
  }
  else
  { // not done yet : continue with next step
    this->measurement_progress_ += 1;
    this->set_timeout( SHT20_DELAY, [this]() { this->advance_measurements_(); } );
  }
}


bool SHT20Component::force_otp_reload_()
{ // force OTP reload during next measurement ( see SHT20 datasheet section 5.8 )
  uint8_t data[2] = {0,0};

  if ( ! this->send_command_( SHT20_CMD_READ_USER_REG ) )
    return false;

  if ( this->read( & data[ 1 ], 1 ) != i2c::ERROR_OK )
    return false;

  data[ 0 ] = SHT20_CMD_WRITE_USER_REG;
  data[ 1 ] &= 0b00111000; // make sure the reserved bits are written back unchanged

  if ( this->write( data, 2 ) != i2c::ERROR_OK )
    return false;

  return true;
}

void SHT20Component::dump_config()
{
  ESP_LOGCONFIG( TAG, "SHT20:" );
  LOG_I2C_DEVICE( this );
  LOG_SENSOR( "  ", "Temperature", this->temperature_sensor_ );
  LOG_SENSOR( "  ", "Humidity", this->humidity_sensor_ );
}

} // namespace sh20
} // namespace esphome

