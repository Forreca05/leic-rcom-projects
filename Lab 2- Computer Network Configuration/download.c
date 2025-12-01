#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

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
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    return sockfd;
}

int main(int argc, char *argv[]) {
    char buf[500];
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

    // ler mensagem de boas-vindas (220)
    usleep(100000); //meti isto porque nao tava a dar de nenhuma maneira
    size_t bytes_read = read(sock, buf, 500);
    buf[bytes_read] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "220", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }


    //Enviar USER/PASS
    char user_command[100];
    sprintf(user_command, "USER %s\r\n", url.name);
    write(sock, user_command, strlen(user_command));
    printf("%s", user_command);

    size_t bytes_read2 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read2] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "331", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    char password_command[100];
    sprintf(password_command, "PASS %s\r\n", url.password);
    write(sock, password_command, strlen(password_command));
    printf("%s", password_command);

    size_t bytes_read3 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read3] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "230", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    char changing_command[100] = "CWD debian\r\n";
    write(sock, changing_command, strlen(changing_command));
    printf("%s",changing_command);

    size_t bytes_read4 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read4] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "250", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    char binary_command[100] = "TYPE I\r\n";
    write(sock, binary_command, strlen(binary_command));
    printf("%s",binary_command);

    size_t bytes_read5 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read5] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "200", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    char size_command[100] = "SIZE README\r\n";
    write(sock, size_command, strlen(size_command));
    printf("%s",size_command);

    size_t bytes_read6 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read6] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "213", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    char passive_command[100] = "PASV\r\n";
    write(sock, passive_command, strlen(passive_command));
    printf("%s",passive_command);

    size_t bytes_read7 = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_read7] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "227", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }

    int h1 = -1, h2 = -1, h3 = -1, h4 = -1, p1 = -1, p2 = -1;
    int count = 0;

    int num = 0;
    int inside = 0;

    for (int i = 0; buf[i] != '\0'; i++) {
        char c = buf[i];

        if (c == '(') { inside = 1; continue; }
        if (c == ')') { p2 = num; break; }

        if (!inside) continue;

        if (isdigit(c)) {
            num = num * 10 + (c - '0');
        }
        else if (c == ',') {
            count++;

            if (count == 1) h1 = num;
            if (count == 2) h2 = num;
            if (count == 3) h3 = num;
            if (count == 4) h4 = num;
            if (count == 5) p1 = num;  // 4º número
            if (count == 6) p2 = num;  // 5º número

            num = 0; // reset para o próximo
        }
    }

    int port = 256 * p1 + p2;
    printf("%d %d %d %d %d %d %d", h1, h2, h3, h4, p1, p2, port);
    
    char ipserver[32];
    sprintf(ipserver, "%d.%d.%d.%d", h1, h2, h3, h4);

    int sockserver = createsocket(ipserver, FTP_PORT);

    char retr_command[100];
    sprintf(retr_command, "RETR %s\r\n", url.);
    write(sock, retr_command, strlen(retr_command));
    printf("%s", retr_command);

    size_t bytes_readserver = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_readserver] = '\0';
    printf("%s\n", buf);
    if (strncmp(buf, "150", 3) != 0) {
        printf("Error: Unexpected reply from connection.\n");
        exit(-1);
    }


    return 0;
}
