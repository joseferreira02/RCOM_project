// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define CTRL_BUF_SIZE 5

// trama Bytes
#define FLAG 0X7E
#define ADDRESS_RX 0X01
#define ADDRESS_TX 0X03
#define UA 0X07
#define SET 0X03
#define DISC 0x0B
#define ESC 0x7D
#define RR_RECEIVED 1
#define REJ_RECEIVED -1

static int sequenceNumber = 0;

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

void resetAlarm()
{
    alarmCount = 0;
    alarmEnabled = FALSE;
}

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

int processCtrlByte(StateMachine *sm, StateType address, StateType control, unsigned char curr_byte, unsigned char buffer[], int *bufferPosition)
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
            *bufferPosition = 0;                     // Reset buffer position when a new FLAG is received
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
            *bufferPosition = 0;                     // Reset buffer position when a new FLAG is received
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
            *bufferPosition = 0;                     // Reset buffer position when a new FLAG is received
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
            return 0;
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;

    case STOP_STATE:{
        // Access control byte
        unsigned char control = buffer[2];

        if (control == 0xAA || control == 0xAB) return RR_RECEIVED;
        else if (control == 0x54 || control == 0x55) return REJ_RECEIVED; 
        }
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


unsigned char* byteStuffing(const unsigned char *data, int dataSize, int *stuffedSize){

    // Allocate memory for the worst case possible
    unsigned char *stuffedData = (unsigned char *)malloc(2*dataSize-2);
    if (stuffedData == NULL){
        printf("Memory allocation failed\n");
        *stuffedSize = -1;
        exit(-1);
    }

    int j = 0; // Index for stuffed data

    stuffedData[j++] = data[0]; // Initial flag should not be stuffed

    // Apply byte stuffing from the second byte to the second-to-last byte
    for (int i = 0; i < dataSize - 1; i++){
        if (data[i] == FLAG){
            stuffedData[j++] = ESC;
            stuffedData[j++] = 0x5E;
        }
        else if (data[i] == ESC){
            stuffedData[j++] = ESC;
            stuffedData[j++] = 0x5D;
        }
        else stuffedData[j++] = data[i];
    }

    stuffedData[j++] = data[dataSize-1];
    *stuffedSize = j; // Update the size of the stuffed data
    return stuffedData;
}

unsigned char* createIFrame(const unsigned char *buf, int bufSize, int* stuffedSize){
    // Dynamically allocate memory for the frame
    unsigned char *frame = (unsigned char *)malloc(CTRL_BUF_SIZE + bufSize + 2);
    if (frame == NULL) {
        printf("Memory allocation failed\n");
        return NULL; // Return error if memory allocation fails
    }

    // Build the frame without stuffing (FLAG | ADDRESS | CONTROL | BCC1 | DATA | BCC2 | FLAG)
    frame[0] = FLAG;
    frame[1] = ADDRESS_TX;
    frame[2] = sequenceNumber ? 0x80 : 0x00; // Sequence number (Ns = 0 or 1)
    frame[3] = frame[1]^frame[2];

    // Copy data to the frame
    for (int i = 0; i < bufSize; i++){
        frame[4 + i] = buf[i];
    }

    // Calculate BCC2
    unsigned char BCC2 = 0;
    for (int i = 0; i < bufSize; i++){
        BCC2 ^= buf[i];
    }

    frame[4 + bufSize] = BCC2;
    frame[5 + bufSize] = FLAG;

    // Apply byte stuffing
    unsigned char *stuffedFrame = byteStuffing(frame, CTRL_BUF_SIZE + bufSize + 2, stuffedSize);
    if (stuffedFrame == NULL){
        printf("ERROR: Couldn't perform byte stuffing\n");
        free(frame);
        exit(-1);
    }

    free(frame);
    return stuffedFrame;
}

