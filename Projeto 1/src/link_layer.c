#include "link_layer.h"
#include <stdlib.h>
#include <string.h>

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

int normalizeBytes (unsigned char* packet, int frameIDX, unsigned char* frame) {
    int previousByte = 0;
    int packetIDX = 0;
    for (int i = 0; i<frameIDX; i++) {
        if (frame[i] == ESC) {
            previousByte = frame[i];
            continue;
        }
        else if (previousByte == ESC) {
            packet[packetIDX++] = frame[i] ^ 0x20;
            previousByte = 0;
        }
        else {
            packet[packetIDX++] = frame[i];
            previousByte = 0;
        }
    }
    return packetIDX;
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
                connectionParameters.nRetransmissions--;
            }
            if (state != STOP) return -1;
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
        currenttransmission++;
    }

    free(frame);
    if (accepted) return frameSize;
    else {
        llclose();
        return -1;
    } 
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte;
    int i = 0;
    unsigned char C = 0;
    State state = START;

    while (state != STOP) {
        if (readByteSerialPort(&byte)) {
            switch (state) {
                case START:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;

                case FLAG_RCV:
                    if (byte == A_TX) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;

                case A_RCV:
                    // Identifica tipo de pacote
                    if (byte == C_N(0) || byte == C_N(1)) {
                        C = byte;
                        state = C_RCV;
                    } else if (byte == C_DISC) {
                        sendSupervFrame(A_RX, C_DISC);
                        return 0; // pacote de controle DISC
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
                    if (byte == ESC) state = DATA_FOUND_ESC;
                    else if (byte == FLAG){
                        unsigned char bcc2 = packet[i-1];
                        i--;
                        packet[i] = '\0';
                        unsigned char acc = packet[0];

                        for (unsigned int j = 1; j < i; j++)
                            acc ^= packet[j];

                        if (bcc2 == acc){
                            state = STOP;
                            sendSupervFrame(A_RX, C_RR(tramaRx));
                            tramaRx = (tramaRx + 1)%2;
                            return i; 
                        }
                        else{
                            printf("Error: retransmition\n");
                            sendSupervFrame(A_RX, C_REJ(tramaRx));
                            return -1;
                        };

                    }
                    else{
                        packet[i++] = byte;
                    }
                    break;
                case DATA_FOUND_ESC:
                    state = BCC_OK;
                    byte ^= 0x20;
                    if (byte == ESC || byte == FLAG) {
                        packet[i++] = byte;}
                    else{
                        packet[i++] = ESC;
                        packet[i++] = byte ^= 0x20;
                    }
                    break;
                default: 
                    break;
            }
        }
    }
    printf("[LLREAD] Falha na receção da trama\n");
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

