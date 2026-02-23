// Copyright (c) 2016-2024, Myriota Pty Ltd, All Rights Reserved
// SPDX-License-Identifier: BSD-3-Clause-Attribution
//
// This file is licensed under the BSD with attribution  (the "License"); you
// may not use these files except in compliance with the License.
//
// You may obtain a copy of the License here:
// LICENSE-BSD-3-Clause-Attribution.txt and at
// https://spdx.org/licenses/BSD-3-Clause-Attribution.html
//
// See the License for the specific language governing permissions and
// limitations under the License.

// An example running on Myriota's "FlexSense" board.
// This example demonstrates how to use the Battery API's. The "BatterySample" job will sample if
// the device is externally or battery-powered "FLEX_IsOnExternalPower" and the battery voltage
// "FLEX_GetBatteryVoltage" and print it on the debug console. "BatterySample" will be scheduled
// "SAMPLES_PER_DAY" times per day.
//! [CODE]

#include <stdio.h>
#include "flex.h"

#define APPLICATION_NAME "Battery API Example"

// Sample frequency
#define SAMPLES_PER_DAY 4

// Sample if the FlexSense device is externally powered and the battery voltage.
static time_t BatterySample(void) {
  // Check if the FlexSense is externally powered (via USB or the FlexSense external cable
  // interface)
  bool is_on_external_power = false;
  if (FLEX_IsOnExternalPower(&is_on_external_power) != 0) {
    printf("Failed to read is on external power!\n");
  }

  // Get the battery voltage. The voltage will be 0 if externally powered.
  int32_t battery_mv = 0;
  if (FLEX_GetBatteryVoltage(&battery_mv) != 0) {
    printf("Failed to read battery voltage!\n");
  }

  // Print the results on the debug console.
  printf("Battery API Sample: Is on external power = %d, Battery mV = %ld\n", is_on_external_power,
    battery_mv);

  return (FLEX_TimeGet() + 24 * 3600 / SAMPLES_PER_DAY);
}

static void on_external_power_handler(const bool *const is_ext_pwr) {
  char buf[32] = {0};
  snprintf(buf, sizeof(buf), "External Power: (%d)\n", *is_ext_pwr);
}

void FLEX_AppInit() {
  printf("%s\n", APPLICATION_NAME);
  FLEX_JobSchedule(BatterySample, FLEX_ASAP());

  // Sample the `is external power` status and the battery voltage if the external power status
  // changes. If the external power status does not change then the sample will be every
  // DAYS_BETWEEN_MESSAGES days
  FLEX_OnExternalPowerHandlerSet(on_external_power_handler);
}
//! [CODE]
