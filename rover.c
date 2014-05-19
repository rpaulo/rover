/*-
 * Copyright (c) 2014 Rui Paulo <rpaulo@felyko.com>                        
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <fcntl.h>
#include <signal.h>
#include <curses.h>
#include <stdlib.h>
#include <pthread.h>
#include <err.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <libgpio.h>
#include <libiic.h>

int going_forward, going_backwards, going_left, going_right;
int speed = 5;

pthread_cond_t update_cond;
pthread_mutex_t update_mtx;

#define	GPIO_AIN1	87
#define	GPIO_AIN2	89
#define	GPIO_BIN1	86
#define	GPIO_BIN2	88

#define	MAX17043_SLAVE	0x36
#define	MAX17043_VCELL	0x02
#define	MAX17043_SOC	0x04
#define	MAX17043_MODE	0x06
#define	MAX17043_CMD	0xfe

#define	HIH6130_SLAVE	0x27

static void
terminate(int sig)
{
	endwin();
	exit(0);
}

static void
max17043_init(iic_handle_t handle)
{
	/* Reset the max17043 */
	iic_write_1(handle, MAX17043_SLAVE, MAX17043_CMD);
	iic_write_2(handle, MAX17043_SLAVE, 0x5400);

	/* Enable quick start mode */
	iic_write_1(handle, MAX17043_SLAVE, MAX17043_MODE);
	iic_write_2(handle, MAX17043_SLAVE, 0x4000);
}

static float
max17043_vcell(iic_handle_t handle)
{
	uint16_t v;

	iic_write_1(handle, MAX17043_SLAVE, MAX17043_VCELL);
	iic_read_2_be(handle, MAX17043_SLAVE, &v);

	return 0.00125 * (v >> 4);
}

static float
max17043_soc(iic_handle_t handle)
{
	float soc;
	uint8_t buf[2];

	iic_write_1(handle, MAX17043_SLAVE, MAX17043_SOC);
	iic_read_1(handle, MAX17043_SLAVE, &buf[0]);
	iic_read_1(handle, MAX17043_SLAVE, &buf[1]);

	soc = (float)buf[0] + (buf[1] / 256.0);
	if (soc > 100.0)
		return 100.0;
	else
		return soc;
}

static const char *
get_battery_status(int status)
{
	if (status < 0)
		return "DISCHARGING";
	else if (status > 0)
		return "CHARGING   ";
	return         "<UNKNOWN>  ";
}

static int
set_battery_status(float prev_charge, float cur_charge)
{
	if (prev_charge == -1)
		return 0;
	else if (prev_charge < cur_charge)
		return 1;
	else if (prev_charge > cur_charge)
		return -1;
	return 0;
}

float main_bat_v = 0;
float main_bat_charge = 0;
int main_bat_status = 0;
static void *
query_main_battery(void *arg)
{
	iic_handle_t handle;
	const struct timespec ts = { 10, 0 };
	float prev_charge = -1;

	handle = *(iic_handle_t *)arg;
	max17043_init(handle);
	for (;;) {
		main_bat_v = max17043_vcell(handle);
		main_bat_charge = max17043_soc(handle);
		if (prev_charge != main_bat_charge)
			main_bat_status = set_battery_status(prev_charge,
			    main_bat_charge);
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
		prev_charge = main_bat_charge;
		nanosleep(&ts, NULL);
	}

	return NULL;
}

float motor_bat_v = 0;
float motor_bat_charge = 0;
int motor_bat_status = 0;
static void *
query_motor_battery(void *arg)
{
	iic_handle_t handle;
	const struct timespec ts = { 15, 0 };
	float prev_charge = -1;

	handle= *(iic_handle_t *)arg;
	max17043_init(handle);
	for (;;) {
		motor_bat_v = max17043_vcell(handle);
		motor_bat_charge = max17043_soc(handle);
		if (prev_charge != motor_bat_charge)
			motor_bat_status = set_battery_status(prev_charge,
			    motor_bat_charge);
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
		prev_charge = motor_bat_charge;
		nanosleep(&ts, NULL);
	}

	return NULL;
}

float temperature = 0.0;
float rh = 0.0;
static void *
query_temperature(void *arg)
{
	iic_handle_t handle;
	uint16_t rh_reg, temp_reg;
	uint32_t reg;
	const struct timespec ts = { 6, 0 };
	int i;

	handle = *(iic_handle_t *)arg;
	for (;;) {
		if (iic_read_4_be(handle, HIH6130_SLAVE, &reg) < 0)
			reg = 0;
		rh_reg = reg >> 16;
		temp_reg = reg & 0xffff;
		rh = (rh_reg & 0x3fff) * 6.10e-3;
		temperature = (temp_reg / 4) * 1.007e-2 - 40.0;
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
		nanosleep(&ts, NULL);
	}

	return NULL;
}

