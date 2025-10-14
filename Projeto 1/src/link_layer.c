#include "link_layer.h"
#include "serial_port.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1
#define FLAG 0x7E
#define ESC 0x7F
#define A_TX 0x03
#define A_RX 0x01
#define C_SET 0x03
#define C_UA 0x07

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
unsigned char tramaRx = 1;

void alarmHandler(int signal)
{
    alarmTriggered = TRUE;
    alarmCount++;
}


int llopen(LinkLayer connectionParameters)
{
    State state = START;
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) return -1;

    unsigned char byte;
    timeout = connectionParameters.timeout;
    retransmitions = connectionParameters.nRetransmissions;
    
    switch(connectionParameters.role) {
         // --- TRANSMITTER ---
        case(LlTx): {
            if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
                perror("signal");
                exit(1);
            }
            while (connectionParameters.nRetransmissions != 0 && state != STOP) {
                unsigned char set[] = {FLAG, A_TX, C_SET, A_TX ^ C_SET, FLAG};
                alarm(connectionParameters.timeout);
                alarmTriggered = FALSE;

                while (alarmTriggered == FALSE && state != STOP) {
                    int res = readByteSerialPort(&byte);
                    if (res > 0) {
                        switch (state) {
                            case START:
                                if (byte == FLAG) state = FLAG_RCV;
                                break;
                            case FLAG_RCV:
                                if (byte == A_TX) state = A_RCV;
                                else if (byte != FLAG) state = START;
                                break;
                            case A_RCV:
                                if (byte == C_UA) state = C_RCV;
                                else if (byte == FLAG) state = FLAG_RCV;
                                else state = START;
                                break;
                            case C_RCV:
                                if (byte == (A_TX ^ C_UA)) state = BCC_OK;
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
                connectionParameters.nRetransmissions--;
            }
            if (state != STOP) return -1;
            break;  
        }

        // --- RECEIVER ---
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
                        if (byte == (A_TX ^ C_SET)) state = BCC_OK;
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
            unsigned char ua[] = {FLAG, A_RX, C_UA, A_RX ^ C_UA, FLAG};
            writeBytesSerialPort(ua, 5);
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

    memcpy(frame + 4, buf, bufSize);
    unsigned char BCC_2 = buf[0];

    for (int i = 1; i < bufSize; i++) BCC_2 ^= buf[i];
     
    int j = 4;
    for (int i = 0; i <  bufSize; i++) {
        if(buf[i] == FLAG || buf[i] == ESC) {
            frame = realloc(frame, ++frameSize);
            frame[j++] = ESC;
        }
        frame[j++] = buf[i];
    }

    frame[j++] = BCC_2;
    frame[j++] = FLAG;

    int currenttransmission = 0;
    int accepted = 0; int rejected = 0;

    while(currenttransmission < retransmitions) {
        alarmTriggered = FALSE;
        alarm(timeout);

        writeBytesSerialPort(frame, j);

        while(!alarmTriggered && )
    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte;
    int frameIDX, BCC2Field;
    int i = 0;
    unsigned char A, C, BCC1, BCC2;
    State state = START;
    unsigned char frame[MAX_PAYLOAD_SIZE];
    while (state != STOP) {  
        if (readByteSerialPort (&byte)) {
            switch (state) {
            
                case START:
                    if (byte = FLAG) state = FLAG_RCV;
                    break;
                    
                case FLAG_RCV:
                    if (byte = A_TX) {
                        state = A_RCV;
                        A = byte; 
                    }
                    else if (byte != FLAG) state = START;
                    break;
                    
                case A_RCV:
                    if (byte == C_N(0) || byte == C_N(1)) {
                        state = C_RCV;
                        C = byte; 
                    }
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                    
                case C_RCV:
                    if (byte = C^A) {
                        state = BCC_OK;
                        frameIDX = 0; 
                    }
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                    
                case BCC_OK:
                    if (byte == FLAG) {
                        if (frameIDX > 0) {
                            int dataSize = normalizeBytes(packet, frameIDX, frame); // falta implementar
                            BCC2Field = packet[dataSize - 1];
                            unsigned char BCC2 = 0;                                           
                            for (int i = 0; i < dataSize - 1; i++) BCC2 ^= packet[i];       // Calculate BCC2 manually to check for errors in the transmission
                            if (BCC2 == BCC2Field) {     // Valid Frame
                                sendResponse(RR); // falta implementar
                                state = STOP;
                                return dataSize - 1;
                            } 
                            else {       // Invalid Frame
                                sendResponse(REJ); // falta implementar
                                state = START;
                            }
                        }
                    }
                    else {
                        frame[frameIDX++] = byte;
                    }
                    break;
            }
                    
        }
        return -1;
    }
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    return 0;
}