// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define CTRL_BUF_SIZE 5

// trama Bytes
#define FLAG 0X7E

#define ESCFLAG 0X5E
#define ESCESC 0X5D


#define ANSWER 0X00

#define ADDRESS_RX 0X01
#define ADDRESS_TX 0X03
#define UA 0X07
#define SET 0X03
#define DISC 0x0B
#define ESC 0x7D
#define C0 0x00
#define C1 0x80

#define RR0 0xAA
#define RR1 0xAB
#define REJ0 0x054
#define REJ1 0x55

#define RR_RECEIVED 1
#define REJ_RECEIVED 0

static int sequenceNumber = 0;
unsigned char sequenceChar = C1;
int isValid = FALSE;
int isRepeated = FALSE;
static int escCheck = FALSE;

// C
// 00000000 / 0x00 Information frame number 0
// 10000000 / 0x80 Information frame number 1
//

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

// returns 1 if BCC2 is correct
int checkBCC2(unsigned char message[], int charsRead, unsigned char bcc2_byte)
{
    unsigned char BCC2;
    for (int i = 0; i < charsRead; i++)
    {
        BCC2 ^= message[i];
    }
    return BCC2 == bcc2_byte;
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

    // IFRAME
    INFO_STATE,

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

int proccessInfoByte(StateMachine *sm, StateType address, StateType control, unsigned char curr_byte, unsigned char buffer[], int *bufferPosition, unsigned char message[], int *charsRead)
{
    // Array of possible RR and REJ control bytes
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
            else if (curr_byte)
            {
                transition(sm, START_STATE);
            }
            break;

    case C_RCV:
        if (curr_byte == C0 || curr_byte == C1)
        {
            if (curr_byte == sequenceChar)
            {
                isRepeated = TRUE;
            }
            else
            {
                isRepeated = FALSE;
            }
            buffer[(*bufferPosition)++] = curr_byte;
            sequenceChar = curr_byte;
            transition(sm, INFO_STATE);
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
            transition(sm, INFO_STATE);
        }
        else
        {
            transition(sm, START_STATE);
        }
        break;
    case INFO_STATE:
        switch (curr_byte)
        {
        case FLAG:
            if(!escCheck)transition(sm, STOP_STATE);
            break;
        case ESCFLAG:
            if (escCheck)
            {
                buffer[(*bufferPosition)++] = FLAG;
                message[(*charsRead)++] = FLAG;
                escCheck = FALSE;
            }
            else
            {
                escCheck = TRUE;
            }
            break;
        case ESCESC:
            if (escCheck)
            {
                buffer[(*bufferPosition)++] = ESC;
                message[(*charsRead)++] = ESC;
                escCheck = FALSE;
            }
            else
            {
                escCheck = TRUE;
            }

            break;
        case ESC:
            escCheck = TRUE;
            break;
        default:
            buffer[(*bufferPosition)++] = curr_byte; // Store FLAG
            message[(*charsRead)++] = curr_byte;
            break;
        }
        break;
    case STOP_STATE:
        // checks condition
        isValid = checkBCC2(message, *charsRead - 2, buffer[(*bufferPosition) - 1]);
        return 0;
        break;
    }
    return -1;
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

        if (curr_byte == control || (((curr_byte == RR0) || (curr_byte == RR1) || (curr_byte == REJ0) || (curr_byte == REJ1))  && control == ANSWER) ) // curr_byte == (RR0 || RR1 || REJ0 || REJ1)
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
            unsigned char access_control = buffer[2];

            if (access_control == 0xAA || access_control == 0xAB) return RR_RECEIVED;
            else if (access_control == 0x54 || access_control == 0x55) return REJ_RECEIVED; 
            }

    default:
        break;
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

}


