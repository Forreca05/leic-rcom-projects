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

    strcpy(url->user, "anonymous");
    strcpy(url->password, "anonymous@");

    if (strncmp(input, "ftp://", 6) != 0) {
        printf("[Parsing] Invalid URL (not FTP!)\n");
        return -1;
    }

    input += 6;

    char *arroba = strchr(input, '@');
    char *slash  = strchr(input, '/');

    if (slash == NULL) {
        printf("[Parsing] Invalid URL (missing '/')\n");
        return -1;
    }

    // If URL matches type: ftp://user:password@host/path
    if (arroba != NULL && arroba < slash) {
        char *doispontos = strchr(input, ':');

        if (doispontos == NULL || doispontos > arroba) {
            printf("[Parsing] Invalid User/Password\n");
            return -1;
        }

        int user_len = doispontos - input;
        strncpy(url->user, input, user_len);
        url->user[user_len] = '\0';

        int pass_len = arroba - doispontos - 1;
        strncpy(url->password, doispontos + 1, pass_len);
        url->password[pass_len] = '\0';

        input = arroba + 1;
        slash = strchr(input, '/');
        if (slash == NULL) {
            printf("[Parsing] Invalid URL (missing /path!)\n");
            return -1;
        }
    }

    // Also supports anonymous login (ftp://host/path)
    int host_len = slash - input;
    strncpy(url->host, input, host_len);
    url->host[host_len] = '\0';

    strcpy(url->path, slash + 1);

    char *last = strrchr(url->path, '/');
    if (last) strcpy(url->file, last + 1);
    else      strcpy(url->file, url->path);

    return 0;
}

int createsocket(const char *adr, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(adr);
    server_addr.sin_port        = htons(port);

    // Creating the socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    // Estabilishing a connection with the server
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

    printf("USER = %s\nPASS = %s\nHOST = %s\nPATH = %s\nFILE = %s\n",
           url.user, url.password, url.host, url.path, url.file);

    struct hostent *h = gethostbyname(url.host);

    if (h == NULL) {
        printf("Erro DNS\n");
        exit(1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    printf("IP = %s\n\n", ip);

    int sock = createsocket(ip, FTP_PORT);
    printf("Connected to FTP server!\n");

    usleep(100000);
    size_t bytes_read = read(sock, buf, 500);
    buf[bytes_read] = '\0';
    printf("%s", buf);

    if (strncmp(buf, SERVICE_READY_CODE, 3) != 0) {
        printf("Error: Can't connect to the server.\n");
        exit(-1);
    }

    // User Command
    char user_command[CMD_SIZE];
    sprintf(user_command, "USER %s\r\n", url.user);
    write(sock, user_command, strlen(user_command));
    printf("%s", user_command);

    size_t bytes_user = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_user] = '\0';
    printf("%s", buf);

    if (strncmp(buf, USERNAME_OK_CODE, 3) != 0) {
        printf("Error: USER command rejected or unexpected server reply.\n");
        exit(-1);
    }

    // Password Command (and Login)
    char password_command[CMD_SIZE];
    sprintf(password_command, "PASS %s\r\n", url.password);
    write(sock, password_command, strlen(password_command));
    printf("%s", password_command);

    size_t bytes_password = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_password] = '\0';
    printf("%s", buf);

    if (strncmp(buf, LOGIN_SUCCESS_CODE, 3) != 0) {
        printf("Error: Login failed or unexpected server reply after PASS command.\n");
        exit(-1);
    }

    // PASV Command
    const char* pasv_command = "PASV\r\n";
    write(sock, pasv_command, strlen(pasv_command));
    printf("%s", pasv_command);

    size_t bytes_passive = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_passive] = '\0';
    printf("%s", buf);

    if (strncmp(buf, PASV_MODE_CODE, 3) != 0) {
        printf("Error: Failed to enter passive mode.\n");
        exit(-1);
    }

    int h1 = -1, h2 = -1, h3 = -1, h4 = -1, p1 = -1, p2 = -1;
    int count = 0, num = 0, inside = 0;

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
            if (count == 5) p1 = num;
            num = 0;
        }
    }

    char ipserver[32];
    sprintf(ipserver, "%d.%d.%d.%d", h1, h2, h3, h4);
    int port = 256 * p1 + p2;

    printf("IP SERVER = %s\n", ipserver);
    printf("PORT = %d\n", port);

    int sockserver = createsocket(ipserver, port);

    // RETR Command
    char retr_command[CMD_SIZE];
    sprintf(retr_command, "RETR %s\r\n", url.path);
    write(sock, retr_command, strlen(retr_command));

    size_t bytes_readserver = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_readserver] = '\0';
    printf("%s", buf);

    if (strncmp(buf, "125", 3) != 0 && strncmp(buf, "150", 3) != 0) {
        printf("Error: Server did not accept RETR command.\n");
        exit(-1);
    }

    FILE *file_fd = fopen(url.file, "wb");
    if (file_fd == NULL) {
        perror("Unable to create local file");
        exit(EXIT_FAILURE);
    }

    int n = 0;
    while ((n = read(sockserver, buf, sizeof(buf))) > 0) {
        size_t written = fwrite(buf, 1, n, file_fd);
        if (written != n) {
            perror("Error writing to file");
            fclose(file_fd);
            exit(EXIT_FAILURE);
        }
    }

    fclose(file_fd);

    if (close(sockserver) < 0) {
        perror("Error closing data socket");
        exit(-1);
    }

    size_t bytes_readquit = read(sock, buf, sizeof(buf) - 1);
    buf[bytes_readquit] = '\0';
    printf("%s", buf);

    if (strncmp(buf, TRANSFER_COMPLETE_CODE, 3) != 0) {
        printf("Error: File transfer not completed successfully.\n");
        exit(-1);
    }

    // Quit Command
    const char* quit_command = "QUIT\r\n";
    write(sock, quit_command, strlen(quit_command));
    printf("%s", quit_command);

    size_t goodbye = read(sock, buf, sizeof(buf) - 1);
    buf[goodbye] = '\0';
    printf("%s", buf);

    if (strncmp(buf, QUIT_SUCCESS_CODE, 3) != 0) {
        printf("Error: Server did not acknowledge QUIT command properly.\n");
        exit(-1);
    }

    if (close(sock) < 0) {
        perror("Error closing control socket");
        exit(-1);
    }

    return 0;
}
