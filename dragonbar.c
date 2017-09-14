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
#define SONG_MAX_WIDTH 64
#define MAIN_DRIVE     "/dev/sda2"
#define INTERFACE      "wlp2s0"

/*
 *   Helpers
 */
void exec(char * source, size_t len, char * cmd)
{
    FILE * fp;
    fp = popen(cmd, "r");
    char * buffer = malloc(len * sizeof(char));
    while (fgets(buffer, len - 1, fp) != NULL);
    pclose(fp);
	strcpy(source, buffer);
	free(buffer);
}


void query(char * dest, char * cmd, size_t len, int index)
{
	// Executing command & storing it in a buffer
	char * buffer = malloc(len * sizeof(char));
	exec(buffer, len, cmd);
	
	// Duplicating the string & creating two pointers in order two free it later
	char * str = strdup(buffer);
	int pos = -1;	
	char * token = str, * buff = str;
	while (token != NULL)
	{
		// Point forwards next token
		strsep(&buff, " ");
		if (strcmp(token, "") != 0)
			pos++;
		if (pos >= index)
		{	
			// Copy string when on right index
			strcpy(dest, token);
			break;
		}
		token = buff;
	}
	// Freeing buffers
	free(buffer);
	free(str);
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
float bandwidth = 0;
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
	struct tm now;
	time_t secondsSinceEpoch;
	int SLEEP_SCHEDULED;

	// Info variables
	while (1)
    {
		// Start update
		T1 = millis();
		
		// Stats array
		char date[64]      = "";
		char clock[16]     = "";
	    char freespace[16] = "";
		char volume[16]    = "";
		char meta[132]     = "";
		char song[SONG_MAX_WIDTH + 5]      = "";

		// Getting time		
		time(&secondsSinceEpoch);
		now = *localtime(&secondsSinceEpoch);
		
		strftime(date, 64, "%a %d %B", &now);
		strftime(clock, 16, "%H:%M", &now);

		// Getting disk space
		query(freespace, "df -h " MAIN_DRIVE, 256, 3);

		// Getting volume
		query(volume, "pactl list sinks | grep \"Volume :\"", 256, 4);

		// Getting song info
		exec(meta, 132, "mpc current -f \"%artist% - %title%\"");
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
		else if (strlen(meta) < 6)
			strcpy(song, "");
		else
			strcpy(song, meta);
		
		// Render
		printf(",[{\"full_text\":\"%s   %3.1f Ko/s   %s   %s  %s  %s \"}] \n",
		song, bandwidth, volume, freespace, date, clock);

		// Flush output stream
		fflush(stdout);

        // End update & sleep until next update
        DELTA_TIME = (unsigned long) millis() - T1;
        SLEEP_SCHEDULED = SLEEP_INTERVAL - DELTA_TIME;
        if (SLEEP_SCHEDULED > 0)
            usleep(SLEEP_SCHEDULED * 1000);
    }

	// Free mem
	pthread_mutex_destroy(&lock);
    return 0;
}
