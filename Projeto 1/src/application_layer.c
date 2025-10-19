#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>

unsigned char *createControlPacket (long int fileSize, char* fileName, int controlfield, int* cPacketSize) {
    int nameLength = strlen(fileName);
    int fileSizeLength = (int) ceil(log2f((float)fileSize)/8.0);
    *cPacketSize = 1+1+fileSizeLength+1+nameLength;
    unsigned char packet[*cPacketSize];
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
    memcpy(fileName, controlPacket+3+fileSizeLength+2, fileNameLength);

    return fileName;
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

    printf("\n[APP] Starting link layer (%s)...\n", role);

    // 1️⃣ Estabelece ligação (SET/UA)
    int fd = llopen(connectionParameters);
    if (fd < 0) {
        printf("[APP] llopen() failed!\n");
        return;
    }

    if (connectionParameters.role == LlTx) {
        // --- TRANSMITTER ---
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("[APP] Error opening file");
            return;
        }

        fseek(file, 0, SEEK_END);
        long int filesize = ftell(file);
        rewind(file);

        unsigned int cPacketSize;
        unsigned char *startPacket = createControlPacket(filesize, filename, CTRL_START, &cPacketSize);

        if(llwrite(startPacket, cPacketSize) == -1){ 
            printf("[APP] Error in start packet\n");
            exit(-1);
        }   

        unsigned char* data = getData(file, filesize); //Get data from file
        int remainingBytes = filesize;
        int dataFieldOctets = 0;
        int dataL1 = 0, dataL2;
        int dPacketSize;

        while (remainingBytes != 0) { //Write data until no more bytes left
            int payloadSize = remainingBytes > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : remainingBytes;
            unsigned char* dataChunk = (unsigned char*) malloc(payloadSize);
            memcpy(dataChunk, data, payloadSize);
            unsigned char *dataPacket = createDataPacket(dataChunk, payloadSize, &dPacketSize);
            
            if(llwrite(dataPacket, dPacketSize) == -1) {
                printf("[APP] Error writing data packets\n");
                exit(-1);
            }
            
            remainingBytes -= MAX_PAYLOAD_SIZE; 
            dataChunk += payloadSize; 
        }

        unsigned char *endPacket = createControlPacket(filesize, filename, CTRL_END, &cPacketSize);

        if(llwrite(endPacket, cPacketSize) == -1){ 
            printf("[APP] Error in end packet\n");
            exit(-1);
        }   
    }

    if (connectionParameters.role == LlRx) {
        unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
        int packetSize = -1;
        while (packetSize = llread(packet)>0); //Tries to read until a packet is sucessfully read

        unsigned long int fileSize = 0;
        unsigned char* fileName = unpackControlPacket(packet, packetSize, &fileSize); 

    }



    

    printf("[APP] Link established successfully!\n");

    // 2️⃣ Fecha ligação (DISC/UA)
    printf("[APP] Closing link...\n");
    llclose();

    printf("[APP] Connection closed.\n");
}
