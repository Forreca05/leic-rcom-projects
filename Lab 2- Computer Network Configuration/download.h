#ifndef DOWNLOAD_H
#define DOWNLOAD_H

typedef struct URL {
    char user[64];
    char password[64];
    char host[128];
    char path[64];
    char file[256];
} URL;

#define FTP_PORT 21

int parse(char *link, URL *url);
int createsocket(const char *adr, int port);

#endif
