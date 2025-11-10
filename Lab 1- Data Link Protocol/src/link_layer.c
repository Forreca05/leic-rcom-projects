#include "link_layer.h"
#include <stdlib.h>
#include <string.h>
#include "serial_port.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#define C_RR(Nr) (0xAA | Nr)
#define C_REJ(Nr) (0x54 | Nr)
#define C_N(Ns) (Ns << 7)

// Size of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer.
#define MAX_PAYLOAD_SIZE 1000

#define _POSIX_SOURCE 1
#define ESC 0x7D
#define FLAG 0x7E
#define A_RX 0x01
#define A_TX 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B

#define FALSE 0
#define TRUE 1

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

int alarmTriggered = FALSE;
int alarmCount = 0;
int timeout = 0;
int retransmitions = 0;
unsigned char tramaTx = 0;
unsigned char tramaRx = 0;

void alarmHandler(int signal)
{
    alarmTriggered = TRUE;
    alarmCount++;
}

int destuffBytes(unsigned char* destuffed, int frameLen, const unsigned char* frame) {
    int currentIDX = 0;
    int i = 0;

    while (i < frameLen) {
        if (frame[i] == ESC) {
            if (i + 1 >= frameLen) break;
            destuffed[currentIDX++] = frame[i + 1] ^ 0x20;
            i += 2;
        } else {
            destuffed[currentIDX++] = frame[i++];
        }
    }
    return currentIDX;
}

int sendSupervFrame(unsigned char A, unsigned char C) {
    unsigned char frame[5] = {FLAG, A, C, A ^ C, FLAG};
    int res = writeBytesSerialPort(frame, 5);
    return res;
}


unsigned char readcontrolframe() {
    unsigned char byte, cField = 0;
    State state = START;
    while(state != STOP && alarmTriggered == FALSE) {
        int res = readByteSerialPort(&byte);
        if (res > 0) {
            switch (state) {
                case START:
                    if (byte == FLAG) {state = FLAG_RCV;}
                    break;
                case FLAG_RCV:
                    if (byte == A_RX) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;
                case A_RCV:
                    if (byte == C_RR(0) || byte == C_RR(1) || byte == C_REJ(0) || byte == C_REJ(1)) {
                        state = C_RCV;
                        cField = byte;
                    } 
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    if (byte == (A_RX ^ cField)) state = BCC_OK;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case BCC_OK:
                    if (byte == FLAG) state = STOP;
                    else state = START;
                    break;
                default: 
                    break;
            }
        }
    }
    alarm(0);
    return cField;
}

