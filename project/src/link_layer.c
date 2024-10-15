// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define CTRL_BUF_SIZE 5

// trama Bytes
#define FLAG 0X7E
#define ADDRESS_RX 0X01
#define ADDRESS_TX 0X03
#define UA 0X07
#define SET 0X03

////////////////////////////////////////////////
// Alarm Handler
////////////////////////////////////////////////

int alarmCount = 0;
int alarmEnabled = FALSE;
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
};

////////////////////////////////////////////////
// State Machine
////////////////////////////////////////////////

typedef enum
{
    START_STATE,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_RCV,
    STOP_STATE
} StateType;

typedef struct
{
    StateType currentState; // Current state of the state machine
} StateMachine;

void transition(StateMachine *sm, StateType newState)
{
    sm->currentState = newState;
}

int proccessCtrlByte(StateMachine *sm, StateType address, StateType control, unsigned char curr_byte, unsigned char buffer[], int *bufferPosition)
{
    switch (sm->currentState)
    {

    case START_STATE:
        *bufferPosition = 0; // Reset buffer position at the start
        if (curr_byte == FLAG)
        {
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            transition(sm, FLAG_RCV);
        }
        break;

    case FLAG_RCV:
        if (curr_byte == address)
        {
            buffer[(*bufferPosition)++] = curr_byte; // Store ADDRESS_RX
            transition(sm, A_RCV);
        }
        else if (curr_byte == FLAG)
        {
            *bufferPosition = 0; // Reset buffer position when a new FLAG is received
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            transition(sm, FLAG_RCV);
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;

    case A_RCV:
        if (curr_byte == control)
        {
            buffer[(*bufferPosition)++] = curr_byte; // Store CONTROL
            transition(sm, C_RCV);
        }
        else if (curr_byte == FLAG)
        {
            *bufferPosition = 0; // Reset buffer position when a new FLAG is received
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            transition(sm, FLAG_RCV);
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;

    case C_RCV:
        if (curr_byte == (buffer[1] ^ buffer[2]))
        {
            buffer[(*bufferPosition)++] = curr_byte; // Store BCC
            transition(sm, BCC_RCV);
        }
        else if (curr_byte == FLAG)
        {
            *bufferPosition = 0; // Reset buffer position when a new FLAG is received
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            transition(sm, FLAG_RCV);
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;

    case BCC_RCV:
        if (curr_byte == FLAG)
        {
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            transition(sm, STOP_STATE);
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;

    case STOP_STATE:
        return 0;
    }
    return -1;
}


void buildCtrlWord(unsigned char address, unsigned char control)
{

    unsigned char buf[CTRL_BUF_SIZE] = {0};

    buf[0] = FLAG;
    buf[1] = address;
    buf[2] = control;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = FLAG;

    int bytes = writeBytesSerialPort(buf, CTRL_BUF_SIZE); // cntrl_buffer
    if (bytes < 0)
    {
        printf("Error opening bytes\n");
        exit(-1);
    }
    printf("%d bytes written\n", bytes);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        printf("Error opening serial port\n");
        exit(-1);
    }
    if (connectionParameters.role == LlRx)
    {

        StateMachine sm;
        sm.currentState = START_STATE;

        unsigned char buf[CTRL_BUF_SIZE] = {0};
        unsigned char curr_byte;
        int bufferPosition = 0;

        do
        {
            int readBytes = readByteSerialPort(&curr_byte);
            if (readBytes < 0)
            {
                printf("error\n");
                exit(-1);
            }
            if (readBytes == 0)
            {
                continue;
            }
            printf("Read byte: 0x%02X\n", curr_byte);
        } while (proccessCtrlByte(&sm, ADDRESS_TX, SET, curr_byte, buf, &bufferPosition) != 0);

        printf("SET received\n");
        buildCtrlWord(ADDRESS_RX, UA);
        printf("sent UA\n");
    }
    else
    {

        // ENABLE ALARM
        alarmCount = 0;
        alarmEnabled = FALSE;
        (void)signal(SIGALRM, alarmHandler);

        // SET UP STATE MACHINE AND BUFFER
        StateMachine sm;
        sm.currentState = START_STATE;

        unsigned char readbuf[CTRL_BUF_SIZE] = {0};
        unsigned char curr_byte;
        int bufferPosition = 0;
        int result = -1;

        while (alarmCount < connectionParameters.nRetransmissions && result < 0)
        {
            if (alarmEnabled == FALSE)
            {

                buildCtrlWord(ADDRESS_TX, SET);
                printf("sent SET\n");

                alarm(connectionParameters.timeout);
                alarmEnabled = TRUE;
            }

            int readBytes = readByteSerialPort(&curr_byte);
            if (readBytes < 0)
            {
                printf("error\n");
                exit(-1);
            }
            if (readBytes == 0)
            {
                continue;
            }
            printf("Read byte: 0x%02X\n", curr_byte);

            result = proccessCtrlByte(&sm, ADDRESS_RX, UA, curr_byte, readbuf, &bufferPosition);
            if(sm.currentState == STOP_STATE){
                printf("UA received\n");
                break;
            }
        }
        if (alarmCount == connectionParameters.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
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
    // must decide if package is for llopen or llclose

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
