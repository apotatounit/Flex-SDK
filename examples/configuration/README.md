# Configuration and Diagnostics Example

This example will blink the Green LED `NUMBER_OF_FLASHES` times, `CONF_BLINK_EVENTS_PER_DAY` times per day and store the total number of flashes in the `DIAG_TOTAL_FLASH_COUNT` diagnostic field. The number of times the Blinky Job has been called is stored in the `DIAG_LAST_SEQUENCE_NUMBER` persistent diagnostic field. The `CONF_BLINK_EVENTS_PER_DAY` configuration field can be updated through the FlexAssist Mobile Application whereas the `DIAG_TOTAL_FLASH_COUNT` and `DIAG_LAST_SEQUENCE_NUMBER` can be viewed but not edited from the mobile application. If FlexSense is reset the `DIAG_TOTAL_FLASH_COUNT` field will be reset while the `DIAG_LAST_SEQUENCE_NUMBER` field will not.

## FlexAssist Mobile Application
The FlexAssist Mobile Application is available on [Android](https://play.google.com/store/apps/details?id=com.myriota.binzel&hl=en) and [iOS](https://apps.apple.com/us/app/flexassist/id6474694371?uo=2)
