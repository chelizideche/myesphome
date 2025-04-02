#include "pasco2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pasco2 {

static const char *const TAG = "pasco2";

static const uint8_t XENSIV_PASCO2_REG_PROD_ID = 0x00U;      // REG_PROD
static const uint8_t XENSIV_PASCO2_REG_SENS_STS = 0x01U;     // SENS_STS
static const uint8_t XENSIV_PASCO2_REG_MEAS_RATE_H = 0x02U;  // MEAS_RATE_H
static const uint8_t XENSIV_PASCO2_REG_MEAS_RATE_L = 0x03U;  // MEAS_RATE_L
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG = 0x04U;     // MEAS_CFG
static const uint8_t XENSIV_PASCO2_REG_CO2PPM_H = 0x05U;     // CO2PPM_H
static const uint8_t XENSIV_PASCO2_REG_CO2PPM_L = 0x06U;     // CO2PPM_L
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS = 0x07U;     // MEAS_STS
static const uint8_t XENSIV_PASCO2_REG_INT_CFG = 0x08U;      // INT_CFG:
static const uint8_t XENSIV_PASCO2_REG_ALARM_TH_H = 0x09U;   // ALARM_TH_H
static const uint8_t XENSIV_PASCO2_REG_ALARM_TH_L = 0x0AU;   // ALARM_TH_L
static const uint8_t XENSIV_PASCO2_REG_PRESS_REF_H = 0x0BU;  // PRESS_REF_H
static const uint8_t XENSIV_PASCO2_REG_PRESS_REF_L = 0x0CU;  // PRESS_REF_L
static const uint8_t XENSIV_PASCO2_REG_CALIB_REF_H = 0x0DU;  // CALIB_REF_H
static const uint8_t XENSIV_PASCO2_REG_CALIB_REF_L = 0x0EU;  // CALIB_REF_L
static const uint8_t XENSIV_PASCO2_REG_SCRATCH_PAD = 0x0FU;  // SCRATCH_PAD
static const uint8_t XENSIV_PASCO2_REG_SENS_RST = 0x10U;     // SENS_RST

static const uint8_t XENSIV_PASCO2_COMM_DELAY_MS = 5U;
static const uint8_t XENSIV_PASCO2_COMM_TEST_VAL = 0xA5U;
static const uint16_t XENSIV_PASCO2_SOFT_RESET_DELAY_MS = 2000U;
static const uint8_t XENSIV_PASCO2_FCS_MEAS_RATE_S = 10;

static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_POS = 0U;  // SENS_STS: ICCER_CLR position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_POS;              // SENS_STS: ICCER_CLR mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_POS = 1U;  // SENS_STS: ORVS_CLR position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_POS;                // SENS_STS: ORVS_CLR mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_POS = 2U;  // SENS_STS: ORTMP_CLR position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_POS;           // SENS_STS: ORTMP_CLR mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ICCER_POS = 3U;  // SENS_STS: ICCER position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ICCER_POS;              // SENS_STS: ICCER mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORVS_POS = 4U;  // SENS_STS: ORVS position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ORVS_POS;                // SENS_STS: ORVS mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORTMP_POS = 5U;  // SENS_STS: ORTMP position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_ORTMP_POS;                    // SENS_STS: ORTMP mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_POS = 6U;  // SENS_STS: PWM_DIS_ST position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_PWM_DIS_ST_POS;            // SENS_STS: PWM_DIS_ST mask
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_POS = 7U;  // SENS_STS: SEN_RDY position
static const uint8_t XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK =
    0x01U << XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_POS;  // SENS_STS: SEN_RDY mask

static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS = 0U;  // MEAS_CFG: OP_MODE position
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_MSK =
    0x03U << XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS;  // MEAS_CFG: OP_MODE mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_IDLE =
    0x00 << XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS;  // MEAS_CFG: OP_MODE_IDLE
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_SINGLESHOT =
    0x01 << XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS;  // MEAS_CFG: OP_MODE_SINGLE_SHOT
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_CONTINOUS =
    0x02 << XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_POS;  // MEAS_CFG: OP_MODE_CONTINOUS

static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS = 2U;  // MEAS_CFG: BOC_CFG position
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_MSK =
    0x03U << XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS;  // MEAS_CFG: BOC_CFG mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_DISABLE =
    0x0 << XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS;  // MEAS_CFG: BOC_CFG DISABLE
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_ENABLE =
    0x1 << XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS;  // MEAS_CFG: BOC_CFG Enable
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_FORCE =
    0x2 << XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_POS;  // MEAS_CFG: BOC_CFG Force

