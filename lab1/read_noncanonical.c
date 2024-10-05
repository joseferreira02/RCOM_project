// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5
#define FLAG 0X7E
#define ADDRESS_RX 0X01
#define ADDRESS_TX 0X03

#define UA 0X07
#define SET 0X03

typedef enum {
    START_STATE,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_RCV,
    STOP_STATE
} StateType;


typedef struct {
    StateType currentState; // Current state of the state machine
} StateMachine;

void transition(StateMachine* sm, StateType newState) {
    sm->currentState = newState; // Update the current state
    printf("Transitioned to state: %d\n", sm->currentState);
}



volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");


    // Loop for input

    //creating a state machine
    StateMachine sm;
    sm.currentState = START_STATE;

    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    unsigned char curr_byte;
    int bufferPosition = 0;



    // Returns after 5 chars have been input
    while(sm.currentState!=STOP_STATE){
        
        //Reads current state and atributes it to curr_byte
        int bytes = read(fd, &curr_byte, 1);
        if(bytes<0){
            printf("error\n");
            exit(-1);
        }

        switch (sm.currentState) {

            case START_STATE:

                if (curr_byte == FLAG) {
                    buf[bufferPosition++] = curr_byte; // Store FLAG
                    transition(&sm, FLAG_RCV);
                }
                break;

            case FLAG_RCV:

                if (curr_byte == ADDRESS_TX) {
                    buf[bufferPosition++] = curr_byte; // Store ADDRESS_TX
                    transition(&sm, A_RCV);
                } else if (curr_byte == FLAG) {
                    buf[bufferPosition++] = curr_byte; // Store FLAG
                    transition(&sm, FLAG_RCV);
                } else {
                    transition(&sm, START_STATE);
                }
                break;

            case A_RCV:
                if (curr_byte == SET) {
                    buf[bufferPosition++] = curr_byte; // Store ADDRESS_TX
                    transition(&sm, C_RCV);
                } else if (curr_byte == FLAG) {

                    buf[bufferPosition++] = curr_byte; // Store FLAG
                    transition(&sm, FLAG_RCV);

                } else {
                    transition(&sm, START_STATE);
                }
                break;

            case C_RCV:
                if (curr_byte == (buf[1]^buf[2]) ) {
                    buf[bufferPosition++] = curr_byte; // Store ADDRESS_TX
                    transition(&sm, BCC_RCV);
                } else if (curr_byte == FLAG) {

                    buf[bufferPosition++] = curr_byte; // Store FLAG
                    transition(&sm, FLAG_RCV);

                } else {
                    transition(&sm, START_STATE);
                }
                break;

            case BCC_RCV:
                if (curr_byte == FLAG ) {
                    buf[bufferPosition++] = curr_byte; // Store ADDRESS_TX
                    transition(&sm, STOP_STATE);
                }
                else {
                    transition(&sm, START_STATE);
                }
                break;
        }
    }

    for (size_t i = 0; i < BUF_SIZE; i++)
    {
        printf("var = 0x%02X\n", buf[i]);
    }
    


    if((TRUE)){
        printf("SET RECEIVED");
        
        unsigned char buf[BUF_SIZE] = {0};
        buf[0] = FLAG;
        buf[1] = ADDRESS_RX; // frames sent by sender
        buf[2] = UA; // control
        buf[3] = buf[1] ^ buf[2];
        buf[4] = FLAG;
        int bytes = write(fd, buf, BUF_SIZE);
        printf("%d bytes written\n", bytes);
    }
        

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
