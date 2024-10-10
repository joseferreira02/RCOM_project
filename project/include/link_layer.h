// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

#define CNTRL_BUF_SIZE 5

//trama Bytes
#define FLAG 0X7E
#define ADDRESS_RX 0X01
#define ADDRESS_TX 0X03
#define UA 0X07
#define SET 0X03


//alarm
int alarmCount = 0;
int alarmEnabled = FALSE;
void alarmHandler(int signal);



//state Machine --
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


// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_
