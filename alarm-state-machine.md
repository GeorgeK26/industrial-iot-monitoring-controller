# Alarm State Machine

States:

- NORMAL
- WARNING
- CRITICAL
- SENSOR_FAULT

Transitions:

- NORMAL -> WARNING at load >= 70%
- NORMAL -> CRITICAL at load >= 90%
- WARNING -> CRITICAL at load >= 90%
- WARNING -> NORMAL below 68%
- CRITICAL -> WARNING below 90% but at/above 70%
- CRITICAL -> NORMAL below 88% and below 70%
- Any state -> SENSOR_FAULT when DHT or ADC fault is detected
- SENSOR_FAULT -> NORMAL/WARNING/CRITICAL after the fault clears, based on load

Critical and sensor-fault states activate the LED and buzzer.
