# I2C Communication Example

This example demonstrates the FlexSense Device interfacing with a BME280 sensor using the I2C interface on the FlexSense External Interface Cable (Only implement I2C communication with a cable length of 1m or less). The example operates as follows:

1. The BME280 sensor DeviceID is read via I2C to confirm it is connected.
2. The FlexSense device writes to the sensor configuration registers to set up humidity measurements in forced mode.
3. The FlexSense device reads the latest uncompensated Humidity measurement from the BME280 sensor.
4. The latest measurement is scheduled for satellite transmission.
5. Steps 3 and 4 are repeated on a fixed schedule of MESSAGES_PER_DAY.
