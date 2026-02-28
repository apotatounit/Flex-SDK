/*
 * Modbus RX timeout test – doc/modbus-rx-timeout-spec.md
 *
 * Correct init order for sensor connected continuously:
 *   1. Power on sensor (FLEX_PowerOutInit)
 *   2. Wait POWER_SETTLE_MS for power and sensor to stabilise
 *   3. Modbus_Init (RS485 serial)
 *   4. Wait SERIAL_SETTLE_MS after serial enable before first frame
 *   5. Request data (one or more reads); duration < 2000 ticks when connected.
 *
 * Test 1 (connected):   Expect OK, duration < 2000 ticks per read.
 * Test 2 (disconnected): Expect FAIL, duration ~2000 ticks per read.
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "flex.h"
#include "modbussensor.h"

#define SENSOR_POWER_SUPPLY  FLEX_POWER_OUT_5V
#define POWER_SETTLE_MS      400u   /* ms after power-on before Modbus_Init (sensor stabilise) */
#define SERIAL_SETTLE_MS     150u   /* ms after Modbus_Init before first read (RS485/serial ready) */
#define NUM_READS            5u    /* number of timed reads to run (first + follow-ups) */

static time_t RunModbusRxTimeoutTest(void)
{
  printf("\r\n");
  printf("========================================\r\n");
  printf("MODBUS RX TIMEOUT TEST\r\n");
  printf("========================================\r\n");
  printf("  Init: PowerOn -> %lu ms -> Modbus_Init -> %lu ms -> read(s).\r\n",
         (unsigned long)POWER_SETTLE_MS, (unsigned long)SERIAL_SETTLE_MS);
  printf("  Test 1 (CONNECTED):   Expect OK, each duration < 2000 ticks.\r\n");
  printf("  Test 2 (DISCONNECTED): Expect FAIL, duration ~2000 ticks.\r\n");
  printf("\r\n");
  printf("Starting in 2s...\r\n");
  FLEX_DelayMs(2000);

  /* 1. Power on */
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("FLEX_PowerOutInit failed\r\n");
    return FLEX_TimeGet() + 10;
  }
  FLEX_DelayMs(POWER_SETTLE_MS);

  /* 2. Modbus (serial) init */
  if (Modbus_Init() != 0)
  {
    printf("Modbus_Init failed\r\n");
    FLEX_PowerOutDeinit();
    return FLEX_TimeGet() + 10;
  }
  FLEX_DelayMs(SERIAL_SETTLE_MS);

  /* 3. Timed reads */
  for (uint32_t i = 0; i < NUM_READS; i++)
  {
    float temperature = MODBUS_TEMPERATURE_INVALID;
    uint32_t t0 = FLEX_TickGet();
    int result = Modbus_Request_Receive_Temperature(&temperature);
    uint32_t t1 = FLEX_TickGet();
    uint32_t duration_ticks = t1 - t0;
    uint32_t duration_ms   = duration_ticks;  /* 1000 ticks = 1 s */

    printf("Read %lu: %s, %.1f °C, %lu ticks (%lu ms) %s\r\n",
           (unsigned long)(i + 1),
           result != 0 ? "FAIL" : "OK",
           (double)temperature,
           (unsigned long)duration_ticks,
           (unsigned long)duration_ms,
           (result == 0 && duration_ticks < 2000u) ? "[PASS]" : "[--]");
  }

  printf("\r\n");
  printf("Pass: connected -> OK and < 2000 ticks; disconnected -> FAIL and ~2000 ticks.\r\n");
  printf("Reset to run again.\r\n");
  printf("\r\n");

  Modbus_Deinit();
  FLEX_PowerOutDeinit();
  return FLEX_TimeGet() + 10;
}

void FLEX_AppInit(void)
{
  printf("Modbus RX timeout test – %s %s\r\n", __DATE__, __TIME__);
  FLEX_JobSchedule(RunModbusRxTimeoutTest, FLEX_ASAP());
}
