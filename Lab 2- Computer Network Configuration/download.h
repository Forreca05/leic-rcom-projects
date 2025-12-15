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

#define CMD_SIZE 128

#define CONNECTION_OPEN_CODE "125"
#define OPENING_CONNECTION_CODE "150"
#define SERVICE_READY_CODE "220"
#define QUIT_SUCCESS_CODE "221"
#define TRANSFER_COMPLETE_CODE "226"
#define PASV_MODE_CODE "227"
#define LOGIN_SUCCESS_CODE "230"
#define USERNAME_OK_CODE "331"

int parse(char *link, URL *url);
int createsocket(const char *adr, int port);

#endif
