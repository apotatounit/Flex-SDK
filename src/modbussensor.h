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
typedef struct {
    FLEX_SerialProtocol protocol;
    uint32_t baud_rate;
    uint32_t rx_timeout_ticks;
} SerialContext;

// Application context structure
typedef struct {
    MYRIOTA_ModbusHandle modbus_handle;
    SerialContext serial_context;
} ApplicationContext;

// Function declarations
int serial_init(void *const ctx);
void serial_deinit(void *const ctx);
ssize_t serial_read(void *const ctx, uint8_t *const buffer, const size_t count);
ssize_t serial_write(void *const ctx, const uint8_t *const buffer, const size_t count);
uint16_t merge_i16(const uint8_t hi, const uint8_t low);
time_t send_message(void);
int Modbus_Init(void);
int Modbus_Deinit(void);
int Modbus_Request_Receive_Temperature(float *const temperature);

#endif // MODBUSSENSOR_H
