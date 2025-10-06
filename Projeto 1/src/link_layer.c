#include "link_layer.h"
#include "serial_port.h"
#include <unistd.h>
#include <stdio.h>

// MISC
#define _POSIX_SOURCE 1
#define FLAG 0x7E
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

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // Open serial port
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        return -1;

    unsigned char byte;
    int res;

    // --- TRANSMITTER ---
    if (connectionParameters.role == LlTx)
    {
        unsigned char set[] = {FLAG, A_TX, C_SET, A_TX ^ C_SET, FLAG};
        State state = START;

        printf("Tx: Sending SET...\n");
        writeBytesSerialPort(set, 5);

        // Espera UA
        while (state != STOP)
        {
            res = readByteSerialPort(&byte);
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
                case STOP:
                    break;
            }
        }
        printf("Tx: UA received. Connection established!\n");
    }

    // --- RECEIVER ---
    else if (connectionParameters.role == LlRx)
    {
        State state = START;

        printf("Rx: Waiting for SET...\n");
        while (state != STOP)
        {
            res = readByteSerialPort(&byte);
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

        printf("Rx: SET received. Sending UA...\n");
        unsigned char ua[] = {FLAG, A_RX, C_UA, A_RX ^ C_UA, FLAG};
        writeBytesSerialPort(ua, 5);
    }

    else
    {
        printf("Error: invalid role.\n");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO: Implement this function

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