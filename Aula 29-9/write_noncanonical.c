// Transmitter - Sends SET and waits for UA with retransmission
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
#include <signal.h>

#define _POSIX_SOURCE 1 // POSIX compliant source

#define BAUDRATE 38400
#define BUF_SIZE 256

#define FLAG 0x7E
#define A    0x03
#define C_SET 0x03
#define C_UA  0x07

#define FALSE 0
#define TRUE 1

int fd = -1;           // File descriptor for open serial port
struct termios oldtio; // Serial port settings to restore on closing
volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

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

    // Install alarm handler
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    // Build SET frame
    unsigned char set[5] = {FLAG, A, C_SET, A ^ C_SET, FLAG};

    int maxRetransmissions = 3;
    int success = FALSE;

    while (alarmCount < maxRetransmissions && !success)
    {
        if (!alarmEnabled)
        {
            int bytes = writeBytesSerialPort(set, 5);
            printf("SET sent (%d bytes)\n", bytes);

            alarm(3);          // start timer
            alarmEnabled = TRUE;
        }

        unsigned char byte;
        int res = readByteSerialPort(&byte);
        if (res > 0)
        {
            if (byte == C_UA)
            {
                printf("UA received! Connection established.\n");
                alarm(0); // cancel alarm
                success = TRUE;
            }
        }
    }

    if (!success)
    {
        printf("Failed: maximum retransmissions reached.\n");
    }

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
    if (fd < 0)
    {
        perror(serialPort);
        return -1;
    }

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
    }

    tcflag_t br;
    switch (baudRate)
    {
        case 38400: br = B38400; break;
        default: fprintf(stderr, "Unsupported baud rate\n"); return -1;
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
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    oflags ^= O_NONBLOCK;
    fcntl(fd, F_SETFL, oflags);
    return fd;
}

int closeSerialPort()
{
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }
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
