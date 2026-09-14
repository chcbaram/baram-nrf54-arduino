/*********************************************************************
 Read the IMU on the XIAO nRF54LM20A Sense.

 The Sense model carries an ST LSM6DS3TR-C accelerometer + gyroscope on
 Wire1 (P0.08 SDA / P0.07 SCL) at 0x6A. The IMU object uses the same API
 as Arduino's official IMU libraries (Arduino_LSM6DS3, Arduino_BMI270_BMM150),
 so sketches written for those carry over with only the #include changed.

 One thing is different on this board: the IMU is powered from LDO1 of
 the nPM1300 PMIC, not from a GPIO. IMU.begin() switches that rail on
 for you. On a board without "Sense" the IMU is not fitted and begin()
 returns 0.

 Defaults are those of Arduino_LSM6DS3: 104 Hz, +-4 g, +-2000 dps.
 Change them after begin() with IMU.setAccelerometer() / setGyroscope().

 Output works in both Tools > Serial Monitor and Tools > Serial Plotter.
 Each line is "name:value" pairs separated by commas, which the plotter
 draws as one trace per name. Acceleration is in g and rotation in dps,
 so the two differ in scale - click a name in the plotter's legend to
 hide the traces you are not looking at.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <XIAO_nRF54LM20A.h>

void setup()
{
  Serial.begin(115200);
  delay(300);

  if ( !IMU.begin() ) {
    Serial.println("IMU not found. On a board without Sense this is expected.");
    while ( 1 ) delay(1000);
  }
}

void loop()
{
  float ax, ay, az, gx, gy, gz;

  if ( IMU.accelerationAvailable() && IMU.gyroscopeAvailable() ) {
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);

    /* No text lines in between: the plotter would read their numbers as data. */
    Serial.printf("ax:%.3f,ay:%.3f,az:%.3f,gx:%.2f,gy:%.2f,gz:%.2f\n",
                  ax, ay, az, gx, gy, gz);
  }

  delay(20);
}
