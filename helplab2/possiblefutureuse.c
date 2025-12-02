
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


    printf("%d %d %d %d %d %d %d\n", h1, h2, h3, h4, p1, p2, port);
    
    int readcommand(char *buf, int sock, const char *code) {
        size_t n = read(sock, buf, sizeof(buf) - 1);
        if (n <= 0) return -1;

        buf[n] = '\0';
        printf("%s", buf);

        if (strncmp(buf, code, 3) != 0) {
            printf("ERROR: expected %s but got %s\n", code, buf);
            return -1;
        }

        return 0;
    }