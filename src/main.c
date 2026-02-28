/*
 * Modbus RX timeout test – doc/modbus-rx-timeout-spec.md
 *
 * Minimal firmware: one Modbus temperature read with duration (ticks and ms).
 * Test 1 (connected):   Plug sensor, power on, reset → expect OK, duration < 2000 ticks.
 * Test 2 (disconnected): No sensor, reset → expect FAIL, duration ~2000 ticks.
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "flex.h"
#include "modbussensor.h"

#define SENSOR_POWER_SUPPLY   FLEX_POWER_OUT_5V
#define MODBUS_MIN_SETTLE_MS 200u  /* min ms from sensor power-up to first valid read (1000 ticks = 1 s) */

static time_t RunModbusRxTimeoutTest(void)
{
  printf("\r\n");
  printf("========================================\r\n");
  printf("MODBUS RX TIMEOUT TEST\r\n");
  printf("========================================\r\n");
  printf("  rx_timeout_ticks = 2000 (~2 s) in modbussensor.c.\r\n");
  printf("  Test 1 (CONNECTED):   Plug sensor, power on, reset. Expect: OK, duration < 2000 ticks.\r\n");
  printf("  Test 2 (DISCONNECTED): No sensor, reset. Expect: FAIL, duration ~2000 ticks.\r\n");
  printf("\r\n");
  printf("One Modbus read in 2s...\r\n");
  FLEX_DelayMs(2000);

  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("FLEX_PowerOutInit failed\r\n");
    return FLEX_TimeGet() + 10;
  }
  if (Modbus_Init() != 0)
  {
    printf("Modbus_Init failed\r\n");
    FLEX_PowerOutDeinit();
    return FLEX_TimeGet() + 10;
  }
  FLEX_DelayMs(MODBUS_MIN_SETTLE_MS);

  float temperature = MODBUS_TEMPERATURE_INVALID;
  uint32_t t0 = FLEX_TickGet();
  int result = Modbus_Request_Receive_Temperature(&temperature);
  uint32_t t1 = FLEX_TickGet();
  uint32_t duration_ticks = t1 - t0;
  uint32_t duration_ms   = duration_ticks;  /* 1000 ticks = 1 s */

  printf("\r\n");
  printf("Result: %s\r\n", result != 0 ? "FAIL" : "OK");
  printf("Temperature: %.1f °C\r\n", (double)temperature);
  printf("Duration: %lu ticks (%lu ms)\r\n", (unsigned long)duration_ticks, (unsigned long)duration_ms);
  printf("Pass: connected → OK and < 2000 ticks; disconnected → FAIL and ~2000 ticks.\r\n");
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