static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS = 4U;  // MEAS_CFG: PWM_MODE position
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS;  // MEAS_CFG: PWM_MODE mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_SINGLE =
    0x00U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS;  // MEAS_CFG: PWM_MODE Single pulse
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_TRAIN =
    0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_MODE_POS;  // MEAS_CFG: PWM_MODE pulse Train

static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS = 5U;  // MEAS_CFG: PWM_OUTEN position
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS;  // MEAS_CFG: PWM_OUTEN mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_EN =
    0x01U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS;  // MEAS_CFG: PWM_OUTEN enable
static const uint8_t XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_DISABLE =
    0x00U << XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_POS;  // MEAS_CFG: PWM_OUTEN enable

static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_POS = 0U;  //  MEAS_STS: ALARM_CLR position
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_POS;                 // MEAS_STS: ALARM_CLR mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_POS = 1U;  // MEAS_STS: INT_STS_CLR position
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_POS;         // MEAS_STS: INT_STS_CLR mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_ALARM_POS = 2U;  // MEAS_STS: ALARM position
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_ALARM_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_STS_ALARM_POS;                 // MEAS_STS: ALARM mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_INT_STS_POS = 3U;  // MEAS_STS: INT_STS position
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_INT_STS_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_STS_INT_STS_POS;            // MEAS_STS: INT_STS mask
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_DRDY_POS = 4U;  // MEAS_STS: DRDY position
static const uint8_t XENSIV_PASCO2_REG_MEAS_STS_DRDY_MSK =
    0x01U << XENSIV_PASCO2_REG_MEAS_STS_DRDY_POS;  // MEAS_STS: DRDY mask

static const uint8_t XENSIV_PASCO2_CMD_SOFT_RESET = 0xA3U;  //  Soft reset the sensor
static const uint8_t XENSIV_PASCO2_CMD_RESET_ABOC = 0xBCU;  // Resets the ABOC context
static const uint8_t XENSIV_PASCO2_CMD_SAVE_FCS_CALIB_OFFSET =
    0xCFU;  // Saves the force calibration offset into the non volatile memory
static const uint8_t XENSIV_PASCO2_CMD_RESET_FCS = 0xFCU;  // Resets the forced calibration correction factor

void PASCO2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up pasco2...");
  // the sensor needs 1000 ms to enter the idle state

  if (this->enable_pin_ != nullptr) {
    // Set enable pin as OUTPUT and disable the enable pin to force vl53 to HW Standby mode
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
    delayMicroseconds(100);
  }

  this->initialize_sensor();
  
  if(this->measurement_mode_ == PERIODIC)
  {
    polling_interval = this->get_update_interval()/1000;  //convert to seconds
    
    if(polling_interval < 5)
    {
      ESP_LOGW(TAG, "Polling interval less than min of 5s, setting to 5s");
    }
    if(polling_interval <= 10)
    {
      ESP_LOGW(TAG, "Datasheet recommends >10s between measurements to allow time for cooling, but interval set to %d",
        polling_interval);
    }
  }
}

void PASCO2Component::initialize_sensor() {
  this->status_clear_error();
  this->remaining_retries_ = 2;  // Reset retries
  this->initialization_state_ = InitializationState::WRITE_SCRATCH_PAD;  // Start state machine
  this->last_action_time_ = millis();  // Initialize timing
}

