#include <wiringPi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <wiringPiSPI.h>
#include <pthread.h>

#define MAXTIMINGS 85
#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "demofarmdb"

#define MAX 100
#define loops 50

#define LIGHTSEN_OUT 2
#define RED 7
#define GREEN 8
#define BLUE 9
#define FAN 22

#define CS_MCP3208 8
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

int buffer2[MAX];
int buffer3[MAX];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;
int seq = 0;
int fput = 0;

int ret_humid, ret_temp;

//static int DHTPIN = 7;
static int DHTPIN = 11;

static int dht22_dat[5] = { 0,0,0,0,0 };

pthread_cond_t empty, fill, fill2, fin;
pthread_mutex_t mutex;

int read_mcp3208_adc(unsigned char adcChannel)
{
	unsigned char buff[3];
	int adcValue = 0;

	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
	buff[1] = ((adcChannel & 0x07) << 6);
	buff[2] = 0x00;

	digitalWrite(CS_MCP3208, 0);
	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

	buff[1] = 0x0f & buff[1];
	adcValue = (buff[1] << 8) | buff[2];

	digitalWrite(CS_MCP3208, 1);

	return adcValue;
}

void Bpluspinmodeset(void)
{
	pinMode(LIGHTSEN_OUT, INPUT);

	pinMode(FAN, OUTPUT);
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE, OUTPUT);
}

int wiringPicheck(void)
{
	if (wiringPiSetup() == -1)
	{
		fprintf(stdout, "Unable to start wiringPi : %s\n", strerror(errno));
		return 1;
	}
}

void put(int value, int value2)
{
	buffer2[fill_ptr] = value;
	buffer3[fill_ptr] = value2;
	fill_ptr = (fill_ptr + 1) % MAX;
	count++;
	seq = 1;
	fput = 0;
	printf("put temp : %d  light : %d\n", value, value2);
	printf("\n");
}

int get()
{
	int tmp = buffer2[use_ptr];

	printf("get temp : %d \n", tmp);
	return tmp;
}

int get2()
{
	int light = buffer3[use_ptr];

	use_ptr = (use_ptr + 1) % MAX;
	count--;
	printf("get light : %d \n", light);
	printf("\n");
	seq = 0;
	return light;
}

int de_fan()
{
	int tmp = buffer2[use_ptr];
	printf("decide fan : %d\n",tmp);
	seq = 2;
	return tmp;
}

int de_led()
{
	int light = buffer3[use_ptr];
	printf("decide led : %d\n", light);
	seq = 3;
	fput = 1;
	return light;
}

void act_fan_on()
{
	if (wiringPicheck())
		printf("Fail");
	pinMode(FAN, OUTPUT);
	digitalWrite(FAN, 1);
}

void act_fan_off()
{
	if (wiringPicheck())
		printf("Fail");
	pinMode(FAN, OUTPUT);
	digitalWrite(FAN, 0);
}

void act_rgbled_on()
{
	if (wiringPicheck())printf("Fail");
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE, OUTPUT);

	digitalWrite(BLUE, 0);
	digitalWrite(RED, 1);
	digitalWrite(GREEN, 0);
}

void act_rgbled_off()
{
	if (wiringPicheck())printf("Fail");
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE, OUTPUT);

	digitalWrite(BLUE, 0);
	digitalWrite(RED, 0);
	digitalWrite(GREEN, 0);
}

static uint8_t sizecvt(const int read)
{
	/* digitalRead() and friends from wiringpi are defined as returning a value
	< 256. However, they are returned as int() types. This is a safety function */

	if (read > 255 || read < 0)
	{
		printf("Invalid data from wiringPi library\n");
		exit(EXIT_FAILURE);
	}
	return (uint8_t)read;
}

