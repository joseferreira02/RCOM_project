/*// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PACKET_SIZE 500 // Tamanho máximo do packet (por decidir)

// Used to signal START and END of file transfer
void createControlPacket(unsigned char *packet, int *packetSize, int controlType, int fileSize, const char *fileName){
    packet[0] = controlType;
    packet[1] = 0x00;
    packet[2] = sizeof(int);
    memcpy(&packet[3], &fileSize, sizeof(int));

    if (fileName != NULL){
        int fileNameLength = strlen(fileName);
        packet[7] = 0x01;
        packet[8] = fileNameLength;
        memcpy(&packet[9], fileName, fileNameLength);
        *packetSize = 9 + fileNameLength;
    }
    else *packetSize = 7;
}

void createDataPacket(unsigned char *packet, int *packetSize, int sequenceNumber, unsigned char *data, int dataSize){
    packet[0] = 2;
    packet[1] = sequenceNumber;
    packet[2] = (dataSize >> 8) & 0xFF;
    packet[3] = dataSize & 0xFF;
    memcpy(&packet[4], data, dataSize);
    *packetSize = 4 + dataSize;
}

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
        printf("ERROR: Failed to open link layer connection.\n");
        return;
    }


    printf("--------------LLWRITE--------------\n");
    FILE *file = NULL;

    // If the role is LlTx, send data
    if (connectionParameters.role == LlTx) {
        file = fopen(filename, "rb");
        if (!file){
            printf("ERROR: Failed to open file.\n");
            llclose(1);
            return;
        }

        // Get the size of the file
        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Send START packet
        unsigned char startPacket[1000];
        int startPacketSize;
        createControlPacket(startPacket, &startPacketSize, 1, fileSize, filename);
        llwrite(startPacket, startPacketSize);

        // Send data packets
        unsigned char dataBuffer[MAX_PACKET_SIZE];
        int sequenceNumber = 0;
        int bytesRead;
        while ((bytesRead = fread(dataBuffer, 1, MAX_PACKET_SIZE, file)) > 0){
            unsigned char dataPacket[1000];
            int dataPacketSize;
            createDataPacket(dataPacket, &dataPacketSize, sequenceNumber, dataBuffer, bytesRead);
            llwrite(dataPacket, dataPacketSize);
            sequenceNumber = (sequenceNumber + 1) % 100;
        }

        // Send END packet
        unsigned char endPacket[1000];
        int endPacketSize;
        createControlPacket(endPacket, &endPacketSize, 3, fileSize, filename);
        llwrite(endPacket, endPacketSize);

        fclose(file);
    }


    else if (connectionParameters.role == LlRx){
        file = fopen(filename, "wb");
            if (!file){
                printf("ERROR: Failed to open file.\n");
                llclose(1);
                return;
            }

        unsigned char receiveBuffer[1024];
        int bytesRead;
        int receiving = 1;

        while (receiving && (bytesRead = llread(receiveBuffer)) > 0){
            if (receiveBuffer[0] == 1){
                printf("Received START packet\n");
                // Opcionalmente processar o packet
            }
            else if (receiveBuffer[0] == 2){
                int dataSize = (receiveBuffer[2] << 8) | receiveBuffer[3];
                fwrite(&receiveBuffer[4], 1, dataSize, file);
            }
            else if (receiveBuffer[0] == 3){
                printf("Received END packet\n");
                // Opcionalmente processar o packet
            }
        }

        fclose(file);
    }

    printf("--------------LLCLOSE--------------\n");
    if (llclose(1) < 0)
    {
        printf("ERROR: Failed to close link layer connection.\n");
        return;
    }

}*/




// Minimal Application Layer to test llopen, llwrite, and llclose

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MOCK_DATA_SIZE 100 // Size of mock data array

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer connectionParameters;

    // Set up link layer connection parameters
    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    connectionParameters.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;

    printf("Opening connection with llopen...\n");
    // Initialize the connection
    if (llopen(connectionParameters) < 0) {
        printf("ERROR: Failed to open link layer connection.\n");
        return;
    }

    // Transmitter (Tx) will send mock data
    if (connectionParameters.role == LlTx) {
        // Create a mock data array to send
        unsigned char mockData[MOCK_DATA_SIZE];
        for (int i = 0; i < MOCK_DATA_SIZE; i++) {
            mockData[i] = (unsigned char)(i % 256); // Fill array with sample data
        }

        printf("Sending mock data with llwrite...\n");
        if (llwrite(mockData, MOCK_DATA_SIZE) < 0) {
            printf("ERROR: Failed to send data with llwrite.\n");
        } else {
            printf("Mock data sent successfully.\n");
        }
    }
    // Receiver (Rx) would read the data here, if necessary
    else if (connectionParameters.role == LlRx) {
        printf("Waiting to receive data with llread...\n");
        unsigned char receiveBuffer[MOCK_DATA_SIZE];
        int bytesRead = llread(receiveBuffer);
        
        if (bytesRead < 0) {
            printf("ERROR: Failed to read data with llread.\n");
        } else {
            printf("Received %d bytes:\n", bytesRead);
            for (int i = 0; i < bytesRead; i++) {
                printf("%02X ", receiveBuffer[i]);
            }
            printf("\n");
        }
    }

    printf("Closing connection with llclose...\n");
    // Close the connection
    if (llclose(1) < 0) {
        printf("ERROR: Failed to close link layer connection.\n");
    } else {
        printf("Connection closed successfully.\n");
    }
}
