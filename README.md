# rover

rover is a small C program to control a FreeBSD-based rover.  It uses of [libiic](https://bitbucket.org/rpaulo/libiic) and [libgpio](https://bitbucket.org/rpaulo/libgpio).

This rover is based on:

* BeagleBone Black
* [Adafruit BeagleBone Proto Cape](https://www.adafruit.com/products/572)
* [TB6612FNG motor driver](https://www.sparkfun.com/products/9457)
* Two 700mA Li-Po batteries
* Two [MAX17043G+U Li-Po fuel gauges](https://www.sparkfun.com/products/10617)
* [Li-Po charger](https://www.sparkfun.com/products/10401)
* [Li-Po charger/booster](https://www.sparkfun.com/products/11231)
* [HIH6130 temperature / relative humidity sensor](https://www.sparkfun.com/products/11295)
* [Realtek USB Wi-Fi](https://www.adafruit.com/products/814)

The basic functions of the rover are:

* control the DC motors
* monitor temperature
* monitor battery charge

The interface is implemented in curses.  Here's a screenshot:

![alt text](http://people.freebsd.org/~rpaulo/rover_ui.png)

Connections
-----------

Everything runs of 3.3V.  The battery fuel gauge is connected to the battery and to the USB battery chargers.  There are two MAX17043G+U being used, but, since they both share the same I2C slave address, they must be connected to different I2C controllers.


Description | GPIO PIN
----------- | --------
HIH6130 SCL | P9 18
HIH6130 SDA | P9 20
MAX17043G+U SCL | P9 18
MAX17043G+U SDA | P9 20
MAX17043G+U SCL | P9 18
MAX17043G+U SDA | P9 20
TB6612FNG AIN1 | P8 29
TB6612FNG AIN2 | P8 30
TB6612FNG BIN1 | P8 27
TB6612FNG BIN2 | P8 28
TB6612FNG PWMA | P8 19 (EPWM2A)
TB6612FNG PWMB | P8 11 (EPWM2B)
TB6612FNG STBY | 3.3V (TBD)

Pictures
--------
[Top](http://people.freebsd.org/~rpaulo/fbsd_rover2.JPG)
[Side](http://people.freebsd.org/~rpaulo/fbsd_rover1.JPG)
