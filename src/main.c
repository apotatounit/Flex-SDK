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
#define MAX_ATTEMPTS         5u     /* max single-attempt reads per (settle, delay) in calibration */
#define POWER_CYCLE_MS      300u   /* ms power off between calibration trials */

/* Settle range: 2000 ± 1000 ms, step 100 ms → 1000..3000 (21 values). */
static const unsigned int SETTLE_CALIBRATION_MS[] = {
  1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000,
  2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000
};
#define NUM_SETTLE_TRIALS    (sizeof(SETTLE_CALIBRATION_MS) / sizeof(SETTLE_CALIBRATION_MS[0]))

/* Read delay (ms) between consecutive read attempts in calibration. */
static const unsigned int READ_DELAY_MS[] = { 0, 50, 100, 200 };
#define NUM_READ_DELAYS      (sizeof(READ_DELAY_MS) / sizeof(READ_DELAY_MS[0]))

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

  /*
   * Phase 2: For each (settle, read_delay), do up to 5 single-attempt reads with
   * read_delay ms between them. Record which read (1..5) first returns non-zero.
   * This distinguishes: settle-driven (longer settle -> non-zero sooner) vs
   * attempt-driven (non-zero after N reads regardless of settle).
   * result[settle_idx][delay_idx] = first non-zero at read 1..5, or 0.
   */
  printf("\r\n--- Settle vs read delay: first non-zero at attempt (1..5) ---\r\n");
  printf("Settle 1000..3000 step 100 ms; read delay 0, 50, 100, 200 ms; up to 5 reads each.\r\n");

  unsigned int minimal_settle_ms = 0u;
  /* first_nonzero[settle_idx][delay_idx] = 1..5 or 0 */
  unsigned int first_nonzero[NUM_SETTLE_TRIALS][NUM_READ_DELAYS];
  for (size_t s = 0; s < NUM_SETTLE_TRIALS; s++)
    for (size_t d = 0; d < NUM_READ_DELAYS; d++)
      first_nonzero[s][d] = 0u;

  for (size_t s = 0; s < NUM_SETTLE_TRIALS; s++)
  {
    unsigned int settle_ms = SETTLE_CALIBRATION_MS[s];
    for (size_t d = 0; d < NUM_READ_DELAYS; d++)
    {
      unsigned int read_delay_ms = READ_DELAY_MS[d];

      if (s > 0 || d > 0)
      {
        FLEX_PowerOutDeinit();
        FLEX_DelayMs(POWER_CYCLE_MS);
      }
      if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
      {
        printf("  settle %u delay %u: PowerOutInit failed\r\n", settle_ms, read_delay_ms);
        continue;
      }
      FLEX_DelayMs(settle_ms);
      if (Modbus_Init() != 0)
      {
        printf("  settle %u delay %u: Modbus_Init failed\r\n", settle_ms, read_delay_ms);
        FLEX_PowerOutDeinit();
        continue;
      }
      FLEX_DelayMs(SERIAL_SETTLE_MS);

      unsigned int first_ok = 0u;
      for (unsigned int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
      {
        float t = MODBUS_TEMPERATURE_INVALID;
        int r = Modbus_ReadTemperature_FirstAttemptOnly(&t);
        if (r == 0 && t != 0.0f && !isnan(t))
        {
          first_ok = attempt;
          break;
        }
        if (attempt < MAX_ATTEMPTS)
          FLEX_DelayMs(read_delay_ms);
      }
      first_nonzero[s][d] = first_ok;
      Modbus_Deinit();
      FLEX_PowerOutDeinit();

      if (first_ok != 0u && minimal_settle_ms == 0u)
        minimal_settle_ms = settle_ms;
    }
  }

  /* Print table: rows = settle, cols = read_delay, cell = first non-zero read # or "-" */
  printf("\r\n        delay(ms): ");
  for (size_t d = 0; d < NUM_READ_DELAYS; d++)
    printf(" %4u", READ_DELAY_MS[d]);
  printf("\r\n");
  for (size_t s = 0; s < NUM_SETTLE_TRIALS; s++)
  {
    printf("settle %4u ms: ", SETTLE_CALIBRATION_MS[s]);
    for (size_t d = 0; d < NUM_READ_DELAYS; d++)
    {
      unsigned int v = first_nonzero[s][d];
      if (v != 0u)
        printf("   %u ", v);
      else
        printf("   - ");
    }
    printf("\r\n");
  }

  /* Summary: minimal settle for which we get non-zero at read 1 (any delay), read 2, etc. */
  printf("\r\n--- Summary (settle vs attempts) ---\r\n");
  for (unsigned int at = 1; at <= MAX_ATTEMPTS; at++)
  {
    unsigned int min_settle = 0u;
    for (size_t s = 0; s < NUM_SETTLE_TRIALS; s++)
    {
      for (size_t d = 0; d < NUM_READ_DELAYS; d++)
        if (first_nonzero[s][d] == at)
        {
          min_settle = SETTLE_CALIBRATION_MS[s];
          break;
        }
      if (min_settle != 0u)
        break;
    }
    if (min_settle != 0u)
      printf("  First non-zero at read %u: minimal settle = %u ms\r\n", at, min_settle);
  }
  if (minimal_settle_ms != 0u)
    printf("  Minimal settle (any delay, any attempt): %u ms\r\n", minimal_settle_ms);
  else
    printf("  No non-zero in 5 reads for any (settle, delay)\r\n");

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
