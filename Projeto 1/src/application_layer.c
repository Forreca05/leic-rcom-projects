#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned char *createControlPacket (long int fileSize, const char *fileName, int controlfield, int* cPacketSize) {
    int nameLength = strlen(fileName);

    int fileSizeLength = 0;
    long int tmpFileSize = fileSize;
    while (tmpFileSize > 0) {
        fileSizeLength++;
        tmpFileSize >>= 8;
    } 

    *cPacketSize = 1+1+1+fileSizeLength+1+1+nameLength;
    unsigned char* packet = (unsigned char*) malloc(*cPacketSize);
    int index = 0;

    packet[index++] = controlfield;
    
    packet[index++] = TYPE_FILESIZE; //T1
    packet[index++] = fileSizeLength; //L1
    for (int i = fileSizeLength - 1; i >= 0; i--) {  //V1
        packet[index++] = (fileSize >> (i * 8)) & 0xFF;
    }

    packet[index++] = TYPE_FILENAME; //T2
    packet[index++] = nameLength; //L2
    memcpy(packet+index, fileName, nameLength); //V2

    return packet;
}

unsigned char *getData(FILE *fd, unsigned int fileSize) {
    unsigned char* data = (unsigned char*)malloc(sizeof(unsigned char) * fileSize);
    fread(data, sizeof(unsigned char), fileSize, fd);
    return data;
}

unsigned char* createDataPacket(unsigned char* data, int payloadSize, int* cPacketSize) {
    *cPacketSize = 3 + payloadSize;
    unsigned char* dataPacket = (unsigned char*)malloc(*cPacketSize);

    int index = 0;
    dataPacket[index++] = CTRL_DATA;                 //C
    dataPacket[index++] = (payloadSize >> 8) & 0xFF; //L2 
    dataPacket[index++] = payloadSize & 0xFF;        //L1 

    memcpy(&dataPacket[index], data, payloadSize);

    return dataPacket;
}

unsigned char* unpackControlPacket(unsigned char* controlPacket, int size, int* fileSize) {
    unsigned char fileSizeLength = controlPacket[2]; 
    unsigned char tempFileSize[fileSizeLength];
    memcpy(tempFileSize, controlPacket + 3, fileSizeLength);
    for (unsigned int i = 0; i<fileSizeLength; i++) {
        *fileSize |= ((unsigned long int) tempFileSize[i]) << (8 * (fileSizeLength - 1 - i));
    }

    unsigned char fileNameLength = controlPacket[3+fileSizeLength+1]; 
    unsigned char *fileName = (unsigned char*)malloc(fileNameLength);
    fileName[fileNameLength] = '\0';
    memcpy(fileName, controlPacket+3+fileSizeLength+2, fileNameLength);

    return fileName;
} 

int unpackDataPacket(unsigned char* dataPacket, int packetSize, unsigned char* data) {
    int L2 = dataPacket[1];
    int L1 = dataPacket[2];
    int payloadSize = L2*256 + L1;
    memcpy(data, dataPacket + 3, payloadSize);

    for (int i = 0; i < payloadSize; i++) {
        data[i] = dataPacket[3 + i];
    }
    return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.timeout = timeout;
    connectionParameters.nRetransmissions = nTries;

    int fd = llopen(connectionParameters);
    if (fd < 0) {
        printf("[APP] llopen() failed!\n");
        return;
    }

    printf("[APP] llopen() successful!\n");
    if (connectionParameters.role == LlTx) {
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("[APP-TX] Error opening file");
            return;
        }

        long int filesize = 0;
        while (fgetc(file) != EOF) {
            filesize++;
        }
        rewind(file);

        int cPacketSize;
        unsigned char *startPacket = createControlPacket(filesize, filename, CTRL_START, &cPacketSize);

        if(llwrite(startPacket, cPacketSize) == -1){ 
            printf("[APP-TX] Error in start packet\n");
            exit(-1);
        }   

        unsigned char* data = getData(file, filesize); //Get data from file
        int remainingBytes = filesize;
        int dPacketSize;

        printf("[APP-TX] Extracting %ld bytes from %s.\n", filesize, filename);
        while (remainingBytes != 0) { //Write data until no more bytes left
            int payloadSize = remainingBytes > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : remainingBytes;
            unsigned char* dataChunk = (unsigned char*) malloc(payloadSize);
            memcpy(dataChunk, data, payloadSize);
            unsigned char *dataPacket = createDataPacket(dataChunk, payloadSize, &dPacketSize);
            
            if(llwrite(dataPacket, dPacketSize) == -1) {
                printf("[APP-TX] Error writing data packets\n");
                exit(-1);
            }
            remainingBytes -= payloadSize; 
            data += payloadSize; 
        }

        unsigned char *endPacket = createControlPacket(filesize, filename, CTRL_END, &cPacketSize);

        if(llwrite(endPacket, cPacketSize) == -1){ 
            printf("[APP-TX] Error in end packet\n");
            exit(-1);
        }   
    }

    if (connectionParameters.role == LlRx) {
        unsigned char *startPacket = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
        int packetSize = llread(startPacket); 

        int fileSize = 0;
        unsigned char* fileName = unpackControlPacket(startPacket, packetSize, &fileSize); 
        printf("[APP-RX] Start Packet Received. Extracting %d bytes from %s.\n", fileSize, fileName);

        FILE *newFile = fopen("penguin-received.gif", "w");
        unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
        while (1) {
            while ((packetSize = llread(packet)) < 0);

            unsigned char C = packet[0];

            if (C == CTRL_END) {
                printf("[APP-RX] End Packet Received\n");
                break;
            }

            if (C == CTRL_DATA) {
                unsigned char *data = (unsigned char *)malloc(packetSize - 3); // Assumindo cabeçalho de 4 bytes
                unpackDataPacket(packet, packetSize, data);
                fwrite(data, sizeof(unsigned char), packetSize - 3, newFile);
                free(data);
            }
        }
        fclose(newFile);
    }

    printf("[APP] Link established successfully!\n");
    printf("[APP] Closing link...\n");
    llclose();
    printf("[APP] Connection closed.\n");
}
