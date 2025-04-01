#include "modbussensor.h"

static ApplicationContext application_context = {0};

static int serial_init(void *const ctx) {
  SerialContext *const serial = ctx;
  return FLEX_SerialInit(serial->protocol, serial->baud_rate);
}

static void serial_deinit(void *const ctx) {
  (void)ctx;
  FLEX_SerialDeinit();
}

static ssize_t serial_read(void *const ctx, uint8_t *const buffer, const size_t count) {
  SerialContext *const serial = ctx;

  uint8_t *curr = buffer;
  const uint8_t *const end = buffer + count;
  const uint32_t end_ticks = FLEX_TickGet() + serial->rx_timeout_ticks;
  while (FLEX_TickGet() <= end_ticks) {
    if (curr >= end) {
      return -1;
    }

    int num_bytes = FLEX_SerialRead(curr, 1);
    if (num_bytes < 0) {
      return -1;
    }
    if (num_bytes == 1) {
      ++curr;
    }
  }

  return curr - buffer;
}

static ssize_t serial_write(void *const ctx, const uint8_t *const buffer, const size_t count) {
  (void)ctx;
  const int result = FLEX_SerialWrite(buffer, count);
  if (result != FLEX_SUCCESS) {
    return result;
  }
  return count;
}

inline uint16_t merge_i16(const uint8_t hi, const uint8_t low) {
  return (int16_t)((uint16_t)hi << 8 | (uint16_t)low);
}

static int Modbus_Request_Receive_Temperature(float *const temperature) {
    const MYRIOTA_ModbusHandle handle = application_context.modbus_handle;

    // NOTE: Enable/disable the Modbus driver in order to conserve power.
    MYRIOTA_ModbusEnable(handle);

    // uint8_t request_bytes[4] = {0};
    uint8_t response_bytes[2] = {0};
    const MYRIOTA_ModbusDeviceAddress slave = 0x01;
    const MYRIOTA_ModbusDataAddress addr = 0x0000;

    int result = MODBUS_ERROR_IO_FAILURE;

    for (uint8_t retries = 0; retries < SENSOR_READ_MAX_RETRIES; ++retries) {
        // Send request to read holding registers
        result = MYRIOTA_ModbusReadInputRegisters(
            handle,
            slave,
            addr,
            1,
            response_bytes
        );
        // result = MYRIOTA_ModbusWrite(handle, slave, addr, request_bytes, sizeof(request_bytes));
        if (result != MODBUS_SUCCESS) {
            printf("Sensor Request Failed: %d\n", result);
            continue;
        }

        uint16_t temp_raw = merge_i16(response_bytes[0], response_bytes[1]);
        *temperature = (float)temp_raw / 10.0f;
        break;
    }

    return result;
}


static int Modbus_Init() {
  // Initialize Modbus device
  application_context.serial_context.protocol = FLEX_SERIAL_PROTOCOL_RS485;
  application_context.serial_context.baud_rate = 4800;
  application_context.serial_context.rx_timeout_ticks = 2000;

  const MYRIOTA_ModbusInitOptions options = {
    .framing_mode = MODBUS_FRAMING_MODE_RTU,
    .serial_interface =
      {
        .ctx = &application_context.serial_context,
        .init = serial_init,
        .deinit = serial_deinit,
        .read = serial_read,
        .write = serial_write,
      },
  };

  application_context.modbus_handle = MYRIOTA_ModbusInit(options);
  if (application_context.modbus_handle <= 0) {
    printf("Failed to initialize Modbus: %d\n", application_context.modbus_handle);
    return -1;
  }
  if (MYRIOTA_ModbusEnable(application_context.modbus_handle)) {
    printf("Failed to enable Modbus\n");
    return -1;
  }
  return 0;
}

static int Modbus_Deinit() {
    if (MYRIOTA_ModbusDisable(application_context.modbus_handle)) {
        printf("Failed to disable Modbus\n");
        return -1;
    }
    return 0;
}