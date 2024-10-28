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

    if (connectionParameters.role == LlTx){
        // Transmitter mode
        FILE * file = fopen(filename, "rb");
        if (file == NULL){
            printf("ERROR: Failed to open file %s\n", filename);
            return;
        }

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);

        unsigned char *buffer = (unsigned char *)malloc(fileSize);
        if (buffer == NULL) {
            printf("ERROR: Memory allocation failed\n");
            fclose(file);
            return;
        }
        int bytesRead;

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0){
            // Send file chunk
            if (llwrite(buffer, bytesRead) < 0){
                printf("Error: Failed to send data\n");
                fclose(file);
                free(buffer);
                return;
            }
        }

        fclose(file);
        printf("File transmission complete\n");
    }

    if (connectionParameters.role == LlRx){
        // Receiver mode
        FILE * file = fopen(filename, "wb");
        if (file == NULL){
            printf("ERROR: Failed to open file %s\n", filename);
            return;
        }


        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);

        unsigned char *buffer = (unsigned char *)malloc(fileSize);
        if (buffer == NULL) {
            printf("ERROR: Memory allocation failed\n");
            fclose(file);
            return;
        }
        int bytesReceived;

        while ((bytesReceived = llread(buffer)) > 0){
            // Write received data to file
            fwrite(buffer, 1, bytesReceived, file);
        }

        fclose(file);
        printf("File reception complete\n");
    }

    if (llclose(1) < 0)
    {
        printf("Error: Failed to close link layer connection.\n");
        return;
    }

}
