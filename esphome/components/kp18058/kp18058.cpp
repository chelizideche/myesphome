#include "kp18058.h"

namespace esphome {
namespace kp18058 {

static const uint8_t KP18058_DELAY = 4;
static const char *const TAG = "kp18058";

#define BIT_CHECK(PIN,N) !!((PIN & (1<<N)))

void usleep(int r)
{
    // delay function do 10*r nops, because rtos_delay_milliseconds is too much
    for (volatile int i = 0; i < r; i++)
        __asm__("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");
}


void kp18058::setup()
{
    this->data_pin_->setup();
    this->clock_pin_->setup();
    delayMicroseconds(KP18058_DELAY);
    this->data_pin_->digital_write(false);
    delayMicroseconds(KP18058_DELAY);
    this->clock_pin_->digital_write(false);
}

void kp18058::dump_config()
{
    ESP_LOGCONFIG(TAG, "KP18058 LED Driver:");
    LOG_PIN(           "  Data Pin: ", this->data_pin_);
    LOG_PIN(           "  Clock Pin: ", this->clock_pin_);
    ESP_LOGCONFIG(TAG, "  cw_current: %d", this->max_cw_current_);
    ESP_LOGCONFIG(TAG, "  rgb_current: %d", this->max_rgb_current_);
}


uint8_t GetParityBit(uint8_t b) {
    uint8_t sum;
    int i;

    sum = 0;
    for (i = 1; i < 8; i++) {
        if (BIT_CHECK(b, i)) {
            sum++;
        }
    }
    if (sum % 2 == 0) {
        return 0;
    }
    return 1;
}

void kp18058::program_led_driver()
{
    bool bAllZero = true;

    for (int i = 0; i < 5; i++) {
        if (channels[i]->get_value() > 0) {
            bAllZero = false;
        }
    }

    // RGB current
    uint8_t byte2 = (channels[0]->get_value() ||
                  channels[1]->get_value() ||
                  channels[2]->get_value())   ? max_rgb_current_ : 1;
    byte2 = byte2 << 1;
    byte2 |= GetParityBit(byte2);

    // Bit 7: RGB PWM, Bit 6: Unknown, Bit 5-1: CW current
    uint8_t byte3 = (1 << 7) | (1 << 6) | (max_cw_current_ << 1);
    byte3 |= GetParityBit(byte3);

    if (bAllZero) {
        Soft_I2C_Start(0x81);
        Soft_I2C_WriteByte(0x00);
        Soft_I2C_WriteByte(0x03);
        Soft_I2C_WriteByte(byte3);
        for (int i = 0; i < 10; i++)
            Soft_I2C_WriteByte(0x00);
    }
    else {
        Soft_I2C_Start(0xE1);
        Soft_I2C_WriteByte(0x00);
        Soft_I2C_WriteByte(byte2);
        Soft_I2C_WriteByte(byte3);
        for (int i = 0; i < 5; i++)
        {
            uint16_t useVal = channels[i]->get_value();

            uint8_t Byte_A;
            Byte_A =  useVal & 0x1F;
            Byte_A = Byte_A << 1;
            Byte_A |= GetParityBit(Byte_A);

            uint8_t Byte_B;
            Byte_B = (useVal >> 5) & 0x1F;
            Byte_B = Byte_B << 1;
            Byte_B |= GetParityBit(Byte_B);

            Soft_I2C_WriteByte(Byte_B);
            Soft_I2C_WriteByte(Byte_A);
        }
    }
    Soft_I2C_Stop();
    return;
}


void Soft_I2C_SetLow(InternalGPIOPin *pin) {
//  HAL_PIN_Setup_Output(pin);
//  HAL_PIN_SetOutputValue(pin, 0);
    pin->digital_write(false);
}

void Soft_I2C_SetHigh(InternalGPIOPin *pin) {
//  HAL_PIN_Setup_Input_Pullup(pin);
    pin->digital_write(true);
}

bool kp18058::Soft_I2C_WriteByte(uint8_t value)
{
    uint8_t curr;
    uint8_t ack = 0;

    for (curr = 0x80; curr != 0; curr >>= 1) {
        if (curr & value) {
            Soft_I2C_SetHigh(data_pin_);
        }
        else {
            Soft_I2C_SetLow(data_pin_);
        }
        Soft_I2C_SetHigh(clock_pin_);
        usleep(KP18058_DELAY);
        Soft_I2C_SetLow(clock_pin_);
    }

    // get Ack or Nak
    Soft_I2C_SetHigh(data_pin_);
    Soft_I2C_SetHigh(clock_pin_);
    usleep(KP18058_DELAY / 2);
// ack = HAL_PIN_ReadDigitalInput(data_pin_);
    Soft_I2C_SetLow(clock_pin_);
    usleep(KP18058_DELAY / 2);
    Soft_I2C_SetLow(data_pin_);
    return (0 == ack);
}

bool kp18058::Soft_I2C_Start(uint8_t addr) {
    Soft_I2C_SetLow(data_pin_);
    usleep(KP18058_DELAY);
    Soft_I2C_SetLow(clock_pin_);
    return Soft_I2C_WriteByte(addr);
}

void kp18058::Soft_I2C_Stop() {
    Soft_I2C_SetLow(data_pin_);
    usleep(KP18058_DELAY);
    Soft_I2C_SetHigh(clock_pin_);
    usleep(KP18058_DELAY);
    Soft_I2C_SetHigh(data_pin_);
    usleep(KP18058_DELAY);
}


}  // namespace kp18058
}  // namespace esphome



