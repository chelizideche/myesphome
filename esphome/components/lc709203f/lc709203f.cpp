#include "esphome/core/log.h"
#include "lc709203f.h"

namespace esphome {
namespace lc709203f {

static const char *TAG = "lc709203f.sensor";

static const uint8_t LC709203F_I2C_ADDR_DEFAULT         = 0x0B;

static const uint8_t LC709203F_BEFORE_RSOC              = 0x04;
static const uint8_t LC709203F_THERMISTOR_B             = 0x06;
static const uint8_t LC709203F_INITIAL_RSOC             = 0x07;
static const uint8_t LC709203F_CELL_TEMPERATURE         = 0x08;
static const uint8_t LC709203F_CELL_VOLTAGE             = 0x09;
static const uint8_t LC709203F_CURRENT_DIRECTION        = 0x0A;
static const uint8_t LC709203F_APA                      = 0x0B;
static const uint8_t LC709203F_APT                      = 0x0C;
static const uint8_t LC709203F_RSOC                     = 0x0D;
static const uint8_t LC709203F_ITE                      = 0x0F;
static const uint8_t LC709203F_IC_VERSION               = 0x11;
static const uint8_t LC709203F_CHANGE_OF_THE_PARAMETER  = 0x12;
static const uint8_t LC709203F_ALARM_LOW_RSOC           = 0x13;
static const uint8_t LC709203F_ALARM_LOW_CELL_VOLTAGE   = 0x14;
static const uint8_t LC709203F_IC_POWER_MODE            = 0x15;
static const uint8_t LC709203F_STATUS_BIT               = 0x16;
static const uint8_t LC709203F_NUMBER_OF_THE_PARAMETER  = 0x1A;

static const uint8_t LC709203F_POWER_MODE_ON            = 0x0001;
static const uint8_t LC709203F_POWER_MODE_SLEEP         = 0x0002;

static const uint8_t LC709203F_STATE_INIT               = 0x01;
static const uint8_t LC709203F_STATE_RSOC               = 0x02;
static const uint8_t LC709203F_STATE_NORMAL             = 0x00;

void lc709203f::setup(){
    ESP_LOGCONFIG(TAG, "Setting up LC709203F...");
    uint16_t buffer;

    if (!this->GetRegister(LC709203F_IC_VERSION, &buffer)) {
        ESP_LOGD(TAG, "I2C Failed");
    }
    
    //Set power mode to on
    //Note: in sleep mode, the IC does not record power usage, so the capacity info probably gets bad.
    this->SetRegister(LC709203F_IC_POWER_MODE, LC709203F_POWER_MODE_ON);
    this->SetRegister(LC709203F_APA, this->APA_);
    this->SetRegister(LC709203F_CHANGE_OF_THE_PARAMETER, 0x0001);
    
    this->State_ = LC709203F_STATE_INIT;
    //Note: Initialization continues in the update() function.

}

void lc709203f::update(){
    uint16_t buffer;
    
    //this->GetRegister(LC709203F_THERMISTOR_B, &buffer);
    //ESP_LOGD(TAG, "Status: 0x%04X", buffer);
    
    if (this->State_ == LC709203F_STATE_NORMAL)
    {
        if (this->voltage_sensor_ != nullptr) 
        {
            this->GetRegister(LC709203F_CELL_VOLTAGE, &buffer);     //Raw units are mV
            this->voltage_sensor_->publish_state(static_cast<float>(buffer)/1000.0);
            this->status_clear_warning();
            //ESP_LOGD(TAG, "Voltage: %4.2f V", static_cast<float>(buffer)/1000.0);
        }
        if (this->battery_remaining_sensor_ != nullptr) 
        {
            this->GetRegister(LC709203F_ITE, &buffer);      //Raw units are .1%
            this->battery_remaining_sensor_->publish_state(static_cast<float>(buffer)/10.0);
            this->status_clear_warning();
            //ESP_LOGD(TAG, "Battery: %4.1f %%", static_cast<float>(buffer)/10.0);
        }
        if (this->temperature_sensor_ != nullptr) 
        {
            //I can't test this with a real thermistor because I don't have a device with
            // an attached thermistor. I have turned on the sensor and made sure that it
            // set up the registers properly.
            this->GetRegister(LC709203F_CELL_TEMPERATURE, &buffer);     //Raw units are .1 K
            this->temperature_sensor_->publish_state( (static_cast<float>(buffer)/10.0) - 273.15 );
            this->status_clear_warning();
            //ESP_LOGD(TAG, "Temperature: %4.2f C", (static_cast<float>(buffer)/10.0) - 273.15 );
        }
    }
    else if (this->State_ == LC709203F_STATE_INIT)
    {
        //We implement a delay here to send the initial RSOC command.
        // This should run once on the first update() after initialization.
        this->State_ = LC709203F_STATE_RSOC;
        this->SetRegister(LC709203F_INITIAL_RSOC, 0xAA55);
    }
    else if (this->State_ == LC709203F_STATE_RSOC)
    {
        //This should run once on the second update() after initialization.
        this->State_ = LC709203F_STATE_NORMAL;
        if (this->temperature_sensor_ != nullptr)
        {
            //This assumes that a thermistor is attached to the device as shown in the datahseet.
            this->SetRegister(LC709203F_STATUS_BIT, 0x0001);
            this->SetRegister(LC709203F_THERMISTOR_B, this->B_Constant_);
        }
        else
        {
            //The device expects to get updates to the temperature in this mode. 
            // I am not doing that now. The temperature register defaults to 25C.
            // In theory, we could have another temperature sensor and have ESPHome 
            // send updated temperature to the device occasionally, but I have no idea 
            // how to make that happen.
            this->SetRegister(LC709203F_STATUS_BIT, 0x0000);
        }
    }
}

void lc709203f::dump_config(){
    ESP_LOGCONFIG(TAG, "LC709203F:");
    LOG_I2C_DEVICE(this);
    //if (this->is_failed()) {
    //    ESP_LOGE(TAG, "Communication with LC709203F failed");
    //}
    LOG_UPDATE_INTERVAL(this);
    ESP_LOGCONFIG(TAG, "  Pack Size: %d mAH", this->pack_size_);
    ESP_LOGCONFIG(TAG, "  Pack APA: 0x%02X", this->APA_);
    
    LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
    LOG_SENSOR("  ", "Battery Remaining", this->battery_remaining_sensor_);
    
    if (this->temperature_sensor_ != nullptr)
    {
        LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
        ESP_LOGCONFIG(TAG, "    B_Constant: %d", this->B_Constant_);
    }
    else
    {
        ESP_LOGCONFIG(TAG, "  No Temperature Sensor");
    }
}

uint8_t lc709203f::GetRegister(uint8_t RegisterToRead, uint16_t *RegisterValue)
{
    uint8_t ReadBuffer[6];
    
    ReadBuffer[0] = (this->address_) << 1;
    ReadBuffer[1] = RegisterToRead;
    ReadBuffer[2] = ((this->address_) << 1) | 0x01;
    
    //Note: the read_register() function does not send a stop between the write and
    // the read portions of the I2C transation when you set the last variable to 'false' 
    // as we do below. Some of the other I2C read functions such as the generic read() 
    // function will send a stop between the read and the write portion of the I2C transaction. 
    // This is bad in this case and will result in reading nothing but 0xFFFF from the registers.
    if (this->read_register(RegisterToRead, &ReadBuffer[3], 3, false) != 0) 
    {
        ESP_LOGD(TAG, "I2C Failed");
    }
    
    if(this->CRC8(ReadBuffer, 5) != ReadBuffer[5])
    {
        ESP_LOGD(TAG, "CRC Mismatch");
    }
    
    *RegisterValue = ((uint16_t)ReadBuffer[4] << 8) | (uint16_t)ReadBuffer[3];
    //TODO: Do error checking/retry here?
    return 0;
}

uint8_t lc709203f::SetRegister(uint8_t RegisterToSet, uint16_t ValueToSet)
{
    uint8_t WriteBuffer[5];
    
    WriteBuffer[0] = (this->address_) << 1;     //We don't actually send this byte, but it is part of the CRC calculation.
    WriteBuffer[1] = RegisterToSet;
    WriteBuffer[2] = ValueToSet&0xFF;           //Low byte
    WriteBuffer[3] = (ValueToSet >> 8) & 0xFF;  //High byte
    WriteBuffer[4] = this->CRC8(WriteBuffer, 4);
    
    //TODO: Do error checking/retry here?
    return (this->write(&WriteBuffer[1], 4, true));
}

uint8_t lc709203f::CRC8(uint8_t *ByteBuffer, uint8_t LengthOfCRC)
{
    //uint8_t len = 5;
    uint8_t crc = 0x00;
    const uint8_t POLYNOMIAL(0x07);

    for (int j = LengthOfCRC; j; --j) 
    {
        crc ^= *ByteBuffer++;

        for (int i = 8; i; --i) 
        {
            crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
        }
    }
    
  return crc;
}

void lc709203f::set_pack_size(uint16_t PackSize)
{
    uint16_t PackSizeArray[6] = {100, 200, 500, 1000, 2000, 3000};
    uint16_t APAArray[6] = {0x08, 0x0B, 0x10, 0x19, 0x2D, 0x36};
    float slope;
    float intercept;
    
    this->pack_size_ = PackSize;    //Pack size in mAH
    
    //The size is used to calculate the 'Adjustment Pack Application' number.
    //Here we assume a type 01 or type 03 battery and do a linear curve fit to find the APA.
    for (uint8_t i = 0; i < sizeof(PackSizeArray)/sizeof(PackSizeArray[0]); i++)
    {
        if (PackSizeArray[i] == PackSize)
        {
            this->APA_ = APAArray[i];
            return;
        }
        else if((i > 0) && (PackSizeArray[i] > PackSize) && (PackSizeArray[i-1] < PackSize))
        {
            slope = static_cast<float>(APAArray[i] - APAArray[i-1])/static_cast<float>(PackSizeArray[i] - PackSizeArray[i-1]);     //Type casting is required here to avoid interger division
            intercept = static_cast<float>(APAArray[i])-slope*static_cast<float>(PackSizeArray[i]);     //Type casting might not be needed here.
            this->APA_ = static_cast<uint8_t>(slope*PackSize+intercept);
            return;
        }
    }
    //We should never get here
    //TODO: set failed here?
}

void lc709203f::set_thermistor_B_constant(uint16_t B_Constant)
{
    this->B_Constant_ = B_Constant;
    return;
}

}  // namespace lc709203f
}  // namespace esphome