static void *
handle_input(void *arg)
{
	int c;
	gpio_handle_t ghandle;
	int speed_pwm = 500 + speed * 100;

	ghandle = *(gpio_handle_t *)arg;

	while ((c = getch()) != ERR) {
		switch (c) {
		case 'q':
			gpio_pin_set(ghandle, GPIO_AIN1, 0);
			gpio_pin_set(ghandle, GPIO_AIN2, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 0);
			terminate(0);
			break;
		case ' ':
			going_left = 0;
			going_right = 0;
			going_forward = 0;
			going_backwards = 0;
			break;
		case '-':
			if (speed > 1)
				speed--;
			speed_pwm = 500 + speed * 100;
			break;
		case '+':
			if (speed < 5)
				speed++;
			speed_pwm = 500 + speed * 100;
			break;
		case KEY_UP:
			going_forward = 1;
			going_backwards = 0;
			going_left = 0;
			going_right = 0;
			break;
		case KEY_DOWN:
			going_backwards = 1;
			going_forward = 0;
			going_left = 0;
			going_right = 0;
			break;
		case KEY_LEFT:
			going_left = 1;
			going_right = 0;
			going_backwards = 0;
			going_forward = 0;
			break;
		case KEY_RIGHT:
			going_right = 1;
			going_left = 0;
			going_backwards = 0;
			going_forward = 0;
			break;
		}
		if (going_backwards) {
			gpio_pin_set(ghandle, GPIO_AIN1, 0);
			gpio_pin_set(ghandle, GPIO_AIN2, 1);
			gpio_pin_set(ghandle, GPIO_BIN1, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 1);
		} else if (going_forward) {
			gpio_pin_set(ghandle, GPIO_AIN1, 1);
			gpio_pin_set(ghandle, GPIO_AIN2, 0);
			gpio_pin_set(ghandle, GPIO_BIN1, 1);
			gpio_pin_set(ghandle, GPIO_BIN2, 0);
		} else if (going_right) {
			gpio_pin_set(ghandle, GPIO_AIN1, 0);
			gpio_pin_set(ghandle, GPIO_AIN2, 0);
			gpio_pin_set(ghandle, GPIO_BIN1, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 1);
		} else if (going_left) {
			gpio_pin_set(ghandle, GPIO_AIN1, 0);
			gpio_pin_set(ghandle, GPIO_AIN2, 1);
			gpio_pin_set(ghandle, GPIO_BIN1, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 0);
		} else {
			gpio_pin_set(ghandle, GPIO_AIN1, 0);
			gpio_pin_set(ghandle, GPIO_AIN2, 0);
			gpio_pin_set(ghandle, GPIO_BIN1, 0);
			gpio_pin_set(ghandle, GPIO_BIN2, 0);
		}

		sysctlbyname("dev.am335x_pwm.2.dutyA", NULL, NULL,
		    &speed_pwm, sizeof(speed_pwm));
		sysctlbyname("dev.am335x_pwm.2.dutyB", NULL, NULL,
		    &speed_pwm, sizeof(speed_pwm));
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int c;
	iic_handle_t iic1, iic2;
	pthread_t thread;
	gpio_handle_t ghandle;
	gpio_config_t config;

	going_forward = going_backwards = going_left = going_right = 0;
	iic1 = iic_open(1);
	if (iic1 == IIC_INVALID_HANDLE)
		warn("iic_open");
	iic2 = iic_open(2);
	if (iic2 == IIC_INVALID_HANDLE)
		warn("iic_open");
	ghandle = gpio_open(0);
	gpio_pin_output(ghandle, GPIO_AIN1);
	gpio_pin_set(ghandle, GPIO_AIN1, 0);
	gpio_pin_output(ghandle, GPIO_AIN2);
	gpio_pin_set(ghandle, GPIO_AIN2, 0);
	gpio_pin_output(ghandle, GPIO_BIN1);
	gpio_pin_set(ghandle, GPIO_BIN1, 0);
	gpio_pin_output(ghandle, GPIO_BIN2);
	gpio_pin_set(ghandle, GPIO_BIN2, 0);

	pthread_cond_init(&update_cond, NULL);
	pthread_mutex_init(&update_mtx, NULL);
	pthread_create(&thread, NULL, query_main_battery, &iic2);
	pthread_create(&thread, NULL, query_motor_battery, &iic1);
	pthread_create(&thread, NULL, query_temperature, &iic2);

	signal(SIGINT, terminate);
	initscr();
	curs_set(0);
	keypad(stdscr, TRUE);
	nonl();
	cbreak();
	noecho();
	box(stdscr, 0, 0);

	pthread_create(&thread, NULL, handle_input, &ghandle);

	for (;;) {
		pthread_mutex_lock(&update_mtx);
		move(1, 2);
		printw("Main Battery Voltage: %.2fV", main_bat_v);
		move(2, 2);
		printw("Main Battery Charge: %.2f%%", main_bat_charge);
		move(3, 2);
		printw("Main Battery is %s", 
		    get_battery_status(main_bat_status));
		move(1, 40);
		printw("Motor Battery Voltage: %.2fV", motor_bat_v);
		move(2, 40);
		printw("Motor Battery Charge: %.2f%%", motor_bat_charge);
		move(3, 40);
		printw("Motor Battery is %s",
		    get_battery_status(motor_bat_status));
		move(5, 2);
		printw("Ambient temperature: %.2fC", temperature);
		move(6, 2);
		printw("Ambient RH: %.2f%%", rh);
		move(5, 40);
		printw("Speed: %d ", speed);
		move(8, 6);
		if (going_forward)
			printw("^");
		else
			printw(" ");
		move(9, 6);
		printw("|");
		move(9, 3);
		if (going_left)
			printw("<-");
		else
			printw("  ");
		move(9, 8);
		if (going_right)
			printw("->");
		else
			printw("  ");
		move(10, 6);
		if (going_backwards)
			printw("v");
		else
			printw("  ");
		move(10, 22);
		printw("Use the directional keys to move the rover.");
		refresh();
		pthread_cond_wait(&update_cond, &update_mtx);
		pthread_mutex_unlock(&update_mtx);
	}
	
	terminate(0);
}
