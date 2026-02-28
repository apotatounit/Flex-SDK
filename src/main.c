/*
 * Modbus RX timeout test – doc/modbus-rx-timeout-spec.md
 *
 * Init order: Power on -> POWER_SETTLE_MS -> Modbus_Init -> SERIAL_SETTLE_MS -> read(s).
 * Sensor often returns 00 00 on first request; longer settle after power-up helps.
 */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "flex.h"
#include "modbussensor.h"

#define SENSOR_POWER_SUPPLY  FLEX_POWER_OUT_5V
#define POWER_SETTLE_MS      800u   /* ms after power-on before Modbus_Init (sensor stabilise); tune via calibration */
#define SERIAL_SETTLE_MS     150u   /* ms after Modbus_Init before first read */
#define NUM_READS            5u
#define POWER_CYCLE_MS      300u   /* ms power off between calibration trials */

/* Settle delays to try in calibration (ms from power-on to first read = settle + SERIAL_SETTLE_MS). */
static const unsigned int SETTLE_CALIBRATION_MS[] = { 400, 500, 600, 700, 800, 1000 };
#define NUM_SETTLE_TRIALS    (sizeof(SETTLE_CALIBRATION_MS) / sizeof(SETTLE_CALIBRATION_MS[0]))

static time_t RunModbusRxTimeoutTest(void)
{
  printf("\r\n");
  printf("========================================\r\n");
  printf("MODBUS RX TIMEOUT TEST\r\n");
  printf("========================================\r\n");

  printf("\r\n--- Settle calibration (first response only, no retries) ---\r\n");
  printf("Trying settle (power-on to Modbus_Init) ms: ");
  for (size_t i = 0; i < NUM_SETTLE_TRIALS; i++)
    printf("%u ", SETTLE_CALIBRATION_MS[i]);
  printf("\r\n");

  unsigned int best_settle_ms = POWER_SETTLE_MS;
  int best_nonzero = 0;

  for (size_t i = 0; i < NUM_SETTLE_TRIALS; i++)
  {
    unsigned int settle_ms = SETTLE_CALIBRATION_MS[i];

    if (i > 0)
    {
      FLEX_PowerOutDeinit();
      FLEX_DelayMs(POWER_CYCLE_MS);
    }
    if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
    {
      printf("  settle %u ms: PowerOutInit failed\r\n", settle_ms);
      continue;
    }
    FLEX_DelayMs(settle_ms);

    if (Modbus_Init() != 0)
    {
      printf("  settle %u ms: Modbus_Init failed\r\n", settle_ms);
      FLEX_PowerOutDeinit();
      continue;
    }
    FLEX_DelayMs(SERIAL_SETTLE_MS);

    float t = MODBUS_TEMPERATURE_INVALID;
    int r = Modbus_ReadTemperature_FirstAttemptOnly(&t);

    Modbus_Deinit();
    FLEX_PowerOutDeinit();

    if (r != 0)
      printf("  settle %u ms: first response FAIL (%d)\r\n", settle_ms, r);
    else
    {
      int nonzero = (t != 0.0f && !isnan(t));
      printf("  settle %u ms: first response %.1f °C %s\r\n",
             settle_ms, (double)t, nonzero ? "[real]" : "(00 00?)");
      if (nonzero && !best_nonzero)
      {
        best_nonzero = 1;
        best_settle_ms = settle_ms;
      }
    }
  }

  printf("\r\n");
  if (best_nonzero)
    printf("Use settle >= %u ms for real reading on first try.\r\n", best_settle_ms);
  else
    printf("All first responses were 0.0 °C or fail; try longer settle or check sensor.\r\n");

  printf("\r\n--- Main test (settle = %u ms, %lu reads) ---\r\n", (unsigned)POWER_SETTLE_MS, (unsigned long)NUM_READS);
  printf("Starting in 2s...\r\n");
  FLEX_DelayMs(2000);

  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("FLEX_PowerOutInit failed\r\n");
    return FLEX_TimeGet() + 10;
  }
  FLEX_DelayMs(POWER_SETTLE_MS);

  if (Modbus_Init() != 0)
  {
    printf("Modbus_Init failed\r\n");
    FLEX_PowerOutDeinit();
    return FLEX_TimeGet() + 10;
  }
  FLEX_DelayMs(SERIAL_SETTLE_MS);

  for (uint32_t i = 0; i < NUM_READS; i++)
  {
    float temperature = MODBUS_TEMPERATURE_INVALID;
    uint32_t t0 = FLEX_TickGet();
    int result = Modbus_Request_Receive_Temperature(&temperature);
    uint32_t t1 = FLEX_TickGet();
    uint32_t duration_ticks = t1 - t0;
    uint32_t duration_ms   = duration_ticks;

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
