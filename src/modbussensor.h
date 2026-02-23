#ifndef MODBUSSENSOR_H
#define MODBUSSENSOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <time.h>
#include "flex.h"
#include "myriota/modbus.h"

#define SENSOR_READ_MAX_RETRIES 3

// Serial context structure
typedef struct
{
    FLEX_SerialProtocol protocol;
    uint32_t baud_rate;
    uint32_t rx_timeout_ticks;
} SerialContext;

// Application context structure
typedef struct
{
    MYRIOTA_ModbusHandle modbus_handle;
    SerialContext serial_context;
} ApplicationContext;

// Sentinel for invalid temperature when Modbus read fails (caller can check with isnan())
#include <math.h>
#define MODBUS_TEMPERATURE_INVALID (NAN)

// Function declarations
int Modbus_Init(void);
int Modbus_Deinit(void);
/** Try to find a Modbus temperature sensor by scanning slave addresses 0x01..0x0F.
 *  Reads input register 0x0001 (2 registers, temperature in tenths of °C).
 *  \param[out] out_slave_addr Slave address that responded (1..15), or 0 if none.
 *  \param[out] out_temperature Temperature in °C, or NAN if none found.
 *  \return 0 if a device was found and read, -1 otherwise. */
int Modbus_ScanForTemperatureSensor(uint8_t *out_slave_addr, float *out_temperature);
int Modbus_Request_Receive_Temperature(float *const temperature);

#endif // MODBUSSENSOR_H
