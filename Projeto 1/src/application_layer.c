#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>

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

    printf("[APP] Link established successfully!\n");

    // 2️⃣ Fecha ligação (DISC/UA)
    printf("[APP] Closing link...\n");
    llclose();

    printf("[APP] Connection closed.\n");
}
