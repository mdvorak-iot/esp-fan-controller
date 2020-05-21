# ESP32 Fan Controller

![platformio build](https://github.com/mdvorak-iot/esp-fan-controller/workflows/platformio%20build/badge.svg)

This is code for single-purpose built controller for driving PWM fans according to temperature.

When connected to WiFi, it exposes metrics via HTTP, including trivial web page.

### My config

```json
{
  "control_pin": 2,
  "rpm_pins": [
    32,
    33,
    34,
    35,
    36,
    4
  ],
  "sensors_pin": 15,
  "primary_sensor_address": "383C01B556644428",
  "sensors": [
    {
      "address": "42011319F2DCD628",
      "name": "Water Inlet"
    },
    {
      "address": "383C01B556644428",
      "name": "Water Outlet"
    },
    {
      "address": "743C01B55629DF28",
      "name": "Ambient"
    }
  ],
  "low_threshold_celsius": 28,
  "high_threshold_celsius": 32,
  "cpu_threshold_celsius": 78,
  "cpu_poll_interval_seconds": 5,
  "low_threshold_duty_percent": 30,
  "high_threshold_duty_percent": 97,
  "hardware_name": "Radiator",
  "cpu_query_url": "http://nas.mdvorak.org:19090/api/v1/query?query=scalar%28round%28avg_over_time%28ohm_cpu_celsius%7Bjob%3D%22mikee_pc%22%2Csensor%3D%22Core+%28Tctl/Tdie%29%22%7D%5B15s%5D%29,0.1%29%29"
}
```