int llopen(LinkLayer connectionParameters)
{
    State state = START;
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;

    unsigned char byte;
    timeout = connectionParameters.timeout;
    retransmitions = connectionParameters.nRetransmissions;
    int attempt = 1;
    
    switch(connectionParameters.role) {
        case(LlTx): {
            if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
                perror("signal");
                exit(1);
            }
            while (connectionParameters.nRetransmissions != 0 && state != STOP) {
                sendSupervFrame(A_TX, C_SET);
                alarm(connectionParameters.timeout);
                alarmTriggered = FALSE;

                while (alarmTriggered == FALSE && state != STOP) {
                    int res = readByteSerialPort(&byte);
                    if (res > 0) {
                        switch (state) {
                            case START:
                                if (byte == FLAG) {
                                    state = FLAG_RCV;
                                }
                                break;
                            case FLAG_RCV:
                                if (byte == A_RX) {
                                    state = A_RCV;
                                }
                                else if (byte != FLAG) state = START;
                                break;
                            case A_RCV:
                                if (byte == C_UA) {
                                    state = C_RCV;
                                }
                                else if (byte == FLAG) state = FLAG_RCV;
                                else state = START;
                                break;
                            case C_RCV:
                                if (byte == (A_RX ^ C_UA)) {
                                    state = BCC_OK;
                                }
                                else if (byte == FLAG) state = FLAG_RCV;
                                else state = START;
                                break;
                            case BCC_OK:
                                if (byte == FLAG) {
                                    state = STOP;
                                }
                                else state = START;
                                break;
                            default: 
                                break;
                        }
                    }
                } 

                if (alarmTriggered && state != STOP) {
                    printf("[LINK-LLOPEN] Timeout! Retransmission attempt number %d...\n", attempt);
                    attempt++;
                }

                connectionParameters.nRetransmissions--;
            }
            if (state != STOP) {
                printf("[LINK-TX] Failed to establish connection after %d attempts.\n", attempt - 1);
                return -1;
            }
            break;  
        }

        case(LlRx): {
            while (state != STOP)
            {
                int res = readByteSerialPort(&byte);
                if (res <= 0) continue;
                switch (state)
                {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_TX) state = A_RCV;
                        else if (byte != FLAG) state = START;
                        break;
                    case A_RCV:
                        if (byte == C_SET) state = C_RCV;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == (A_TX ^ C_SET)) {
                            state = BCC_OK;
                        }
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) state = STOP;
                        else state = START;
                        break;
                    case STOP:
                        break;
                }
            }
            sendSupervFrame(A_RX, C_UA);
            break;
        }
        default:
            return -1;
            break;
    }
    return 0;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int frameSize = 6 + bufSize;  // FLAG + A + C + BCC1 + dados + BCC2 + FLAG
    unsigned char *frame = (unsigned char *) malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = A_TX;
    frame[2] = C_N(tramaTx);
    frame[3] = frame[1] ^ frame[2];

    unsigned char BCC_2 = buf[0];

    for (int i = 1; i < bufSize; i++) BCC_2 ^= buf[i];
     
    int j = 4;
    for (int i = 0; i <  bufSize; i++) {
        if(buf[i] == FLAG || buf[i] == ESC) {
            frame = realloc(frame, ++frameSize);
            frame[j++] = ESC;
            frame[j++] = buf[i] ^ 0x20; // Write the XORed byte
        } else {
            frame[j++] = buf[i]; // Write the normal byte
        }
    }

    if(BCC_2 == FLAG || BCC_2 == ESC) {
        frame = realloc(frame, ++frameSize);
        frame[j++] = ESC;
        frame[j++] = BCC_2 ^ 0x20;
    } else {
        frame[j++] = BCC_2;
    }

    frame[j++] = FLAG;

    int currenttransmission = 0;
    int accepted = 0; int rejected = 0;

    while(currenttransmission < retransmitions) {
        alarmTriggered = FALSE;
        alarm(timeout);
        accepted = 0; rejected = 0;

        while(!alarmTriggered && !accepted && !rejected) {
            writeBytesSerialPort(frame, j);
            unsigned char result = readcontrolframe();

            if(!result) {
                continue;
            }
            else if (result == C_REJ(tramaTx)) {
                printf("[LINK-LLWRITE] REJ received, retransmitting...\n");
                rejected = 1;
            }
            else if ((result == C_RR((tramaTx + 1) % 2))) {
                accepted = 1;
                tramaTx = (tramaTx + 1) % 2;
            } 
            else {
                continue;
            }
        }
        if (accepted) break;
        if (alarmTriggered) {
            currenttransmission++;
            printf("[LINK-LLWRITE] Timeout! Retransmission number %d...\n", currenttransmission);
        }
    }

    free(frame);
    if (accepted) return frameSize;
    else {
        printf("[LINK-LLWRITE] Failed to transmit frame after %d attempts.\n", retransmitions);
        return -1;
    } 
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte;
    int frameIDX = 0;
    unsigned char C = 0;
    State state = START;

    unsigned char frame[MAX_PAYLOAD_SIZE*2+2]; // Worst-case scenario: all bytes need to be stuffed  + BCC
    unsigned char destuffed[MAX_PAYLOAD_SIZE]; 

    while (state != STOP) {
        if (readByteSerialPort(&byte)) {
            switch (state) {
                case START:
                    frameIDX = 0;
                    if (byte == FLAG) state = FLAG_RCV;
                    break;

                case FLAG_RCV:
                    if (byte == A_TX) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;

                case A_RCV:
                    if (byte == C_N(0) || byte == C_N(1)) {
                        C = byte;
                        state = C_RCV;
                    } else if (byte == C_DISC) {
                        sendSupervFrame(A_RX, C_DISC);
                        return 0; 
                    } else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;

                case C_RCV:
                    if (byte == (C ^ A_TX)) {
                        state = BCC_OK;
                    } else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;

                case BCC_OK:
                if (byte == FLAG) {
                    if (frameIDX > 0) {
                        int dataSize = destuffBytes(destuffed, frameIDX, frame);
                        
                        unsigned char receivedBCC2 = destuffed[dataSize - 1];
                        unsigned char calculatedBCC2 = destuffed[0];
                        for (int i = 1; i < dataSize - 1; i++)
                            calculatedBCC2 ^= destuffed[i];

                        if (receivedBCC2 == calculatedBCC2) {
                            if (C == C_N(tramaRx)) {
                                tramaRx = (tramaRx + 1) % 2;
                                sendSupervFrame(A_RX, C_RR(tramaRx)); // Acknowledge receipt of frame, waiting for next
                                memcpy(packet, destuffed, dataSize - 1);
                                return dataSize - 1;
                            } 
                            else { // Duplicated Frame
                                sendSupervFrame(A_RX, C_RR(tramaRx)); 
                                state = START;
                            }
                        } else {
                            printf("[LINK-LLREAD] Requesting retransmission (REJ).\n");
                            sendSupervFrame(A_RX, C_REJ(tramaRx));
                            state = START;
                        }
                    }
                } 
                 
                else {
                    if (frameIDX < MAX_PAYLOAD_SIZE*2)
                        frame[frameIDX++] = byte;
                    else {
                        printf("[LINK-LLREAD] Frame overflow\n");
                        frameIDX = 0;
                        state = START;
                    }
                }
                break;
            }
        }
    }
    printf("[LINK-LLREAD] Reception failed: no valid frame received\n");
    return -1;
}


////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose() {
    State state = START;
    unsigned char byte;
    if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
                perror("signal");
                exit(1);
            }
    while (retransmitions != 0 && state != STOP) {
        sendSupervFrame(A_TX, C_DISC);
        alarm(timeout);
        alarmTriggered = FALSE;

        while (alarmTriggered == FALSE && state != STOP) {
            int res = readByteSerialPort(&byte);
            if (res > 0) {
                switch (state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RX) state = A_RCV;
                        else if (byte != FLAG) state = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) state = C_RCV;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == (A_RX ^ C_DISC)) state = BCC_OK;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) state = STOP;
                        else state = START;
                        break;
                    default: 
                        break;
                }
            }
        } 
        retransmitions--;
    }
    if (state != STOP) return -1;
    sendSupervFrame(A_TX, C_UA);
    return closeSerialPort();
}

