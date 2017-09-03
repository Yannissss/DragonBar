#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "string.h"
#include "stddef.h"
#include "locale.h"
#include "pthread.h"
#include "unistd.h"
#include "assert.h"

// Configuration
#define SLEEP_INTERVAL 300 // in milliseconds
#define MAIN_DRIVE     "/dev/sda2"
#define INTERFACE      "wlp2s0"

/*
 *   Helpers
 */
char * exec(size_t len, char * cmd)
{
    FILE * fp;
    fp = popen(cmd, "r");
    char * buffer = malloc(len * sizeof(char));
    while (fgets(buffer, len - 1, fp) != NULL);
    pclose(fp);
    return buffer;
}

char * split(char * source, int index)
{
	char * buffer = strdup(source);
	char * arr_buffer[32];
	int i = 0;
	char * token;
	while ((token = strsep(&buffer, " ")) != NULL)
	{
		if (strcmp(token, "") != 0)
		{
				  arr_buffer[i] = strdup(token);
				  i++;
		}
	}
	return arr_buffer[index];
}

/*
 * Net mutex
 */
float bandwidth;
pthread_mutex_t lock;

/*
 * Net utils
 */
void * netinfo(void * arg)
{
	while(1)
	{
		FILE * fp;
		char * buffer = malloc(64 * sizeof(char));
		fp = fopen("/sys/class/net/wlp2s0/statistics/rx_bytes", "r");
		
		// Calculate current download bandwidth
		float rx_start = 0;
		fscanf(fp, "%f", &rx_start);
		fclose(fp);
		
		usleep(250000); // Wait 0.25 s
		fp = fopen("/sys/class/net/" INTERFACE "/statistics/rx_bytes", "r");
		float rx_end = 0;
        fscanf(fp, "%f", &rx_end);
        fclose(fp);
		free(buffer);

		// Modify bandiwidth
		pthread_mutex_lock(&lock);
		bandwidth = (rx_end - rx_start) / 250;
		pthread_mutex_unlock(&lock);
	}
}

int main(int argc, char const *argv[])
{
	// Setting up locale
	setlocale(LC_TIME, "fr_FR.UTF-8");

	// Start netinfo thread
	assert(pthread_mutex_init(&lock, NULL) == 0);
	pthread_t netstat;
	pthread_create(&netstat, NULL, netinfo, NULL);

	// Setup i3 bar protocol
	printf("{ \"version\": 1 } \n");
	printf("[ \n");
	printf("[] \n");

    // Update loop
    struct tm * now;
	time_t secondsSinceEpoch;
	while (1)
    {
		// Getting time		
		time(&secondsSinceEpoch);
		now = localtime(&secondsSinceEpoch);
		
		char date[64] = "";
		char time[16] = "";

		strftime(date, 64, "%a %d %B", now);
		strftime(time, 16, "%H:%M", now);

		// Command buffer
		char * cmd = "";

		// Getting disk space
		cmd = exec(256, "df -h " MAIN_DRIVE);
		char * used = "";
		used = split(cmd, 3);

		// Getting volume
		cmd = exec(132, "pactl list sinks | grep \"Volume: f\"" );
        char * volume = "";
		volume = split(cmd, 4);

		// Render
		printf(",[{\"full_text\":\" %3.1f Ko/s  %s   %s  %s  %s \"}] \n", bandwidth, volume, used, date, time);

		// Free mem
		usleep(SLEEP_INTERVAL * 1000);
    }

	// Free mem
	pthread_mutex_destroy(&lock);
    return 0;
}
