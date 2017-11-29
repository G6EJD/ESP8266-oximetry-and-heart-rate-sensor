# ESP8266-oximetry-and-heart-rate-sensor

A system that uses an ESP8266 and MAX30100 integrated pulse oximetry and heart-rate sensor.

It combines two LEDs, a photodetector, optimized optics and low-noise analog signal processing to detect pulse oximetry
and heart-rate signals.

As the LED emits light into a person’s finger, the integrated photodetector measures variations in light caused by blood volume changes and then the integrated 16‑bit analogue-to-digital converter (ADC) with programmable sample rate converts the photodetector output to a digital value. This value is then processed by an on‑chip low-noise analogue signal processor that determines the heart rate and blood oxygen content. Data is read through a serial I2C interface.

The result is a display on an ILI9341 TFT of the users blood oxygen levels and pulse rate.

Currrently only the ESP8266 operates with the MAX 3010 library.