int read_dht22_dat()
{
	uint8_t laststate = HIGH;
	uint8_t counter = 0;
	uint8_t j = 0, i;

	dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

	// pull pin down for 18 milliseconds
	pinMode(DHTPIN, OUTPUT);
	digitalWrite(DHTPIN, HIGH);
	delay(10);
	digitalWrite(DHTPIN, LOW);
	delay(18);
	// then pull it up for 40 microseconds
	digitalWrite(DHTPIN, HIGH);
	delayMicroseconds(40);
	// prepare to read the pin
	pinMode(DHTPIN, INPUT);

	// detect change and read data
	for (i = 0; i< MAXTIMINGS; i++) {
		counter = 0;
		while (sizecvt(digitalRead(DHTPIN)) == laststate) {
			counter++;
			delayMicroseconds(1);
			if (counter == 255) {
				break;
			}
		}
		laststate = sizecvt(digitalRead(DHTPIN));

		if (counter == 255) break;

		// ignore first 3 transitions
		if ((i >= 4) && (i % 2 == 0)) {
			// shove each bit into the storage bytes
			dht22_dat[j / 8] <<= 1;
			if (counter > 50)
				dht22_dat[j / 8] |= 1;
			j++;
		}
	}

	// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	// print it out if data is good
	if ((j >= 40) &&
		(dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF))) {
		float t, h;

		h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
		h /= 10;
		t = (float)(dht22_dat[2] & 0x7F) * 256 + (float)dht22_dat[3];
		t /= 10.0;
		if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

		ret_humid = (int)h;
		ret_temp = (int)t;
		//printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		//printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);

		return ret_temp;
	}
	else
	{
		printf("Data not good, skip\n");
		return 0;
	}
}

int get_light_sensor(void)
{
	if (wiringPiSetup() <0)
	{
		fprintf(stderr, "Unable to setup wiringPi : %s\n", strerror(errno));
		return 1;
	}

	if (digitalRead(LIGHTSEN_OUT))
		return 1;
	else
		return 0;
}

void *sensor(void *arg)
{
	int i;
	int received_temp;
	int received_light;
	unsigned char adcChannel_light = 0;

	pinMode(CS_MCP3208, OUTPUT);

	if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
	{
		fprintf(stdout, "wiringPiSPISetup Failed : %s\n", strerror(errno));
	}

	for (i = 0; i < loops; i++)
	{
		while (read_dht22_dat() == 0)
		{
			delay(3000);
		}
		delay(1000);
		received_temp = ret_temp;
		received_light = read_mcp3208_adc(adcChannel_light);

		pthread_mutex_lock(&mutex);
		while (count == MAX && fput == 0)
		{
			pthread_cond_wait(&empty, &mutex);
		}
		put(received_temp, received_light);
		pthread_cond_signal(&fill);
		pthread_mutex_unlock(&mutex);
	}
}

void *db(void * arg)
{
	int i;
	char query[1024];

	for (i = 0; i < loops; i++)
	{
		pthread_mutex_lock(&mutex);
		while (count == 0 || seq != 3)
			pthread_cond_wait(&fin, &mutex);
		int tmp = get();
		int light = get2();
		pthread_cond_signal(&empty);
		pthread_mutex_unlock(&mutex);

		delay(10000);
		sprintf(query, "insert into th1 values (now(), %d, 0, %d)", tmp,light);
		if (mysql_query(connector, query))
		{
			fprintf(stderr, "%s\n", mysql_error(connector));
			printf("Write DB error");
		}
		
		printf("Temperature : %d  light : %d \n", tmp, light);
	}
}

void *fan(void *arg)
{
	int i;
	for (i = 0; i < loops; i++)
	{
		pthread_mutex_lock(&mutex);
		while (count == 0 || seq != 1)
			pthread_cond_wait(&fill, &mutex);
		int tmp = de_fan();
		if (tmp > 20)
		{
			printf("fan on\n");
			act_fan_on();
			delay(5000);
			printf("fan off\n");
			act_fan_off();
			printf("\n");
		}
		
		pthread_cond_signal(&fill2);
		pthread_mutex_unlock(&mutex);
	}
}

void *led(void *arg)
{
	int i;
	for (i = 0; i < loops; i++)
	{
		pthread_mutex_lock(&mutex);
		while (count == 0 || seq != 2)
			pthread_cond_wait(&fill2, &mutex);
		int light = de_led();
		if (light > 800)
		{
			printf("led on\n");
			act_rgbled_on();
			delay(5000);
			printf("led off\n");
			act_rgbled_off();
			printf("\n");
		}

		pthread_cond_signal(&fin);
		pthread_mutex_unlock(&mutex);
	}
}

int main(int argc, char *argv[])
{
	pthread_t p1, p2, p3, p4;
	printf("main : begin \n");

	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE);

	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	Bpluspinmodeset();

	connector = mysql_init(NULL);
	if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
	{
		fprintf(stderr, "%s\n", mysql_error(connector));
		return 0;
	}
	printf("MySQL(rpidb) opened.\n");

	pthread_create(&p1, NULL, sensor, "A");
	pthread_create(&p2, NULL, fan, "B");
	pthread_create(&p3, NULL, led, "C");
	pthread_create(&p4, NULL, db, "D");

	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
	pthread_join(p3, NULL);
	pthread_join(p4, NULL);
	printf("main : done with both \n");

	mysql_close(connector);
	return 0;
}
