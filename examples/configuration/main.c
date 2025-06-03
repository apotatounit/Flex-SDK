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
// This example will blink the Green LED "NUMBER_OF_FLASHES" times, "CONF_BLINK_EVENTS_PER_DAY"
// times per day and store the total number of flashes in the "DIAG_TOTAL_FLASH_COUNT" diagnostic
// field. The number of times the Blinky Job has been called is stored in the
// "DIAG_LAST_SEQUENCE_NUMBER" persistent diagnostic field. The "CONF_BLINK_EVENTS_PER_DAY"
// configuration field can be updated through the FlexAssist Mobile Application whereas the
// "DIAG_TOTAL_FLASH_COUNT" and "DIAG_LAST_SEQUENCE_NUMBER" can be viewed but not edited from the
// mobile application. If FlexSense is reset the "DIAG_TOTAL_FLASH_COUNT" field will be reset while
// the "DIAG_LAST_SEQUENCE_NUMBER" field will not.
//! [CODE]

#include <stdio.h>
#include "flex.h"

#define APPLICATION_NAME "Diagnostics and Configuration Example"

// Green LED ON time
#define LED_ON_TIME_SEC 1

// Green LED OFF time
#define LED_OFF_TIME_SEC 1

// Number of LED Flashes
#define NUMBER_OF_FLASHES 5

// Default Blinky Interval
#define BLINK_EVENTS_PER_DAY 5

// CONF_BLINK_EVENTS_PER_DAY will be set as a configuration type. This type can be viewed and
// updated through FlexAssist and its value will persist even if FlexSense is reset.
#define CONF_BLINK_EVENTS_PER_DAY FLEX_DIAG_CONF_ID_USER_0
// DIAG_TOTAL_FLASH_COUNT will be set as a diagnostic type. This type can be viewed through
// FlexAssist and its value will not persist and will be set to its default if FlexSense is reset.
#define DIAG_TOTAL_FLASH_COUNT FLEX_DIAG_CONF_ID_USER_1
// DIAG_TOTAL_FLASH_COUNT will be set as a persistent diagnostic type. This type can be viewed
// through FlexAssist and its value will persist even if FlexSense is reset.
#define DIAG_LAST_SEQUENCE_NUMBER FLEX_DIAG_CONF_ID_USER_2

// clang-format off
FLEX_DIAG_CONF_TABLE_BEGIN()
  FLEX_DIAG_CONF_TABLE_U32_ADD(CONF_BLINK_EVENTS_PER_DAY, "Blink Events Per Day", BLINK_EVENTS_PER_DAY, FLEX_DIAG_CONF_TYPE_CONF),
  FLEX_DIAG_CONF_TABLE_U32_ADD(DIAG_TOTAL_FLASH_COUNT, "Total Flash Count", 0, FLEX_DIAG_CONF_TYPE_DIAG),
  FLEX_DIAG_CONF_TABLE_U32_ADD(DIAG_LAST_SEQUENCE_NUMBER, "Last Sequence Number", 0, FLEX_DIAG_CONF_TYPE_PERSIST_DIAG),
FLEX_DIAG_CONF_TABLE_END();
// clang-format on

static time_t Blinky(void) {
  uint32_t sequence_number = 0;
  if (FLEX_DiagConfValueRead(DIAG_LAST_SEQUENCE_NUMBER, &sequence_number) != 0) {
    printf("Failed to read the sequence number!\n");
  }

  uint32_t flash_count = 0;
  if (FLEX_DiagConfValueRead(DIAG_TOTAL_FLASH_COUNT, &flash_count) != 0) {
    printf("Failed to read the flash count!\n");
  }

  for (uint8_t i = 0; i < NUMBER_OF_FLASHES; ++i) {
    FLEX_LEDGreenStateSet(FLEX_LED_ON);
    printf("Green LED On\n");
    FLEX_Sleep(LED_ON_TIME_SEC);

    FLEX_LEDGreenStateSet(FLEX_LED_OFF);
    printf("Green LED Off\n");
    FLEX_Sleep(LED_OFF_TIME_SEC);

    ++flash_count;
  }

  sequence_number++;
  if (FLEX_DiagConfValueWrite(DIAG_LAST_SEQUENCE_NUMBER, &sequence_number) != 0) {
    printf("Failed to write the sequence number!\n");
  };

  if (FLEX_DiagConfValueWrite(DIAG_TOTAL_FLASH_COUNT, &flash_count) != 0) {
    printf("Failed to write the flash count!\n");
  };

  uint32_t blink_events_per_day = BLINK_EVENTS_PER_DAY;
  if (FLEX_DiagConfValueRead(CONF_BLINK_EVENTS_PER_DAY, &blink_events_per_day) != 0) {
    printf("Failed to read blink_interval_min!\n");
  }

  return (FLEX_TimeGet() + 24 * 3600 / blink_events_per_day);
}

// Handle the update made to the CONF_BLINK_EVENTS_PER_DAY configuration value.
static void blink_events_per_day_handler(const void *const value) {
  if (value == NULL) {
    return;
  }

  const uint32_t *const blink_events_per_day = value;

  // Schedule the Blinky job to run at the new interval
  FLEX_JobSchedule(Blinky, (FLEX_TimeGet() + 24 * 3600 / *blink_events_per_day));
}

void FLEX_AppInit() {
  printf("%s\n", APPLICATION_NAME);

  // Creat a handler to run the tracker job if the measurement interval is changed
  FLEX_DiagConfValueNotifyHandlerSet(CONF_BLINK_EVENTS_PER_DAY, blink_events_per_day_handler);

  // Schedule the Blinky job to run.
  FLEX_JobSchedule(Blinky, FLEX_ASAP());
}

//! [CODE]
