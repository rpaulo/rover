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
#ifdef BBB
#include "iic.h"
#endif

int going_up, going_down, going_left, going_right;
int speed = 5;

pthread_cond_t update_cond;
pthread_mutex_t update_mtx;

void
terminate(int sig)
{
	endwin();
	exit(0);
}
#ifdef BBB
void
max17043_init(int fd)
{
	uint8_t buf[1];
	const int slave = 0x36;
	struct iic_msg msg[3];
	struct iic_rdwr_data rdwr;

	/* Reset the max17043 */
	buf[0] = 0xfe;
	msg[0].slave = slave;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = buf;
	buf[0] = 0x00;
	msg[1].slave = slave;
	msg[1].flags = IIC_M_WR;
	msg[1].len = 1;
	msg[1].buf = buf;
	buf[0] = 0x54;
	msg[2].slave = slave;
	msg[2].flags = IIC_M_WR;
	msg[2].len = 1;
	msg[2].buf = buf;
	rdwr.nmsgs = 3;
	rdwr.msgs = msg;
	ioctl(fd, I2CRDWR, &rdwr);

	/* Enable quick start mode */
	buf[0] = 0x06;
	msg[0].slave = slave;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = buf;
	buf[0] = 0x40;
	msg[1].slave = slave;
	msg[1].flags = IIC_M_WR;
	msg[1].len = 1;
	msg[1].buf = buf;
	buf[0] = 0x00;
	msg[2].slave = slave;
	msg[2].flags = IIC_M_WR;
	msg[2].len = 1;
	msg[2].buf = buf;
	rdwr.nmsgs = 3;
	rdwr.msgs = msg;
	ioctl(fd, I2CRDWR, &rdwr);
}

float
max17043_vcell(int fd)
{
	uint8_t buf[2];
	const int slave = 0x36;
	struct iic_msg msg[2];
	struct iic_rdwr_data rdwr;

retry:
	buf[0] = 0x02;
	buf[1] = 0;
	msg[0].slave = slave;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = buf;
	msg[1].slave = slave;
	msg[1].flags = IIC_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf;
	rdwr.nmsgs = 2;
	rdwr.msgs = msg;
	if (ioctl(fd, I2CRDWR, &rdwr) < 0) {
		if (errno == EBUSY)
			goto retry;
		return 0;
	}

	return 0.00125 * ((buf[1] | (buf[0] << 8)) >> 4);
}

float
max17043_soc(int fd)
{
	const int slave = 0x36;
	uint8_t buf[2];
	struct iic_msg msg[2];
	struct iic_rdwr_data rdwr;
	float soc;

retry:
	buf[0] = 0x04;
	buf[1] = 0;
	msg[0].slave = slave;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = buf;
	msg[1].slave = slave;
	msg[1].flags = IIC_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf;
	rdwr.nmsgs = 2;
	rdwr.msgs = msg;
	if (ioctl(fd, I2CRDWR, &rdwr) < 0) {
		if (errno == EBUSY)
			goto retry;
		return 0;
	}

	soc = (float)buf[0] + (buf[1] / 256.0);
	if (soc > 100.0)
		return 100.0;
	else
		return soc;
}
#endif

const char *
get_battery_status(int status)
{
	if (status < 0)
		return "DISCHARGING";
	else if (status > 0)
		return "CHARGING   ";
	return         "<UNKNOWN>  ";
}

int
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
void *
query_main_battery(void *arg)
{
#ifdef BBB
	int fd;
	const struct timespec ts = { 1, 0 };
	float prev_charge = -1;

	fd = *(int *)arg;
	max17043_init(fd);
	for (;;) {
		main_bat_v = max17043_vcell(fd);
		main_bat_charge = max17043_soc(fd);
		main_bat_status = set_battery_status(prev_charge,
		    main_bat_charge);
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
		prev_charge = main_bat_charge;
		nanosleep(&ts, NULL);
	}
#endif

	return NULL;
}

