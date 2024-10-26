// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
   
    LinkLayer connectionParameters;
    
    strcpy(connectionParameters.serialPort,serialPort);    
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;


    printf("--------------LLOPEN--------------\n");
    // Call llopen to initialize the link layer connection
    if (llopen(connectionParameters) < 0)
    {
        printf("Error: Failed to open link layer connection.\n");
        return;
    }
    printf("----------------------------------\n");


    printf("--------------LLCLOSE--------------\n");

    if (llclose(1) < 0)
    {
        printf("Error: Failed to close link layer connection.\n");
        return;
    }
    printf("-----------------------------------\n");

}
