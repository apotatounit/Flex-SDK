# Battery API Example

An example running on Myriota's `FlexSense` board. This example demonstrates how to use the Battery API's. The `BatterySample` job will sample if the device is externally or battery-powered `FLEX_IsOnExternalPower` and the battery voltage `FLEX_GetBatteryVoltage`. `BatterySample` will be scheduled `SAMPLES_PER_DAY` times per day.