unsigned char* byteStuffing(const unsigned char *frame, int frameSize, int *stuffedSize){

    // Allocate memory for the worst case possible
    unsigned char *stuffedData = (unsigned char *)malloc(2*frameSize-2);
    if (stuffedData == NULL){
        printf("Memory allocation failed\n");
        *stuffedSize = -1;
        exit(-1);
    }

    int j = 0; // Index for stuf
    
    // First 4 bytes shouldn't be stuffed (FLAG, ADDRESS, CONTROL, BCC)
    stuffedData[j++] = frame[0];
    stuffedData[j++] = frame[1];
    stuffedData[j++] = frame[2];
    stuffedData[j++] = frame[3];

    // Apply byte stuffing to the data section
    for (int i = 4; i < frameSize-2; i++){
        if (frame[i] == FLAG){
            stuffedData[j++] = ESC;
            stuffedData[j++] = 0x5E;
        }
        else if (frame[i] == ESC){
            stuffedData[j++] = ESC;
            stuffedData[j++] = 0x5D;
        }
        else stuffedData[j++] = frame[i];
    }

    // Last 2 bytes shouldn't be stuffed (BCC2, FLAG)
    stuffedData[j++] = frame[frameSize-2];
    stuffedData[j++] = frame[frameSize-1];
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
            // printf("Read byte: 0x%02X\n", curr_byte);
        } while (proccessCtrlByte(&sm, ADDRESS_TX, SET, curr_byte, buf, &bufferPosition) != 0);

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

            // printf("Read byte: 0x%02X\n", curr_byte);

            result = proccessCtrlByte(&sm, ADDRESS_RX, UA, curr_byte, readbuf, &bufferPosition);
        }
        if (alarmCount == connectionParameters.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }
        if (!result)
        {
            printf("UA received\n");
            alarm(0);
        }
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
        result = proccessCtrlByte(&sm, ADDRESS_RX, UA, curr_byte, ackFrame, &bufferPosition);

        if (result == RR_RECEIVED){
            printf("Acknowledgement received\n");
            alarm(0);
            sequenceNumber = (sequenceNumber + 1) % 2; // Update Sequence Number
            break;
        }
        else if (result == REJ_RECEIVED){
            printf("Frame rejected. Retransmiting...\n");
            alarmEnabled = FALSE;
            alarmCount++;
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
    StateMachine sm;
    sm.currentState = START_STATE;
    unsigned char curr_byte;
    unsigned char buffer[5];
    int bufferPosition = 0;

    // message
    unsigned char message[5];
    int charsRead = 0;
    isValid = FALSE;
    isRepeated = FALSE;

    // just proccess the byte  + destuffing
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

    } while (proccessInfoByte(&sm, ADDRESS_TX, sequenceNumber, curr_byte, buffer, &bufferPosition, message, &charsRead) != 0);

    // Send RR|REJ
    if (isValid && isRepeated)
    {
        sequenceNumber == 0 ? buildCtrlWord(ADDRESS_RX, RR0) : buildCtrlWord(ADDRESS_RX, RR1);
        printf("REPEATED RR SENT\n");
    }
    if (isValid && !isRepeated)
    {
        sequenceNumber == 1 ? buildCtrlWord(ADDRESS_RX, RR0) : buildCtrlWord(ADDRESS_RX, RR1);
        printf("CORRECT RR SENT\n");
    }
    if (!isValid)
    {
        sequenceNumber == 0 ? buildCtrlWord(ADDRESS_RX, REJ0) : buildCtrlWord(ADDRESS_RX, REJ1);
        printf("REJ SENT\n");
    }

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
            // printf("Read byte: 0x%02X\n", curr_byte);
        } while (proccessCtrlByte(&sm, ADDRESS_TX, DISC, curr_byte, buf, &bufferPosition) != 0);
        printf("DISC received\n");

        

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

                buildCtrlWord(ADDRESS_RX, DISC);
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
            // printf("Read byte: 0x%02X\n", curr_byte);

            result = proccessCtrlByte(&sm, ADDRESS_TX, UA, curr_byte, buf, &bufferPosition);
        }

        if (!result)
        {
            printf("UA received\n");
            alarm(0);
        }

        if (alarmCount == cp.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }
    }
    else
    {

        // alarm setup
        resetAlarm();
        (void)signal(SIGALRM, alarmHandler);

        // SET UP STATE MACHINE AND BUFFER
        StateMachine sm;
        sm.currentState = START_STATE;

        unsigned char readbuf[CTRL_BUF_SIZE] = {0};
        unsigned char curr_byte;
        int bufferPosition = 0;
        int result = -1;

        // reads DISC BYTE
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
            result = proccessCtrlByte(&sm, ADDRESS_RX, DISC, curr_byte, readbuf, &bufferPosition);
        }

        if(result ==0)
        {
            printf("DISC received\n");
            alarm(0);
        }

        if (alarmCount == cp.nRetransmissions)
        {
            printf("Maximum retransmissions reached. Exiting...\n");
            return -1;
        }

        // sends UA BYTE
        buildCtrlWord(ADDRESS_TX, UA);
        printf("sent UA\n");
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
