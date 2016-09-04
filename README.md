# Arduino Optical Energy Meter (NRF24L01 + BPW40)

Energy meter that uses optical port of installed energy meter (_that red blinking LED_) and sends data over nRF24 module.

## Hardware used

- [Arduino Nano v3](https://www.arduino.cc/en/Main/ArduinoBoardNano)
- [nRF24L01](http://www.nordicsemi.com/eng/Products/2.4GHz-RF/nRF24L01)
  - [RF24 Lib from TMRh20](https://github.com/tmrh20/RF24/) (_:book: library used_)
  - [Getting started with nRF by Bryan Thompson](http://www.madebymarket.com/blog/dev/getting-started-with-nrf24L01-and-arduino.html) (_article that helped alot_)
  - [nRF24L01 on Arduino Playground](http://playground.arduino.cc/InterfacingWithHardware/Nrf24L01)
  - [2.4G Wireless nRF24L01p | ELECFreaks](http://www.elecfreaks.com/wiki/index.php?title=2.4G_Wireless_nRF24L01p)
- I2C 0.96" (SSD1306 128x64) Blue OLED display
  - [u8glib](https://github.com/olikraus/u8glib) (_:book: library used_)
- [BPW40 phototransistor](http://www.tme.eu/en/details/bpw40/transmitting-and-receiving-ir-elements/) (_&lambda; max 780nm_)
- 10 _kOhm_ resistor (_for BPW40_)
- 40 _uF_/10 _V_ capacitor (_for nRF24L01_)

## Features:

- Uses NO interrupts. Assumed that frequency is enough to determine each impulse.
- Provided determination of unusually long impulses
- Provided count of 1kW/h single cycle in packet (_allows to decide about missed packets if throttling is used_)
- Packet contains:
  - Current consumption rate (_kWh_) - 0.01 (_float, 2 decimals, accuracy 0.01_)
  - Consumed amount (_kWh_)  - 123456.05 (_float, 2 decimals, accuracy 0.05_)
  - Impulses count (_single cycle_) - 1234 (_integer_)

## TODO:

- Configurable number of impulses per kW/h during operation (**a feature to implement**)
- Possbility of adding current meter readings during operation to sync with actual values (**a feature to implement**)
- Real-time configurable packet throttling

This hardware is meant to be used as separate component of Home Automation Network.

> Project inspired by inability to replace installed energy meter and strong will towards Home Automation.
> Any corrections, improvements, optimzations are highly appreciated.