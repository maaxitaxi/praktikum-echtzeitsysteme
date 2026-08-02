#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BACKLOG 3
#define MAX_ITEMS 100
#define MAX_LEN 256

typedef struct {
    char cmd[32];
    char data[256];
    int len;
} Packet;

typedef struct {
    char items[MAX_ITEMS][MAX_LEN];
    int count;
    pthread_mutex_t mutex;
} ItemList;

ItemList item_list = { .count = 0 };

static void to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

void process_packet(Packet *req, Packet *res) {
    memset(res, 0, sizeof(*res));
    pthread_mutex_lock(&item_list.mutex);

    char cmd[32];
    strncpy(cmd, req->cmd, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    to_upper(cmd);

    if (strcmp(cmd, "READ") == 0) {
        int idx = atoi(req->data);
        if (idx >= 0 && idx < item_list.count) {
            strncpy(res->data, item_list.items[idx], sizeof(res->data) - 1);
            strcpy(res->cmd, "OK");
        } else {
            snprintf(res->data, sizeof(res->data), "Index %d existiert nicht", idx);
            strcpy(res->cmd, "ERROR");
        }
    } else if (strcmp(cmd, "WRITE") == 0) {
        if (item_list.count < MAX_ITEMS && strlen(req->data) > 0) {
            strncpy(item_list.items[item_list.count], req->data, MAX_LEN - 1);
            item_list.items[item_list.count][MAX_LEN - 1] = '\0';
            item_list.count++;
            snprintf(res->data, sizeof(res->data), "ok, jetzt %d Eintraege", item_list.count);
            strcpy(res->cmd, "OK");
        } else {
            strcpy(res->data, "Liste voll oder keine Daten");
            strcpy(res->cmd, "ERROR");
        }
    } else if (strcmp(cmd, "LIST") == 0) {
        if (item_list.count == 0) {
            strcpy(res->data, "Liste ist leer");
        } else {
            char buf[1024] = "";
            for (int i = 0; i < item_list.count; i++) {
                char line[256];
                snprintf(line, sizeof(line), "[%d] %s\n", i, item_list.items[i]);
                strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
            }
            strncpy(res->data, buf, sizeof(res->data) - 1);
        }
        strcpy(res->cmd, "OK");
    } else {
        snprintf(res->data, sizeof(res->data), "unbekanntes Kommando: %s", req->cmd);
        strcpy(res->cmd, "ERROR");
    }

    res->data[sizeof(res->data) - 1] = '\0';
    res->cmd[sizeof(res->cmd) - 1] = '\0';
    pthread_mutex_unlock(&item_list.mutex);
}

void handle_client(int fd) {
    Packet req, res;
    memset(&req, 0, sizeof(req));

    ssize_t n = recv(fd, &req, sizeof(req), 0);
    if (n < 0) { perror("recv"); return; }
    if (n == 0) { puts("Verbindung vom Client geschlossen"); return; }

    req.cmd[sizeof(req.cmd) - 1] = '\0';
    req.data[sizeof(req.data) - 1] = '\0';

    process_packet(&req, &res);
    send(fd, &res, sizeof(res), 0);
}

int main(void) {
    pthread_mutex_init(&item_list.mutex, NULL);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv);
        exit(EXIT_FAILURE);
    }

    if (listen(srv, BACKLOG) < 0) {
        perror("listen");
        close(srv);
        exit(EXIT_FAILURE);
    }

    printf("Server hoert auf Port %d\n", PORT);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int client = accept(srv, (struct sockaddr *)&caddr, &len);
        if (client < 0) { perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
        printf("Verbindung von %s:%d\n", ip, ntohs(caddr.sin_port));

        handle_client(client);
        close(client);
    }

    close(srv);
    return 0;
}
