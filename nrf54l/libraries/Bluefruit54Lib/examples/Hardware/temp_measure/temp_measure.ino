/*********************************************************************
 Read the die temperature.

 This is the temperature of the chip itself, not the room: it sits a
 few degrees above ambient, and higher once the radio is busy.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <nrfx_temp.h>

nrfx_temp_config_t config = NRFX_TEMP_DEFAULT_CONFIG;

void setup()
{
  Serial.begin(115200);
  Serial.println("Die temperature");

  /* NULL handler = blocking mode. The non-blocking form needs the TEMP
   * interrupt, which this core does not route yet. */
  nrfx_temp_init(&config, NULL);
}

void loop()
{
  nrfx_temp_measure();                       // blocks until the reading is ready

  int32_t raw = nrfx_temp_result_get();
  Serial.printf("%.2f C\n", raw / 4.0f);     // the raw value is quarter-degrees

  delay(1000);
}