void PASCO2Component::loop() {
  switch (this->initialization_state_) {
    case InitializationState::IDLE:
      // No action needed, initialization not yet started.
      break;

    case InitializationState::WRITE_SCRATCH_PAD:
      if (millis() - this->last_action_time_ >= 100) {
        if (!this->write_byte(XENSIV_PASCO2_REG_SCRATCH_PAD, XENSIV_PASCO2_COMM_TEST_VAL, true)) {
          if (--this->remaining_retries_ == 0) {
            ESP_LOGE(TAG, "Failed to Write Scratch Pad");
            this->error_code_ = COMM_FAILED;
            this->mark_failed();
            this->initialization_state_ = InitializationState::COMPLETE;        // End state
          }
        } else {
          this->remaining_retries_ = 2;                                         // Reset retries on success
          this->initialization_state_ = InitializationState::READ_SCRATCH_PAD;  // Move to next state
        }
        this->last_action_time_ = millis();  // Update timing
      }
      break;

    case InitializationState::READ_SCRATCH_PAD:
      if (millis() - this->last_action_time_ >= 100) {
        uint8_t read_back[2];
        if (!this->read_bytes(XENSIV_PASCO2_REG_SCRATCH_PAD, &read_back[0], 1)) {
          ESP_LOGE(TAG, "Failed to read Scratch Pad");
          this->error_code_ = COMM_FAILED;
          this->mark_failed();
          this->initialization_state_ = InitializationState::COMPLETE;  // End state
        } else if (read_back[0] != XENSIV_PASCO2_COMM_TEST_VAL) {
          ESP_LOGE(TAG, "Scratch Pad Does not Match");
          this->error_code_ = COMM_FAILED;
          this->mark_failed();
          this->initialization_state_ = InitializationState::COMPLETE;  // End state
        } else {
          this->initialization_state_ = InitializationState::SOFT_RESET;  // Move to soft reset state
        }
        this->last_action_time_ = millis();  // Update timing
      }
      break;

    case InitializationState::SOFT_RESET:
      if (millis() - this->last_action_time_ >= 100) {
        if (!this->write_byte(XENSIV_PASCO2_REG_SENS_RST, XENSIV_PASCO2_CMD_SOFT_RESET, true)) {
          ESP_LOGE(TAG, "Error Sending Soft Reset.");
          this->error_code_ = SOFT_RESET_FAILED;
          this->mark_failed();
          this->initialization_state_ = InitializationState::COMPLETE;  // End state
        } else {
          this->last_action_time_ = millis();  // Update timing
          this->initialization_state_ = InitializationState::READ_STATUS;  // Move to read status state
        }
      }
      break;

    case InitializationState::READ_STATUS:
      if (millis() - this->last_action_time_ >= 2500) {
        uint8_t read_back[2];
        if (!this->read_bytes(XENSIV_PASCO2_REG_SENS_STS, &read_back[0], 1)) {
          ESP_LOGE(TAG, "Error Reading Status Reg.");
          this->error_code_ = SOFT_RESET_FAILED;
          this->mark_failed();
          this->initialization_state_ = InitializationState::COMPLETE;  // End state
        } else {
          // Check status and handle errors
          handle_status_errors(read_back[0]);
          if (this->initialization_state_ != InitializationState::COMPLETE) {
            this->initialized_ = true;    // Mark as initialized
            this->start_measurement_();   // Start measurements
            ESP_LOGD(TAG, "Sensor initialized");
            this->initialization_state_ = InitializationState::COMPLETE;  // End state
          }
        }
        this->last_action_time_ = millis();  // Update timing
      }
      break;

    case InitializationState::COMPLETE:
      // No action needed, initialization complete
      break;
  }
  
  //read the sensor if in continuous mode
  if(this->initialized_ && this->measurement_mode_ == PERIODIC)
  {
    int16_t co2result;
    if(read_sensor_(&co2result))
    {
      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(co2result);

      ESP_LOGD(TAG, "Read Co2 level %d ppm", co2result);
    }
  }
}

bool PASCO2Component::read_sensor_(int16_t *co2result)
{
   // Check if data is ready
    uint8_t read_back;
 
    int errorCode = this->read_register(XENSIV_PASCO2_REG_MEAS_STS, &read_back, 1, true);   
    switch (errorCode) {
      case i2c::ErrorCode::NO_ERROR:
        break;
      case i2c::ErrorCode::ERROR_NOT_ACKNOWLEDGED:
        //device does not ACK reads during the measurement cycle in periodic mode, so not an error, retry after a delay
        return false;
      default:
        //should not happen
        ESP_LOGW(TAG, "MEAS_STS Reading Failed: error %d", errorCode);
        this->status_set_warning(); //this is an actual problem
        return false;
    }
    
    if (read_back & XENSIV_PASCO2_REG_MEAS_STS_DRDY_MSK) {
      if (!this->read_byte_16(XENSIV_PASCO2_REG_CO2PPM_H, (uint16_t *)co2result)) {
        ESP_LOGW(TAG, "Result Reading Failed, retrying next loop!");
        return false;
      }
    } else {
      ESP_LOGW(TAG, "Data not ready yet, retrying next loop!");
      return false;
    }
    
    //data is valid
    return true;
}

