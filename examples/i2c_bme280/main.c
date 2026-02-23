// Copyright (c) 2024, Myriota Pty Ltd, All Rights Reserved
// SPDX-License-Identifier: BSD-3-Clause-Attribution
//
// This file is licensed under the BSD with attribution  (the "License"); you
// may not use these files except in compliance with the License.
//
// You may obtain a copy of the License here:
// LICENSE-BSD-3-Clause-Attribution.txt and at
// https://spdx.org/licenses/BSD-3-Clause-Attribution.html
//
// See the License for the specific language governing permissions and
// limitations under the License.

// An example running on Myriota's "FlexSense" board.
// This example demonstrates the FlexSense Device interfacing with a
// BME280 sensor using the I2C interface on the FlexSense External
// Interface Cable (Only implement I2C communication with a cable length
// of 1m or less).
//! [CODE]

#include <stdio.h>
#include <string.h>
#include "flex.h"

#define APPLICATION_NAME "I2C BME280 Example"
#define MESSAGES_PER_DAY (4)

// BME Values
#define BME280_I2C_ADDRESS (0x77)
#define BME280_ID (0x60)
#define BME280_INIT_VALUE (0x00)
#define BME280_SUCCESS_VALUE (0)
#define BME280_FAIL_VALUE (-1)
#define BME280_REGISTER_READ_1MS_DELAY (1)
#define BME280_ID_READ_COUNT_MAX (5)

// BME280 Registers
#define BME280_REG_ID (0xD0)
#define BME280_REG_CONFIG (0xF5)
#define BME280_REG_CTRL_MEAS (0xF4)
#define BME280_REG_HUMIDITY (0xF2)
#define BME280_REG_HUMIDITY_LSB (0xFE)
#define BME280_REG_HUMIDITY_MSB (0xFD)

// BME280 Configuration Values
#define BME280_TEMPERATURE_CONFIG_RESERVED_MASK (0x02)
#define BME280_HUMIDITY_CONFIG_RESERVED_MASK (0xF8)
// No standby time as we will be in forced mode, no IIR filter and no 3-wire SPI
#define BME280_TEMPERATURE_CONFIG (0x00)
// oversampling set to 1
#define BME280_HUMIDITY_CONFIG (0x01)
// acquisition settings for temperature sampling and forced mode.
#define BME280_ACQUISITION_CONFIG (0x21)

// message structure
typedef struct {
  uint16_t sequence_number;
  uint16_t humidity;
  uint32_t time;
} __attribute__((packed)) message;

// Perform an I2C read.
static int ReadRegister8(uint8_t reg) {
  uint8_t rx;

  if (FLEX_ExtI2CRead(BME280_I2C_ADDRESS, &reg, sizeof(reg), &rx, sizeof(rx)) == 0) {
    return rx;
  }
  return BME280_FAIL_VALUE;
}

// Perform an I2C write.
static int WriteRegister8(uint8_t reg, uint8_t value) {
  uint8_t tx[2];
  tx[0] = reg;
  tx[1] = value;

  if (FLEX_ExtI2CWrite(BME280_I2C_ADDRESS, tx, sizeof(tx)) == 0) {
    return BME280_SUCCESS_VALUE;
  }
  return BME280_FAIL_VALUE;
}

// Configure the temperature measuring behaviour.
static int SetTemperatureConfig(void) {
  int currentValue = ReadRegister8(BME280_REG_CONFIG);

  if (currentValue == BME280_FAIL_VALUE) {
    return BME280_FAIL_VALUE;
  }
  // Isolate the reserved bit and add the desired temperature settings.
  uint8_t desiredValue =
    (currentValue & BME280_TEMPERATURE_CONFIG_RESERVED_MASK) | BME280_TEMPERATURE_CONFIG;

  return WriteRegister8(BME280_REG_CONFIG, desiredValue);
}

// Configure the humidity measuring behaviour.
static int SetHumidityConfig() {
  int currentValue = ReadRegister8(BME280_REG_HUMIDITY);

  if (currentValue == BME280_FAIL_VALUE) {
    return BME280_FAIL_VALUE;
  }
  // retain [7:3] and set the humidity config [2:0]
  uint8_t humidityConfig =
    (currentValue & BME280_HUMIDITY_CONFIG_RESERVED_MASK) | BME280_HUMIDITY_CONFIG;
  return WriteRegister8(BME280_REG_HUMIDITY, humidityConfig);
}

// Configure the data acquisition config. This required to trigger a read in forced mode.
static int SetDataAcquisition() {
  return WriteRegister8(BME280_REG_HUMIDITY, BME280_ACQUISITION_CONFIG);
}

// Read the Humidity from the BME sensor
static time_t ReadHumidity() {
  static uint16_t sequence_number = 0;
  if (SetDataAcquisition() == BME280_FAIL_VALUE) {
    printf("Failed to trigger a read!\n");
    return (FLEX_TimeGet() + 24 * 3600 / MESSAGES_PER_DAY);
  }

  message message;

  int humidityLsb = ReadRegister8(BME280_REG_HUMIDITY_LSB);
  int humidityMsb = ReadRegister8(BME280_REG_HUMIDITY_MSB);

  if (humidityLsb == BME280_FAIL_VALUE || humidityMsb == BME280_FAIL_VALUE) {
    printf("Failed to read humidity!\n");
    return (FLEX_TimeGet() + 24 * 3600 / MESSAGES_PER_DAY);
  }

  message.sequence_number = sequence_number++;
  message.humidity = (humidityMsb << 8) | humidityLsb;
  message.time = FLEX_TimeGet();

  FLEX_MessageSchedule((void *)&message, sizeof(message));
  printf("Scheduled message: Uncompensated Humidity: %u  Time: %lu Seq Num: %u\n",
    message.humidity / 1024, message.time, message.sequence_number);

  return (FLEX_TimeGet() + 24 * 3600 / MESSAGES_PER_DAY);
}

// Initialise the BME sensor
static int Init(void) {
  int retVal = BME280_FAIL_VALUE;
  uint8_t deviceid = BME280_INIT_VALUE;
  uint8_t idReadCount = BME280_ID_READ_COUNT_MAX;

  while (idReadCount > 0) {
    // Read the deviceID
    deviceid = ReadRegister8(BME280_REG_ID);
    // If the register returns the expected deviceID we can continue.
    if (deviceid == BME280_ID) {
      break;
    }
    // Decrement the number of read attempts remaining.
    idReadCount--;
    /* Delay added concerning the low speed of power up system to
    facilitate the proper reading of the chip ID */
    FLEX_DelayMs(BME280_REGISTER_READ_1MS_DELAY);
  }

  // Module ID read failed.
  retVal = (idReadCount == BME280_INIT_VALUE) ? BME280_FAIL_VALUE : BME280_SUCCESS_VALUE;

  if (retVal == BME280_FAIL_VALUE) {
    retVal += SetTemperatureConfig();
    retVal += SetHumidityConfig();
    retVal += SetDataAcquisition();
  }

  return retVal;
}

void FLEX_AppInit() {
  printf("%s\n", APPLICATION_NAME);

  if (Init() == BME280_SUCCESS_VALUE) {
    printf("Sensor Initialised.\n");
    FLEX_JobSchedule(ReadHumidity, FLEX_ASAP());
  } else {
    printf("Failed to initialise the sensor!\n");
  }
}
//! [CODE]
