// Receiver - Waits for SET and replies with UA
//
// Modified by: [teu nome]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define _POSIX_SOURCE 1

#define FALSE 0
#define TRUE 1

#define BAUDRATE 38400

#define FLAG 0x7E
#define A    0x03
#define C_SET 0x03
#define C_UA  0x07

int fd = -1;
struct termios oldtio;

// Receiver state machine states
typedef enum {
    START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
} State;

int openSerialPort(const char *serialPort, int baudRate);
int closeSerialPort();
int readByteSerialPort(unsigned char *byte);
int writeBytesSerialPort(const unsigned char *bytes, int nBytes);

// ---------------------------------------------------
// MAIN
// ---------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <SerialPort>\n", argv[0]);
        exit(1);
    }

    const char *serialPort = argv[1];
    if (openSerialPort(serialPort, BAUDRATE) < 0)
    {
        perror("openSerialPort");
        exit(-1);
    }
    printf("Serial port %s opened\n", serialPort);

    State state = START;
    unsigned char byte;

    while (state != STOP)
    {
        if (readByteSerialPort(&byte) > 0)
        {
            switch (state)
            {
                case START:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == A) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;
                case A_RCV:
                    if (byte == C_SET) state = C_RCV;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    if (byte == (A ^ C_SET)) state = BCC_OK;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case BCC_OK:
                    if (byte == FLAG) state = STOP;
                    else state = START;
                    break;
                default: state = START;
            }
        }
    }

    printf("SET received successfully!\n");

    // Send UA
    unsigned char ua[5] = {FLAG, 0x01, C_UA, 0x01 ^ C_UA, FLAG};
    int bytes = writeBytesSerialPort(ua, 5);
    printf("UA sent (%d bytes)\n", bytes);

    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        exit(-1);
    }
    printf("Serial port %s closed\n", serialPort);

    return 0;
}

// ---------------------------------------------------
// SERIAL PORT LIBRARY IMPLEMENTATION
// ---------------------------------------------------
int openSerialPort(const char *serialPort, int baudRate)
{
    int oflags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    fd = open(serialPort, oflags);
    if (fd < 0) return -1;

    if (tcgetattr(fd, &oldtio) == -1) return -1;

    tcflag_t br;
    switch (baudRate)
    {
        case 38400: br = B38400; break;
        default: return -1;
    }

    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = br | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 1;

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);

    oflags ^= O_NONBLOCK;
    fcntl(fd, F_SETFL, oflags);
    return fd;
}

int closeSerialPort()
{
    tcsetattr(fd, TCSANOW, &oldtio);
    return close(fd);
}

int readByteSerialPort(unsigned char *byte)
{
    return read(fd, byte, 1);
}

int writeBytesSerialPort(const unsigned char *bytes, int nBytes)
{
    return write(fd, bytes, nBytes);
}
