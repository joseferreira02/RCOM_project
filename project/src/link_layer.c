// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>  // For SIGALRM and signal handling
#include <unistd.h>  // For alarm() function
#include <stdio.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


////////////////////////////////////////////////
// Alarm Handler
////////////////////////////////////////////////

void alarmHandler(int signal){
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n",alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    if(connectionParameters.role == LlRx){
        //receiver


    }else{

        //ENABLE ALARM
        alarmCount = 0;
        (void)signal(SIGALRM,alarmHandler);

        while (alarmCount < connectionParameters.nRetransmissions)
        {
            if (alarmEnabled == FALSE)
            {    
                unsigned char buf[CNTRL_BUF_SIZE] = {0};
                buf[0] = FLAG;
                buf[1] = ADDRESS_TX; // frames sent by sender
                buf[2] = SET; // control
                buf[3] = buf[1] ^ buf[2];
                buf[4] = FLAG;


                //int bytes = write(fd, buf, BUF_SIZE);
                int bytes = writeBytesSerialPort(buf,CNTRL_BUF_SIZE); //cntrl_buffer
                if(bytes<0){
                    printf("Error opening bytes\n");
                    exit(-1);
                }
                printf("%d bytes written\n", bytes);
                alarm(connectionParameters.timeout); // Set alarm to be triggered in timeout
                alarmEnabled = TRUE;
                //READ
                
                
            } 
        }
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO
    //must decide if package is for llopen or llclose

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