float motor_bat_v = 0;
float motor_bat_charge = 0;
int motor_bat_status = 0;
void *
query_motor_battery(void *arg)
{
#ifdef BBB
	int fd;
	const struct timespec ts = { 1, 0 };
	float prev_charge = -1;

	fd = *(int *)arg;
	max17043_init(fd);
	for (;;) {
		motor_bat_v = max17043_vcell(fd);
		motor_bat_charge = max17043_soc(fd);
		motor_bat_status = set_battery_status(prev_charge,
		    motor_bat_charge);
		pthread_mutex_lock(&update_mtx);
		pthread_cond_signal(&update_cond);
		pthread_mutex_unlock(&update_mtx);
		prev_charge = motor_bat_charge;
		nanosleep(&ts, NULL);
	}
#endif

	return NULL;
}

float temperature = 0.0;
float rh = 0.0;
void *
query_temperature(void *arg)
{
#ifdef BBB
	struct iic_msg msg[1];
	struct iic_rdwr_data rdwr;
	uint8_t buf[4];
	int fd;
	uint8_t rh_msb, rh_lsb, temp_msb, temp_lsb;
	const struct timespec ts = { 1, 0 };

	fd = *(int *)arg;
	for (;;) {
		bzero(&buf, sizeof(buf));
		msg[0].slave = 0x27;
		msg[0].flags = IIC_M_RD;
		msg[0].len = sizeof(buf);
		msg[0].buf = buf;
		rdwr.nmsgs = 1;
		rdwr.msgs = msg;
		if (ioctl(fd, I2CRDWR, &rdwr) == 0) {
			rh_msb = buf[0] & 0x3f;
			rh_lsb = buf[1];
			temp_msb = buf[2];
			temp_lsb = buf[3];
			rh = ((rh_msb << 8) | rh_lsb) * 6.10e-3;
			temperature = (((temp_msb << 8) | temp_lsb) / 4) * 
			    1.007e-2 - 40.0;
			pthread_mutex_lock(&update_mtx);
			pthread_cond_signal(&update_cond);
			pthread_mutex_unlock(&update_mtx);
		} else {
			rh = 0;
			temperature = 0;
		}
		nanosleep(&ts, NULL);
	}
#endif
	return NULL;
}

void *
handle_input(void *arg)
{
	int c;

	while ((c = getch()) != ERR) {
		switch (c) {
		case 'q':
			terminate(0);
			break;
		case '-':
			if (speed > 1)
				speed--;
			break;
		case '+':
			if (speed < 10)
				speed++;
			break;
		case KEY_UP:
			going_up = 1;
			break;
		case KEY_DOWN:
			going_down = 1;
			break;
		case KEY_LEFT:
			going_left = 1;
			break;
		case KEY_RIGHT:
			going_right = 1;
			break;
		}
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
	int iic1_fd, iic2_fd;
	pthread_t thread;

	going_up = going_down = going_left = going_right = 0;
	iic1_fd = open("/dev/iic1", O_RDWR);
	if (iic1_fd < 0)
		warn("open");
	iic2_fd = open("/dev/iic2", O_RDWR);
	if (iic2_fd < 0)
		warn("open");

	pthread_cond_init(&update_cond, NULL);
	pthread_mutex_init(&update_mtx, NULL);
	pthread_create(&thread, NULL, query_main_battery, &iic2_fd);
	pthread_create(&thread, NULL, query_motor_battery, &iic1_fd);
	pthread_create(&thread, NULL, query_temperature, &iic2_fd);

	signal(SIGINT, terminate);
	initscr();
	curs_set(0);
	keypad(stdscr, TRUE);
	nonl();
	cbreak();
	noecho();
	box(stdscr, 0, 0);

	pthread_create(&thread, NULL, handle_input, NULL);

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
		if (going_up) {
			move(8, 6);
			printw("^");
		}
		move(9, 6);
		printw("|");
		if (going_left) {
			move(9, 3);
			printw("<-");
		}
		if (going_right) {
			move(9, 8);
			printw("->");
		}
		if (going_down) {
			move(10, 6);
			printw("v");
		}
		move(9, 30);
		printw("Use the directional keys to move the rover.");
		refresh();
		pthread_cond_wait(&update_cond, &update_mtx);
		pthread_mutex_unlock(&update_mtx);
	}
	
	terminate(0);
}