void PASCO2Component::handle_status_errors(uint8_t status) {
  if ((status & XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK) != 0U) {
    ESP_LOGE(TAG, "Error XENSIV_PASCO2_REG_SENS_STS_ICCER_MSK");
    this->error_code_ = XENSIV_PASCO2_ICCERR;
    this->mark_failed();
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK) != 0U) {
    ESP_LOGE(TAG, "Error XENSIV_PASCO2_REG_SENS_STS_ORVS_MSK");
    this->error_code_ = XENSIV_PASCO2_ORVS;
    this->mark_failed();
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK) != 0U) {
    ESP_LOGE(TAG, "Error XENSIV_PASCO2_REG_SENS_STS_ORTMP_MSK");
    this->error_code_ = XENSIV_PASCO2_ORTMP;
    this->mark_failed();
  } else if ((status & XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK) == 0U) {
    ESP_LOGE(TAG, "Error XENSIV_PASCO2_REG_SENS_STS_SEN_RDY_MSK");
    this->error_code_ = XENSIV_PASCO2_ERR_NOT_READY;
    this->mark_failed();
  }
}

void PASCO2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "pasco2:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMM_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case SOFT_RESET_FAILED:
        ESP_LOGW(TAG, "Sensor Reset failed!");
        break;
      case XENSIV_PASCO2_ICCERR:
        ESP_LOGW(TAG, "Sensor ICCERR!");
        break;
      case XENSIV_PASCO2_ORVS:
        ESP_LOGW(TAG, "Sensor ORVS Error!");
        break;
      case XENSIV_PASCO2_ORTMP:
        ESP_LOGW(TAG, "Sensor ORTMP Error!");
        break;
      case XENSIV_PASCO2_ERR_NOT_READY:
        ESP_LOGW(TAG, "Sensor Not Ready Error!");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Automatic self calibration: %s", ONOFF(this->enable_asc_));
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using sensor '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else {
    if (this->ambient_pressure_compensation_) {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %dmBar", this->ambient_pressure_);
    } else {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation disabled");
    }
  }
  switch (this->measurement_mode_) {
    case PERIODIC:
      ESP_LOGCONFIG(TAG, "  Measurement mode: periodic (%d)", polling_interval);
      break;
    case SINGLE_SHOT:
      ESP_LOGCONFIG(TAG, "  Measurement mode: single shot");
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

void PASCO2Component::update() {
  if (!initialized_ || calibrating_) {
    return;
  }

  if (this->ambient_pressure_source_ != nullptr) {
    float pressure = this->ambient_pressure_source_->state;
    if (!std::isnan(pressure)) {
      set_ambient_pressure_compensation(pressure);
    }
  }

  if (this->measurement_mode_ == PERIODIC) {
    return; //handle this case in loop()
  }

  //the sensor will not respond to I2C commands for approximately 1 second during measurement cycles. To avoid missing
  //  data, reread if it doesn't respond
  uint32_t wait_time = 1500;  
  if (this->measurement_mode_ == SINGLE_SHOT) {
    start_measurement_();
    wait_time = 1500;  // Single shot measurement takes ~1 sec
  }
  
  set_retry(wait_time, 3, [this](const uint8_t remaining_attempts) {   
    
    int16_t co2result;
    if(read_sensor_(&co2result))
    {
      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(co2result);
      
      ESP_LOGD(TAG, "Read Co2 level %d ppm", co2result);
    } else {
      return  RetryResult::RETRY;
    }
    
    this->status_clear_warning();
    return RetryResult::DONE;
  }, 1.0f);  // set_retry
}

bool PASCO2Component::perform_forced_calibration(uint16_t current_co2_concentration) {
  calibrating_ = true;
  
  //set device to idle 
  if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG, XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_IDLE, true)) {
    ESP_LOGE(TAG, "Failed to stop measurements");
    this->status_set_warning();
  }
  //set update period to 10s per calibration guide
  if (!this->write_byte_16(XENSIV_PASCO2_REG_MEAS_RATE_H, (uint16_t)XENSIV_PASCO2_FCS_MEAS_RATE_S)){
    ESP_LOGW(TAG, "Failed to set measurement period to 10s");
    this->status_set_warning();
  }
  //load calibration value 
	if (this->write_byte_16(XENSIV_PASCO2_REG_CALIB_REF_H, current_co2_concentration)) {
      ESP_LOGD(TAG, "setting forced calibration CO2 level %d ppm", current_co2_concentration);
	}
   //set force calibration flag, this starts the calibration process and data acquisition
  if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG, XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_FORCE | 
      XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_CONTINOUS, true)) {
    ESP_LOGE(TAG, "Failed to force calibration");
    this->status_set_warning();
  }  
  //the arduino example code doesn't actually read the values back, it just waits for 
  //    XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_FORCE to be cleared.
  set_retry(6200, 10, [this](const uint8_t remaining_attempts) {
		uint8_t read_back = 0;
		
		if (!this->read_bytes(XENSIV_PASCO2_REG_MEAS_CFG, &read_back, 1)) {
		  return  RetryResult::RETRY;
		}
    
		if(read_back & XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_FORCE)
    {
      ESP_LOGD(TAG, "Calibrating...");
      
      if(remaining_attempts ==0)
      {
        ESP_LOGD(TAG, "calibration timed out!");
        this->status_set_warning();
      }
      return  RetryResult::RETRY;
    }
    
    if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG,XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_IDLE, true)) {
      ESP_LOGE(TAG, "Failed to idle measurement");
      this->status_set_warning();
    }
    
    //store calibraiton data to NVRAM
    if (!this->write_byte(XENSIV_PASCO2_REG_SENS_RST, XENSIV_PASCO2_CMD_SAVE_FCS_CALIB_OFFSET, true)) {
      ESP_LOGE(TAG, "Failed to save calibration to NVRAM");
      this->status_set_warning();
    }  

    ESP_LOGD(TAG, "calibration complete!");
        
    //restart acquiring co2 data using new calibration, delay seems to reduce odds chip doesn't respond   
    this->set_timeout(100, [this]() { start_measurement_();});
    this->set_timeout(200, [this]() { calibrating_ = false;});

		return RetryResult::DONE;
	}, 1.0f);

  return true;
}

