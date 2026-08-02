#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_DATA_LEN 255

typedef struct {
    char command[32];
    char data[256];
    int length;
} Packet;

int main(int argc, char *argv[]) {
    char server_ip[32] = "127.0.0.1";
    char command[32] = "LIST";
    char data[256] = "";

    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Aufruf: %s [ip] <WRITE|READ|LIST> [text]\n", argv[0]);
        return 0;
    }

    int idx = 1;
    if (strchr(argv[1], '.') != NULL || strcmp(argv[1], "localhost") == 0) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
        idx = 2;
    }

    if (argc > idx) {
        strncpy(command, argv[idx], sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';

        if (argc > idx + 1) {
            if (strlen(argv[idx + 1]) > MAX_DATA_LEN) {
                fprintf(stderr, "Daten zu lang (max %d Zeichen)\n", MAX_DATA_LEN);
                return 1;
            }
            strncpy(data, argv[idx + 1], sizeof(data) - 1);
            data[sizeof(data) - 1] = '\0';
        }
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    Packet req = {0};
    strncpy(req.command, command, sizeof(req.command) - 1);
    strncpy(req.data, data, sizeof(req.data) - 1);
    req.length = strlen(req.data);

    if (send(sock, &req, sizeof(req), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    Packet res = {0};
    ssize_t n = recv(sock, &res, sizeof(res), 0);
    if (n < 0) { perror("recv"); close(sock); return 1; }
    if (n == 0) { close(sock); return 0; }

    res.data[sizeof(res.data) - 1] = '\0';
    printf("%s\n", res.data);

    close(sock);
    return 0;
}
