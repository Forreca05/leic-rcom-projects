#ifndef DOWNLOAD_H
#define DOWNLOAD_H

typedef struct URL {
    char host[128];
    char name[64];
    char password[64];
    char path[512];
    char file[256];
} URL;

#define FTP_PORT 21

int parse(char *link, URL *url);
int createsocket(const char *adr, int port);

#endif
