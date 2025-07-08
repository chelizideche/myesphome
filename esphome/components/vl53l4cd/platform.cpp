#include "platform.h"
#include <Wire.h>

#define I2C_TIMEOUT_MS 100

uint8_t VL53L4CD_WriteMulti(Dev_t dev, uint16_t reg_index, uint8_t *pdata, uint32_t count) {
  Wire.beginTransmission(dev);  // dev 是 I2C 7-bit 地址，例如 0x29
  Wire.write((reg_index >> 8) & 0xFF);  // 高字节
  Wire.write(reg_index & 0xFF);        // 低字节
  for (uint32_t i = 0; i < count; i++) {
    Wire.write(pdata[i]);
  }
  return Wire.endTransmission() == 0 ? 0 : 255;
}

uint8_t VL53L4CD_ReadMulti(Dev_t dev, uint16_t reg_index, uint8_t *pdata, uint32_t count) {
  Wire.beginTransmission(dev);
  Wire.write((reg_index >> 8) & 0xFF);  // 高字节
  Wire.write(reg_index & 0xFF);        // 低字节
  if (Wire.endTransmission(false) != 0) {
    return 255;  // NACK
  }

  Wire.requestFrom((int)dev, (int)count);
  uint32_t i = 0;
  while (Wire.available() && i < count) {
    pdata[i++] = Wire.read();
  }
  return (i == count) ? 0 : 255;
}

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t RegisterAddress, uint32_t *value) {
  uint8_t buffer[4];
  uint8_t status = VL53L4CD_ReadMulti(dev, RegisterAddress, buffer, 4);
  *value = (uint32_t)buffer[0] << 24 | (uint32_t)buffer[1] << 16 |
           (uint32_t)buffer[2] << 8 | buffer[3];
  return status;
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t RegisterAddress, uint16_t *value) {
  uint8_t buffer[2];
  uint8_t status = VL53L4CD_ReadMulti(dev, RegisterAddress, buffer, 2);
  *value = (uint16_t)buffer[0] << 8 | buffer[1];
  return status;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t RegisterAddress, uint8_t *value) {
  return VL53L4CD_ReadMulti(dev, RegisterAddress, value, 1);
}

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t RegisterAddress, uint8_t value) {
  return VL53L4CD_WriteMulti(dev, RegisterAddress, &value, 1);
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t RegisterAddress, uint16_t value) {
  uint8_t buffer[2] = {
      (uint8_t)(value >> 8),
      (uint8_t)(value & 0xFF),
  };
  return VL53L4CD_WriteMulti(dev, RegisterAddress, buffer, 2);
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t RegisterAddress, uint32_t value) {
  uint8_t buffer[4] = {
      (uint8_t)(value >> 24),
      (uint8_t)((value >> 16) & 0xFF),
      (uint8_t)((value >> 8) & 0xFF),
      (uint8_t)(value & 0xFF),
  };
  return VL53L4CD_WriteMulti(dev, RegisterAddress, buffer, 4);
}

uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs) {
  delay(TimeMs);
  return 0;
}