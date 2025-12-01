#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "download.h"

int parse(char *link, URL *url) {

    char *input = link;

    strcpy(url->name, "anonymous");
    strcpy(url->password, "anonymous@");

    // tem de começar por ftp://
    if (strncmp(input, "ftp://", 6) != 0) {
        printf("URL inválido\n");
        return -1;
    }

    input += 6; // saltar ftp://

    char *arroba = strchr(input, '@');
    char *slash  = strchr(input, '/');

    if (slash == NULL) {
        printf("URL inválido: falta /path\n");
        return -1;
    }

    // se existir user:pass@
    if (arroba != NULL && arroba < slash) {

        char *doispontos = strchr(input, ':');

        if (doispontos == NULL || doispontos > arroba) {
            printf("Formato user:password inválido\n");
            return -1;
        }

        // user
        int user_len = doispontos - input;
        strncpy(url->name, input, user_len);
        url->name[user_len] = '\0';

        // password
        int pass_len = arroba - doispontos - 1;
        strncpy(url->password, doispontos + 1, pass_len);
        url->password[pass_len] = '\0';

        // HOST começa depois da @
        input = arroba + 1;

        // recalcular slash
        slash = strchr(input, '/');
        if (slash == NULL) {
            printf("URL inválido\n");
            return -1;
        }
    }

    // HOST
    int host_len = slash - input;
    strncpy(url->host, input, host_len);
    url->host[host_len] = '\0';

    // PATH completo
    strcpy(url->path, slash + 1);

    // FILE
    char *last = strrchr(url->path, '/');
    if (last)
        strcpy(url->file, last + 1);
    else
        strcpy(url->file, url->path);

    return 0;
}

int createsocket(const char *adr, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(adr);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s ftp://...\n", argv[0]);
        exit(1);
    }

    URL url;
    memset(&url, 0, sizeof(url));

    if (parse(argv[1], &url) != 0) {
        printf("Erro no parse\n");
        exit(1);
    }

    printf("USER=%s\nPASS=%s\nHOST=%s\nPATH=%s\nFILE=%s\n",
           url.name, url.password, url.host, url.path, url.file);
    
    // DNS
    struct hostent *h = gethostbyname(url.host);

    if (h == NULL) {
        printf("Erro DNS\n");
        exit(1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    printf("IP = %s\n", ip);

    // abrir socket para porto FTP
    int sock = createsocket(ip, FTP_PORT);
    printf("Ligado ao servidor FTP!\n");

    return 0;
}
