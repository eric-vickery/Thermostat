| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | ESP32-S31 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | --------- | ----- |

# Hello World Example

Starts a FreeRTOS task to print "Hello World".

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## How to use example

Follow detailed instructions provided specifically for this example.

Select the instructions depending on Espressif chip installed on your development board:

- [ESP32 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)


## Example folder contents

The project **hello_world** contains one source file in C language [hello_world_main.c](main/hello_world_main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.

## Optional local heater relay

By default the thermostat tells a remote unit over ESP-NOW when to turn the heater on and off.
It can also switch a relay directly from a GPIO on the board. This is off by default and untested on
hardware beyond the wiring notes below, so treat it as a starting point.

To enable it, uncomment `THERMOSTAT_RELAY` in `main/thermostat_config.h`:

```c
#define THERMOSTAT_RELAY 1
#define THERMOSTAT_RELAY_GPIO 44          // RXD0 on the Elecrow 1.28" rotary display's UART port
#define THERMOSTAT_RELAY_ACTIVE_LOW 1     // relay turns on when the pin is pulled low
```

The relay follows the heater state and starts off at boot.

**Wiring on the Elecrow 1.28" rotary display**

- Use the UART port: RXD0 (GPIO44) for the signal, `+5V` for the relay board's supply, and `GND`.
- The port has a MOSFET level shifter on each pin with 10k pull-ups to 3.3V on the ESP32 side and to 5V
  on the port side. The pin therefore behaves like an open-drain output that idles at 5V.
- That suits a 5V **active-low** opto-isolated relay board, which is the common kind: the line idles
  at 5V (relay off) and the ESP32 pulls it low to switch the relay on, sinking only a few mA.
- An **active-high** board will not work directly, because the 10k pull-up can supply only about
  0.5 mA, which is too little to drive an opto-isolator. Add a transistor stage.
- RXD0 is used instead of TXD0 because TXD0 carries boot messages on every reset, which would click
  the relay. RXD0 is an input during boot, so the relay stays off until the application starts.
- This takes over the UART0 RX pin, so the serial console no longer accepts typed input. Log output
  is unaffected.

**Safety**

- Opto-isolation only separates the logic side if the relay board has its own supply. Sharing GND
  with the thermostat removes that isolation, which is fine for switching a dry contact such as a 24V
  HVAC call for heat.
- If you switch mains voltage, check the relay's contact rating, use a proper enclosure, and keep
  mains wiring away from the low-voltage side. You are responsible for a safe installation.
