/*
 * Modbus RTU over RS485 (4800 baud). Temperature from input register 0x0001 (tenths °C).
 *
 * Modbus_Init: MYRIOTA_ModbusInit → MYRIOTA_ModbusEnable (calls serial_init → FLEX_SerialInit).
 * Modbus_Deinit: MYRIOTA_ModbusDisable (calls serial_deinit → FLEX_SerialDeinit) → MYRIOTA_ModbusDeinit.
 * serial_read: byte-by-byte read with rx_timeout_ticks (~2 s) total timeout.
 */
#include "modbussensor.h"

static ApplicationContext application_context = {0};

/* Serial callbacks used by MYRIOTA Modbus stack */
static int serial_init(void *const ctx)
{
  SerialContext *const serial = ctx;
  return FLEX_SerialInit(serial->protocol, serial->baud_rate);
}

static void serial_deinit(void *const ctx)
{
  (void)ctx;
  FLEX_SerialDeinit();
}

/* Inter-byte timeout: after at least MIN_BYTES_BEFORE_INTER_BYTE received, if no further byte
 * within this many ticks, treat frame as complete. 1000 ticks = 1 s. */
#define INTER_BYTE_TIMEOUT_TICKS      100u   /* ~100 ms after last byte */
#define MIN_BYTES_BEFORE_INTER_BYTE  9u      /* full Read Input Registers response = 9 bytes; avoids returning on 8-byte TX echo */

/* Read up to count bytes; blocks until count bytes, inter-byte timeout (frame end), or rx_timeout_ticks elapsed. */
static ssize_t serial_read(void *const ctx, uint8_t *const buffer, const size_t count)
{
  SerialContext *const serial = ctx;

  uint8_t *curr = buffer;
  const uint8_t *const end = buffer + count;
  const uint32_t total_end_ticks = FLEX_TickGet() + serial->rx_timeout_ticks;
  uint32_t next_byte_end_ticks = total_end_ticks;

  while (FLEX_TickGet() <= total_end_ticks)
  {
    if (curr >= end)
    {
      return (ssize_t)(curr - buffer);
    }

    int num_bytes = FLEX_SerialRead(curr, 1);
    if (num_bytes < 0)
    {
      return -1;
    }
    if (num_bytes == 1)
    {
      ++curr;
      /* Only use short inter-byte timeout once we have a full response (avoid returning on TX echo). */
      if ((size_t)(curr - buffer) >= MIN_BYTES_BEFORE_INTER_BYTE)
      {
        next_byte_end_ticks = FLEX_TickGet() + INTER_BYTE_TIMEOUT_TICKS;
      }
    }
    else
    {
      if ((size_t)(curr - buffer) >= MIN_BYTES_BEFORE_INTER_BYTE && FLEX_TickGet() > next_byte_end_ticks)
      {
        return (ssize_t)(curr - buffer);
      }
    }
  }

  return (ssize_t)(curr - buffer);
}

static ssize_t serial_write(void *const ctx, const uint8_t *const buffer, const size_t count)
{
  (void)ctx;
  const int result = FLEX_SerialWrite(buffer, count);
  if (result != FLEX_SUCCESS)
  {
    return result;
  }
  return count;
}

inline int16_t merge_i16(const uint8_t hi, const uint8_t low)
{
  return (int16_t)((uint16_t)hi << 8 | (uint16_t)low);
}

#define MODBUS_SCAN_SLAVE_MIN  0x01
#define MODBUS_SCAN_SLAVE_MAX  0x0F
#define MODBUS_TEMP_REG_ADDR   0x0001

/** Read temperature from a specific slave (input register 0x0001, 2 regs, value in tenths °C). */
static int modbus_read_temperature_at_slave(const MYRIOTA_ModbusHandle handle,
    MYRIOTA_ModbusDeviceAddress slave, float *const temperature)
{
  uint8_t response_bytes[4] = {0};
  const MYRIOTA_ModbusDataAddress addr = MODBUS_TEMP_REG_ADDR;

  int result = MYRIOTA_ModbusReadInputRegisters(handle, slave, addr, 2, response_bytes);
  if (result != MODBUS_SUCCESS)
    return result;
  if (response_bytes[0] == 0x00 && response_bytes[1] == 0x00)
    return -MODBUS_ERROR_IO_FAILURE; /* treat all-zero as no sensor */
  int16_t temp_raw = merge_i16(response_bytes[0], response_bytes[1]);
  *temperature = (float)temp_raw / 10.0f;
  return MODBUS_SUCCESS;
}

int Modbus_ScanForTemperatureSensor(uint8_t *out_slave_addr, float *out_temperature)
{
  const MYRIOTA_ModbusHandle handle = application_context.modbus_handle;
  if (handle <= 0)
  {
    if (out_slave_addr) *out_slave_addr = 0;
    if (out_temperature) *out_temperature = MODBUS_TEMPERATURE_INVALID;
    return -1;
  }
  if (out_slave_addr) *out_slave_addr = 0;
  if (out_temperature) *out_temperature = MODBUS_TEMPERATURE_INVALID;

  for (uint8_t slave = MODBUS_SCAN_SLAVE_MIN; slave <= MODBUS_SCAN_SLAVE_MAX; slave++)
  {
    float t = MODBUS_TEMPERATURE_INVALID;
    int r = modbus_read_temperature_at_slave(handle, slave, &t);
    if (r == MODBUS_SUCCESS && !isnan(t))
    {
      if (out_slave_addr) *out_slave_addr = slave;
      if (out_temperature) *out_temperature = t;
      printf("Modbus scan: found temperature sensor at slave 0x%02X, %.1f °C\n", (unsigned)slave, (double)t);
      return 0;
    }
  }
  printf("Modbus scan: no temperature sensor found (slaves 0x%02X..0x%02X)\n",
      (unsigned)MODBUS_SCAN_SLAVE_MIN, (unsigned)MODBUS_SCAN_SLAVE_MAX);
  return -1;
}