void PASCO2Component::set_ambient_pressure_compensation(float pressure_in_hpa) {
  ambient_pressure_compensation_ = true;
  uint16_t new_ambient_pressure = (uint16_t) pressure_in_hpa;
  if (!initialized_) {
    ambient_pressure_ = new_ambient_pressure;
    return;
  }
  // Only send pressure value if it has changed since last update
  if (new_ambient_pressure != ambient_pressure_) {
    update_ambient_pressure_compensation_(new_ambient_pressure);
    ambient_pressure_ = new_ambient_pressure;
  } else {
    ESP_LOGD(TAG, "ambient pressure compensation skipped - no change required");
  }
}

bool PASCO2Component::update_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  if (this->write_byte_16(XENSIV_PASCO2_REG_PRESS_REF_H, pressure_in_hpa)) {
    ESP_LOGD(TAG, "setting ambient pressure compensation to %d hPa", pressure_in_hpa);
    return true;
  } else {
    ESP_LOGE(TAG, "Error setting ambient pressure compensation.");
    return false;
  }
}

bool PASCO2Component::start_measurement_() {
  uint8_t measurement_command = XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_CONTINOUS |
                                XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_ENABLE | XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_EN;
  if (this->measurement_mode_ == SINGLE_SHOT) {
    measurement_command = XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_SINGLESHOT | XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_ENABLE |
                          XENSIV_PASCO2_REG_MEAS_CFG_PWM_OUTEN_EN;
  }

  if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG,
                        XENSIV_PASCO2_REG_MEAS_CFG_OP_MODE_IDLE | XENSIV_PASCO2_REG_MEAS_CFG_BOC_CFG_ENABLE, true)) {
    ESP_LOGE(TAG, "Failed to stop measurements");
    this->status_set_warning();
  }

  //set the measurement rate if in periodic mode
  if (this->measurement_mode_ == PERIODIC && !this->write_byte_16(XENSIV_PASCO2_REG_MEAS_RATE_H, polling_interval)) {
    ESP_LOGE(TAG, "Setting Measurement Rate Failed!");
    return false;
  }    
  }

  static uint8_t remaining_retries = 3;
  while (remaining_retries) {
    if (!this->write_byte(XENSIV_PASCO2_REG_MEAS_CFG, measurement_command, true)) {
      ESP_LOGE(TAG, "Error starting measurements.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->status_set_warning();
      if (--remaining_retries == 0)
        return false;
      delay(50);  // NOLINT wait 50 ms and try again
    }
    this->status_clear_warning();
    return true;
  }
  return false;
}

}  // namespace pasco2
}  // namespace esphome
