// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

    /*
    printf("--------------LLWRITE-------------\n");
    unsigned char testData[] = {0x01, 0x02, 0x03, 0x04}; // Test data buffer
    int bufSize = sizeof(testData);

    // If the role is LlTx, send data
    if (connectionParameters.role == LlTx) {
        int bytesSent = llwrite(testData, bufSize);
        if (bytesSent > 0) {
            printf("Sent %d bytes successfully.\n", bytesSent);
        } else {
            printf("Failed to send data.\n");
        }
    }
    */

    unsigned char buffer[16] = {
        0x7E,                  // Start byte
        0x03,                  // Control byte
        0x00,
        0x00 ^ 0x03,           // BCC1 (0x7E ^ 0x03 = 0x7D)
        
        // Random payload (10 bytes, hardcoded)
        0xA4, 0x29, 0xB3, 0x3F, 0x18, 0x5D, 0xAA, 0x43, 0x61, 0xDD,
        
        // BCC2 (XOR of all payload bytes)
        0x11,
        
        
        0x7E                   // End byte
    };

    if(connectionParameters.role == LlTx){
        writeBytesSerialPort(buffer, 16);
    }
    
    printf("--------------LLREAD--------------\n");
    // If the role is LlRx, receive data
    if (connectionParameters.role == LlRx) {
        unsigned char receiveBuffer[1024] = {0};
        int bytesRead = llread(receiveBuffer);
        if (bytesRead > 0) {
            printf("Received %d bytes:\n", bytesRead);
            for (int i = 0; i < bytesRead; i++) {
                printf("0x%02X ", receiveBuffer[i]);
            }
            printf("\n");
        } else {
            printf("Failed to receive data.\n");
        }
    }



    if (llclose(1) < 0)
    {
        printf("Error: Failed to close link layer connection.\n");
        return;
    }
    

}

