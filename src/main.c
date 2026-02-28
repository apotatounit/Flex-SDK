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

/* Settle delays to try in calibration (ms from power-on to Modbus_Init). */
static const unsigned int SETTLE_CALIBRATION_MS[] = { 400, 500, 600, 700, 800, 1000, 1200, 1500, 2000 };
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
    printf("First-try real reading: settle >= %u ms.\r\n", best_settle_ms);
  else
    printf("All first responses 0.0 °C or fail -> use skip-first-then-read.\r\n");

  /* Phase 2: Minimal settle with "skip first, then read" (second read only). */
  printf("\r\n--- Minimal settle (skip first response, then read) ---\r\n");
  unsigned int minimal_settle_ms = 0u;

  for (size_t i = 0; i < NUM_SETTLE_TRIALS; i++)
  {
    unsigned int settle_ms = SETTLE_CALIBRATION_MS[i];
    if (i > 0)
    {
      FLEX_PowerOutDeinit();
      FLEX_DelayMs(POWER_CYCLE_MS);
    }
    if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
      continue;
    FLEX_DelayMs(settle_ms);
    if (Modbus_Init() != 0)
    {
      FLEX_PowerOutDeinit();
      continue;
    }
    FLEX_DelayMs(SERIAL_SETTLE_MS);
    /* Skip first (often 00 00) */
    float discard = MODBUS_TEMPERATURE_INVALID;
    (void)Modbus_ReadTemperature_FirstAttemptOnly(&discard);
    /* Second read: real value? */
    float t2 = MODBUS_TEMPERATURE_INVALID;
    int r2 = Modbus_ReadTemperature_FirstAttemptOnly(&t2);
    Modbus_Deinit();
    FLEX_PowerOutDeinit();

    if (r2 == 0 && t2 != 0.0f && !isnan(t2))
    {
      printf("  settle %u ms: after skip-first, second read = %.1f °C [OK]\r\n", settle_ms, (double)t2);
      if (minimal_settle_ms == 0u)
        minimal_settle_ms = settle_ms;
    }
  }
  if (minimal_settle_ms != 0u)
    printf("Minimal settle (skip first then read): %u ms\r\n", minimal_settle_ms);
  else
    printf("Minimal settle: none found in range (skip first then read still 0.0/fail)\r\n");

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

  uint32_t sum_ticks = 0u;
  uint32_t count_fast = 0u;
  uint32_t max_ticks = 0u;

  for (uint32_t i = 0; i < NUM_READS; i++)
  {
    float temperature = MODBUS_TEMPERATURE_INVALID;
    uint32_t t0 = FLEX_TickGet();
    int result = Modbus_Request_Receive_Temperature(&temperature);
    uint32_t t1 = FLEX_TickGet();
    uint32_t duration_ticks = t1 - t0;
    uint32_t duration_ms   = duration_ticks;

    if (result == 0 && duration_ticks < 2000u)
    {
      sum_ticks += duration_ticks;
      count_fast++;
      if (duration_ticks > max_ticks)
        max_ticks = duration_ticks;
    }

    printf("Read %lu: %s, %.1f °C, %lu ticks (%lu ms) %s\r\n",
           (unsigned long)(i + 1),
           result != 0 ? "FAIL" : "OK",
           (double)temperature,
           (unsigned long)duration_ticks,
           (unsigned long)duration_ms,
           (result == 0 && duration_ticks < 2000u) ? "[PASS]" : "[--]");
  }

  printf("\r\n");
  printf("--- Determined ---\r\n");
  if (minimal_settle_ms != 0u)
    printf("  Minimal settle (skip first then read): %u ms\r\n", minimal_settle_ms);
  printf("  Read timeout (disconnected): 2000 ticks (~2 s) per serial_read\r\n");
  if (count_fast > 0u)
    printf("  Read duration (connected, short timeout): avg %lu ms, max %lu ms\r\n",
           (unsigned long)(sum_ticks / count_fast), (unsigned long)max_ticks);
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
