# rover

rover is a small C program to control a FreeBSD-based rover.  It uses of [libiic](https://bitbucket.org/rpaulo/libiic) and [libgpio](https://bitbucket.org/rpaulo/libgpio).

This rover is based on:

* BeagleBone Black
* Adafruit Beagle Proto Cape
* [TB6612FNG motor driver](https://www.sparkfun.com/products/9457)
* Two 700mA Li-Po batteries
* Two [MAX17043G+U Li-Po fuel gauges](https://www.sparkfun.com/products/10617)
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

Connections
-----------

Everything runs of 3.3V.  The battery fuel gauge is connected to the battery and to the USB battery chargers.  There are two MAX17043G+U being used, but, since they both share the same I2C slave address, they must be connected to different I2C controllers.

<table>
	<tr><th>Description</th><th>GPIO PIN</th></tr>
	<tr><td>HIH6130 SCL</td><td>P9 18</td></tr>
	<tr><td>HIH6130 SDA</td><td>P9 20</td></tr>
	<tr><td>MAX17043G+U SCL</td><td>P9 18</td></tr>
	<tr><td>MAX17043G+U SDA</td><td>P9 20</td></tr>
	<tr><td>MAX17043G+U SCL</td><td>P9 18</td></tr>
	<tr><td>MAX17043G+U SDA</td><td>P9 20</td></tr>
	<tr><td>TB6612FNG AIN1</td><td>P8 29</td></tr>
	<tr><td>TB6612FNG AIN2</td><td>P8 30</td></tr>
	<tr><td>TB6612FNG BIN1</td><td>P8 27</td></tr>
	<tr><td>TB6612FNG BIN2</td><td>P8 28</td></tr>
	<tr><td>TB6612FNG PWMA</td><td>P8 19 (EPWMA)</td></tr>
	<tr><td>TB6612FNG PWMB</td><td>P8 11 (EPWMB)</td></tr>
	<tr><td>TB6612FNG PWMB</td><td>3.3V (TBD)</td></tr>
</table>