int Modbus_Request_Receive_Temperature(float *const temperature)
{
  const MYRIOTA_ModbusHandle handle = application_context.modbus_handle;

  if (temperature)
    *temperature = MODBUS_TEMPERATURE_INVALID;

  if (handle <= 0)
    return -MODBUS_ERROR_INVALID_HANDLE;

  uint8_t response_bytes[4] = {0};
  const MYRIOTA_ModbusDeviceAddress slave = 0x01;
  const MYRIOTA_ModbusDataAddress addr = MODBUS_TEMP_REG_ADDR;

  int result = MODBUS_ERROR_IO_FAILURE;

  for (uint8_t retries = 0; retries < SENSOR_READ_MAX_RETRIES; ++retries)
  {
    memset(response_bytes, 0xFFFF, sizeof(response_bytes));

    result = MYRIOTA_ModbusReadInputRegisters(handle, slave, addr, 2, response_bytes);
    if (result != MODBUS_SUCCESS)
    {
      printf("Sensor Request Failed: %d\n", result);
      continue;
    }
    printf("Response Bytes: %02X %02X\n", response_bytes[0], response_bytes[1]);

    /* Skip 00 00 only on first attempt (sensor may not be ready yet); on retry accept as valid 0.0 °C */
    if (retries == 0 && response_bytes[0] == 0x00 && response_bytes[1] == 0x00)
    {
      printf("Skipping first zero result (retry for 0 °C or not ready)\n");
      result = MODBUS_ERROR_IO_FAILURE;
      continue;
    }

    int16_t temp_raw = merge_i16(response_bytes[0], response_bytes[1]);
    if (temperature)
      *temperature = (float)temp_raw / 10.0f;
    break;
  }

  if (result != MODBUS_SUCCESS && temperature)
    *temperature = MODBUS_TEMPERATURE_INVALID;
  return result;
}

int Modbus_ReadTemperature_FirstAttemptOnly(float *const temperature)
{
  const MYRIOTA_ModbusHandle handle = application_context.modbus_handle;
  if (temperature)
    *temperature = MODBUS_TEMPERATURE_INVALID;
  if (handle <= 0)
    return -MODBUS_ERROR_INVALID_HANDLE;

  uint8_t response_bytes[4] = {0};
  const MYRIOTA_ModbusDeviceAddress slave = 0x01;
  const MYRIOTA_ModbusDataAddress addr = MODBUS_TEMP_REG_ADDR;

  int result = MYRIOTA_ModbusReadInputRegisters(handle, slave, addr, 2, response_bytes);
  if (result != MODBUS_SUCCESS)
    return result;
  /* Accept 00 00 as 0.0 °C (no "skip first zero" - for settle calibration) */
  int16_t temp_raw = merge_i16(response_bytes[0], response_bytes[1]);
  if (temperature)
    *temperature = (float)temp_raw / 10.0f;
  return MODBUS_SUCCESS;
}

/** Init: create Modbus handle, then enable serial (FLEX_SerialInit). Caller must wait ≥ MODBUS_MIN_SETTLE_MS after power-up before first read. */
int Modbus_Init()
{
  application_context.serial_context.protocol = FLEX_SERIAL_PROTOCOL_RS485;
  application_context.serial_context.baud_rate = 4800;
  application_context.serial_context.rx_timeout_ticks = 2000; /* ~2 s total read timeout */

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
  if (application_context.modbus_handle <= 0)
  {
    printf("Modbus_Init: MYRIOTA_ModbusInit failed: %d\n", application_context.modbus_handle);
    return -1;
  }
  printf("Modbus_Init: enabling serial (RS485 %lu baud)...\n", (unsigned long)application_context.serial_context.baud_rate);
  int ret = MYRIOTA_ModbusEnable(application_context.modbus_handle);
  if (ret)
  {
    printf("Modbus_Init: ModbusEnable failed: %d\n", ret);
    MYRIOTA_ModbusDeinit(application_context.modbus_handle);
    application_context.modbus_handle = 0;
    return -1;
  }
  return 0;
}

/** Deinit: disable serial (FLEX_SerialDeinit), then free Modbus handle. Required before sleep when sensor may be disconnected. */
int Modbus_Deinit()
{
  MYRIOTA_ModbusHandle h = application_context.modbus_handle;
  if (h <= 0)
    return 0;
  int ret = MYRIOTA_ModbusDisable(h);
  if (ret)
    printf("Modbus_Deinit: ModbusDisable failed %d (serial may be disconnected)\n", ret);
  MYRIOTA_ModbusDeinit(h);
  application_context.modbus_handle = 0;
  return ret;
}