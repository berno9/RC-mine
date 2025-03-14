alarm.c
// Alarm example
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;
#define BUF_SIZE 256

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int alarm2(int fd, unsigned char *buf, unsigned char *buf2, int time)
{
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < time + 1)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(time); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
       
        int bytes2 = read(fd, buf2, BUF_SIZE);
        //printf("bytes2: %d\n", bytes2);
        if(bytes2 > 0){
            alarmEnabled = FALSE;
            printf("%d bytes received\n", bytes2);
            break;
        }
        else if (alarmEnabled == FALSE && alarmCount < time + 1) {
            int bytes3 = write(fd, buf, BUF_SIZE);
            printf("%d Sending again\n", bytes3);
        }
    }

    printf("Ending program\n");

    return 0;
}