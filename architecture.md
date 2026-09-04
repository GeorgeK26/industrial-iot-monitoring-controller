# Architecture

The controller follows this processing chain:

1. Acquire DHT22 environmental data.
2. Acquire the analog load signal.
3. Validate sensor values.
4. Filter the ADC signal with a moving average.
5. Convert the filtered ADC value to a 0–100% load estimate.
6. Run the alarm state machine.
7. Drive LED/buzzer outputs.
8. Record state transitions as events.
9. Update the OLED interface.
10. Emit JSON telemetry.
11. Monitor Wi-Fi and attempt reconnection when disconnected.

The design separates acquisition, decision-making, output control, UI, and telemetry so the project can later be expanded toward a physical industrial controller.
