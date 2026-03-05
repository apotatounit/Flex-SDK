/**
 * Continuous diagnostics: analog (0.5–4.5 V → 0–10 bar), pulse count/ppm,
 * Modbus temperature. Sample every 1 s; print moving average over last 60 s.
 * Output is plottable key=value for plugins.
 *
 * Refer to doc/agent-in-calibration.md for sensor interfaces.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "flex.h"
#include "modbussensor.h"

#define APPLICATION_NAME "Sensor Diagnostics (1s sampling, 60s MA)"

#define SENSOR_POWER_SUPPLY    FLEX_POWER_OUT_5V
#define ANALOG_IN_MODE        FLEX_ANALOG_IN_VOLTAGE
#define PULSE_WAKEUP_COUNT    0
#define SAMPLE_INTERVAL_MS    1000
#define MOVING_AVERAGE_WINDOW 60

/* Pressure: 0.5 V = 0 bar, 4.5 V = 10 bar → bar = (V - 0.5) * (10.0/4.0) */
#define PRESSURE_V_MIN  0.5f
#define PRESSURE_V_MAX  4.5f
#define PRESSURE_BAR_MAX 10.0f

static bool bModbusInitDone = false;

static int read_analog_voltage_v(float *out_v)
{
  uint32_t mv = 0;
  int ret = FLEX_AnalogInputReadVoltage(&mv);
  if (ret != 0)
    return ret;
  *out_v = (float)mv / 1000.0f;
  return 0;
}

/** Convert voltage to bar (0.5–4.5 V → 0–10 bar). Out-of-range returns NAN. */
static float voltage_to_bar(float v)
{
  if (v < PRESSURE_V_MIN || v > PRESSURE_V_MAX)
    return NAN;
  return (v - PRESSURE_V_MIN) * (PRESSURE_BAR_MAX / (PRESSURE_V_MAX - PRESSURE_V_MIN));
}

static int read_temperature_c(float *out_c)
{
  *out_c = MODBUS_TEMPERATURE_INVALID;
  if (Modbus_Request_Receive_Temperature(out_c) != 0)
    return -1;
  return 0;
}

typedef struct
{
  float buf[MOVING_AVERAGE_WINDOW];
  int n;
  int head;
  float sum;
  int valid_count; /* for temp: only count non-NAN in sum */
} RingAvg;

static void ring_avg_init(RingAvg *r)
{
  r->n = 0;
  r->head = 0;
  r->sum = 0.0f;
  r->valid_count = 0;
}

static void ring_avg_push(RingAvg *r, float val)
{
  int idx = r->head;
  if (r->n < MOVING_AVERAGE_WINDOW)
  {
    r->buf[idx] = val;
    r->n++;
    if (!isnan(val)) { r->sum += val; r->valid_count++; }
  }
  else
  {
    float old = r->buf[idx];
    r->buf[idx] = val;
    if (!isnan(old)) { r->sum -= old; r->valid_count--; }
    if (!isnan(val)) { r->sum += val; r->valid_count++; }
  }
  r->head = (r->head + 1) % MOVING_AVERAGE_WINDOW;
}

/** Moving average; returns NAN if no valid samples. */
static float ring_avg_mean(const RingAvg *r)
{
  if (r->valid_count == 0)
    return NAN;
  return (float)(r->sum / (double)r->valid_count);
}

static int init_sensors(void)
{
  if (FLEX_PowerOutInit(SENSOR_POWER_SUPPLY) != 0)
  {
    printf("Failed to enable sensor power supply.\n");
    return -1;
  }
  if (FLEX_AnalogInputInit(ANALOG_IN_MODE) != 0)
  {
    printf("Failed to init analog input.\n");
    return -1;
  }
  if (bModbusInitDone == false)
  {
    if (Modbus_Init() != 0)
      printf("Failed to init Modbus.\n");
    else
      bModbusInitDone = true;
  }
  if (FLEX_PulseCounterInit(PULSE_WAKEUP_COUNT, FLEX_PCNT_DEFAULT_OPTIONS) != 0)
  {
    printf("Failed to init pulse counter.\n");
    return -1;
  }
  return 0;
}

/** Print one line per second in plottable key=value format for plugins. */
static void print_plottable(uint32_t tick, float pressure_v, float pressure_bar,
  float temp_c, uint64_t pulses, float ppm,
  float pressure_ma, float temp_ma, float ppm_ma)
{
  /* Plottable: space-separated key=value so plugins can split and plot */
  printf("tick=%lu pressure_v=%.3f pressure_bar=", (unsigned long)tick, (double)pressure_v);
  if (isnan(pressure_bar))
    printf("nan");
  else
    printf("%.2f", (double)pressure_bar);
  printf(" temp_c=");
  if (isnan(temp_c))
    printf("nan");
  else
    printf("%.1f", (double)temp_c);
  printf(" pulses=%lu ppm=%.0f", (unsigned long)pulses, (double)ppm);
  printf(" pressure_ma=");
  if (isnan(pressure_ma))
    printf("nan");
  else
    printf("%.2f", (double)pressure_ma);
  printf(" temp_ma=");
  if (isnan(temp_ma))
    printf("nan");
  else
    printf("%.1f", (double)temp_ma);
  printf(" ppm_ma=");
  if (isnan(ppm_ma))
    printf("nan");
  else
    printf("%.0f", (double)ppm_ma);
  printf("\n");
  fflush(stdout);
}

void FLEX_AppInit(void)
{
  printf("%s\n", APPLICATION_NAME);
  printf("Nilus diagnostics – 0.5–4.5 V → 0–10 bar, ppm from 1s window, 60s moving average\n");
  printf("Plottable: tick pressure_v pressure_bar temp_c pulses ppm pressure_ma temp_ma ppm_ma\n");

  if (init_sensors() != 0)
  {
    printf("Sensor init failed; exiting.\n");
    return;
  }

  RingAvg avg_bar, avg_ppm, avg_temp;
  ring_avg_init(&avg_bar);
  ring_avg_init(&avg_ppm);
  ring_avg_init(&avg_temp);

  uint64_t prev_pulses = FLEX_PulseCounterGet();
  uint32_t prev_tick = FLEX_TickGet();

  for (;;)
  {
    FLEX_DelayMs(SAMPLE_INTERVAL_MS);

    uint32_t tick = FLEX_TickGet();
    uint64_t pulses = FLEX_PulseCounterGet();
    uint32_t elapsed_ms = tick - prev_tick;
    float ppm = (elapsed_ms > 0)
      ? (float)((pulses - prev_pulses) * 60000ULL / (uint64_t)elapsed_ms)
      : 0.0f;
    prev_pulses = pulses;
    prev_tick = tick;

    float pressure_v = NAN;
    if (read_analog_voltage_v(&pressure_v) != 0)
      pressure_v = NAN;
    float pressure_bar = voltage_to_bar(pressure_v);

    float temp_c = MODBUS_TEMPERATURE_INVALID;
    (void)read_temperature_c(&temp_c);

    ring_avg_push(&avg_bar, pressure_bar);
    ring_avg_push(&avg_ppm, ppm);
    ring_avg_push(&avg_temp, temp_c);

    float pressure_ma = ring_avg_mean(&avg_bar);
    float temp_ma = ring_avg_mean(&avg_temp);
    float ppm_ma = ring_avg_mean(&avg_ppm);

    print_plottable(tick, pressure_v, pressure_bar, temp_c, pulses, ppm,
                    pressure_ma, temp_ma, ppm_ma);
  }
}
