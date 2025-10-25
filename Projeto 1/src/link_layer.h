#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include "serial_port.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#define C_RR(Nr) (0xAA | Nr)
#define C_REJ(Nr) (0x54 | Nr)
#define C_N(Ns) (Ns << 7)

// Size of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer.
#define MAX_PAYLOAD_SIZE 910

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
    DATA_FOUND_ESC,
    STOP
} State;

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return 0 on success or -1 on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or -1 on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or -1 on error.
int llread(unsigned char *packet);

// Close previously opened connection and print transmission statistics in the console.
// Return 0 on success or -1 on error.
int llclose();

#endif // _LINK_LAYER_H_
