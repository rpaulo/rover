# rover

rover is a small C program to control a FreeBSD-based rover.  It uses of [libiic](https://bitbucket.org/rpaulo/libiic) and [libgpio](https://bitbucket.org/rpaulo/libgpio).

This rover is based on:

* BeagleBone Black
* Adafruit Beagle Proto Cape
* [TB6612FNG motor driver](https://www.sparkfun.com/products/9457)
* Two 700mA Li-Po batteries
* Two [Li-Po fuel gauges](https://www.sparkfun.com/products/10617)
* [Li-Po charger](https://www.sparkfun.com/products/10401)
* [Li-Po charger/booster](https://www.sparkfun.com/products/11231)
* [HIH6130 temperature / relative humidity sensor](https://www.sparkfun.com/products/11295)
* Realtek USB Wi-Fi 

The basic functions of the rover are:

* control the DC motors
* monitor temperature
* monitor battery charge

The interface is implemented in curses.  Here's a screenshot:

![alt text](http://people.freebsd.org/~rpaulo/rover_ui.png)