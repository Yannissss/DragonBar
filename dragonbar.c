#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "string.h"
#include "stddef.h"
#include "locale.h"
#include "pthread.h"
#include "unistd.h"
#include "assert.h"
#include <sys/time.h>

// Configuration
#define SLEEP_INTERVAL 250 // in milliseconds
#define SONG_SPEED     2 // in letters per seconds 
#define SONG_MAX_WIDTH 40
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

// Time utils
unsigned long long millis()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    unsigned long long millisecondsSinceEpoch =
        (unsigned long long)(tv.tv_sec) * 1000 +
        (unsigned long long)(tv.tv_usec) / 1000;
    return millisecondsSinceEpoch;
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
	// Flush output stream
	fflush(stdout);
	
    // Update loop
    unsigned long long T1 = 0;
    unsigned long DELTA_TIME;
	struct tm * now;
	time_t secondsSinceEpoch;
	while (1)
    {
		// Start update
		T1 = millis();

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
		
		// Getting song info
		char meta[132] = "";
	    char song[64] = "";
		strcpy(meta, exec(132, "mpc current -f \"%artist% - %title%\""));
		meta[strlen(meta) - 1] = '\0';
		if (strlen(meta) > SONG_MAX_WIDTH )
		{
			char buffer[140] = "";
			strcat(buffer, meta);
			strcat(buffer, " - ");
			int pos_start = (int) ( (unsigned long long) SONG_SPEED * T1 / 1000 ) % strlen(buffer);
			for (int i = 0;i < SONG_MAX_WIDTH; i++)
				song[i] = buffer[ (pos_start + i) % strlen(buffer) ];
		}
		else
			strcpy(song, meta);
		
		// Render
		printf(",[{\"full_text\":\"%s  %3.1f Ko/s   %s   %s  %s  %s \"}] \n", song, bandwidth, volume, used, date, time);

		// Flush output stream
		fflush(stdout);

        // End update & sleep until next update
        DELTA_TIME = (unsigned long) millis() - T1;
        int SLEEP_SCHEDULED = SLEEP_INTERVAL - DELTA_TIME;
        if (SLEEP_SCHEDULED > 0)
            usleep(SLEEP_SCHEDULED * 1000);
    }

	// Free mem
	pthread_mutex_destroy(&lock);
    return 0;
}
