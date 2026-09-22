# HeatControl 6CH — базовий BOM

## Controller / low-voltage

- 1 × ESP32 DevKit V1 (ESP32-WROOM class)
- 1 × DS3231 RTC module with backup cell
- 7 × DS18B20 temperature sensors
- 1 × 4.7 kΩ resistor for the DS18B20 DATA pull-up
- Relay outputs: 13 total for the universal design (12 for CH1…CH6 valve pairs + 1 AUX TEN). Use a 6+8, 8+8, or 6+6 plus separate 1-channel arrangement as appropriate.
- 5 V DC power supply sized for the relay board and the chosen low-voltage architecture
- Appropriate 3.3 V regulation/power path for ESP32, sensors and RTC as required by the selected hardware
- Enclosure, terminal blocks, fusing/protection and wiring appropriate to the installation

## Optional / actuator side

- Up to 6 × 230 V motorized ball valves (2 relay outputs per valve)
- Up to 6 × heating loads (1 relay output per heater channel in the HeatControl software model)
- Where the heating load exceeds the relay/contact rating, use a correctly rated contactor/interface rather than switching the load directly with a small relay board.

## Important selection note

The software defines the 12 logical outputs, but the exact relay-board input circuit is not specified by the uploaded project. Select the board only after checking its input voltage, active level, isolation/driver circuit and contact ratings. The project template currently assumes `RELAY_ON = HIGH` and `RELAY_OFF = LOW` and labels this as provisional.

## Optional V0.15 limit-switch hardware

- 1 × EXT GPIO module — MCP23017 16-bit I/O expander module, default I²C address `0x20` (optional)
- Up to 12 × valve limit switches: 2 per valve (`OPEN` + `CLOSE`)
- Low-voltage interface/wiring suitable for the selected limit switches and enclosure

The MCP23017 shares the existing SDA/SCL bus with the DS3231. Only 12 of its 16 GPIOs are assigned to the six channels; four remain available for future use. The controller remains fully usable without this optional expansion board.


### Additional sensor
- 1 × DS18B20 for OUTDOOR/ambient temperature (7 total DS18B20).


## AUX TEN

- 1 × dedicated relay output for the controller-wide AUX TEN (GPIO5).
- 1 × additional heater/TEN load in the physical AUX zone.
- AUX is not a seventh heating channel and uses one selected CH1…CH6 sensor as its zone-temperature reference.
- For any mains load, verify relay/contact ratings and protection with a qualified electrician.