LinkLayer cp;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // save connectionParameters
    cp = connectionParameters;

    int fd = openSerialPort(connectionParameters.serialPort,
                            connectionParameters.baudRate);
    if (fd < 0)
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
        } while (processCtrlByte(&sm, ADDRESS_TX, SET, curr_byte, buf, &bufferPosition) != 0);

        printf("SET received\n");
        buildCtrlWord(ADDRESS_RX, UA);
        printf("sent UA\n");
    }
    else
    {

        resetAlarm();
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

            result = processCtrlByte(&sm, ADDRESS_RX, UA, curr_byte, readbuf, &bufferPosition);
        }
        if (alarmCount == connectionParameters.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }

        printf("UA received\n");
    }

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{

    int stuffedSize = 0;
    unsigned char* stuffedFrame = createIFrame(buf, bufSize, &stuffedSize);

    (void)signal(SIGALRM, alarmHandler);

    // SET UP STATE MACHINE AND BUFFER
    StateMachine sm;
    sm.currentState = START_STATE;

    // Wait for acknowledgment frame (RR or REJ)
    unsigned char ackFrame[CTRL_BUF_SIZE];
    unsigned char curr_byte;
    int bufferPosition = 0;
    int result = -1;
    int bytesSent = 0;

    // Retransmission logic
    while (alarmCount < cp.nRetransmissions && result < 0){
        
        if(alarmEnabled == FALSE){
            // Send the frame
            bytesSent = writeBytesSerialPort(stuffedFrame, stuffedSize);
            printf("Sent I Frame\n");

            // Start timer
            alarm(cp.timeout);
            alarmEnabled = TRUE;
        }

        int readBytes = readByteSerialPort(&curr_byte);
        if (readBytes < 0)
        {
            printf("error\n");
            exit(-1);
        }
        if (readBytes == 0) continue;

        printf("Read byte: 0x%02X\n", curr_byte);

        // State Machine processing
        // Process the acceptance or rejection frame sent back by the receiver
        result = processCtrlByte(&sm, ADDRESS_RX, UA, curr_byte, ackFrame, &bufferPosition);

        if (result == RR_RECEIVED){
            printf("Acknoledgement received\n");
            alarm(0);
            sequenceNumber = (sequenceNumber + 1) % 2; // Update Sequence Number
            break;
        }
        else if (result == REJ_RECEIVED){
            printf("Frame rejected. Retransmiting...\n");
            resetAlarm();
        }
    }

    if (alarmCount == cp.nRetransmissions)
    {
        printf("Maximum retransmissions reached. Exiting...\n");
        return -1;
    }
    free(stuffedFrame);
    return bytesSent;
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
    // show statistics

    if (cp.role == LlRx)
    {

        StateMachine sm;
        sm.currentState = START_STATE;

        unsigned char buf[CTRL_BUF_SIZE] = {0};
        unsigned char curr_byte;
        int bufferPosition = 0;

        // reads DISC BYTE
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
        } while (processCtrlByte(&sm, ADDRESS_TX, DISC, curr_byte, buf, &bufferPosition) != 0);
        printf("DISC received\n");

        // SENDS DISC BYTE
        buildCtrlWord(ADDRESS_RX, DISC);
        printf("sent DISC\n");

        // reset buffer/sm
        bufferPosition = 0;
        memset(buf, 0, sizeof(buf));
        sm.currentState = START_STATE;
        int result = -1;
        // alarm setup
        resetAlarm();
        (void)signal(SIGALRM, alarmHandler);

        // READS UA BYTE
        while (alarmCount < cp.nRetransmissions && result < 0)
        {
            if (alarmEnabled == FALSE)
            {

                buildCtrlWord(ADDRESS_TX, DISC);
                printf("sent DISC\n");

                alarm(cp.timeout);
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

            result = processCtrlByte(&sm, ADDRESS_TX, UA, curr_byte, buf, &bufferPosition);
        }
        if (alarmCount == cp.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }
        printf("UA RECEIVED\n");
    }
    else
    {


        //alarm setup
        resetAlarm();
        (void)signal(SIGALRM, alarmHandler);

        // SET UP STATE MACHINE AND BUFFER
        StateMachine sm;
        sm.currentState = START_STATE;

        unsigned char readbuf[CTRL_BUF_SIZE] = {0};
        unsigned char curr_byte;
        int bufferPosition = 0;
        int result = -1;

        //reads DISC BYTE
        while (alarmCount < cp.nRetransmissions && result < 0)
        {
            if (alarmEnabled == FALSE)
            {

                buildCtrlWord(ADDRESS_TX, DISC);
                printf("sent DISC\n");

                alarm(cp.timeout);
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
            result = processCtrlByte(&sm, ADDRESS_RX, DISC, curr_byte, readbuf, &bufferPosition);
        }
        printf("DISC received\n");

        //sends UA BYTE
        buildCtrlWord(ADDRESS_TX, UA);
        printf("sent UA\n");
        if (alarmCount == cp.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }
    }

    if (showStatistics)
    {
        printf("---- Statistics ----\n");
        printf("EMPTY");
        printf("--------------------\n");
    }

    int clstat = closeSerialPort();
    return clstat;
}
