#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_ITEMS 100
#define MAX_LEN 256
#define MAX_PEERS 16

typedef struct { char cmd[32]; char data[256]; int len; } PDU;

char items[MAX_ITEMS][MAX_LEN];
int item_cnt = 0;
pthread_mutex_t list_mtx = PTHREAD_MUTEX_INITIALIZER;

int peer_fds[MAX_PEERS];
int peer_cnt = 0;
pthread_mutex_t peer_mtx = PTHREAD_MUTEX_INITIALIZER;

void add_item(const char *s) {
    pthread_mutex_lock(&list_mtx);
    if (item_cnt < MAX_ITEMS) {
        strncpy(items[item_cnt], s, MAX_LEN - 1);
        items[item_cnt][MAX_LEN - 1] = '\0';
        item_cnt++;
    }
    pthread_mutex_unlock(&list_mtx);
}

void del_item(int idx) {
    pthread_mutex_lock(&list_mtx);
    if (idx >= 0 && idx < item_cnt) {
        for (int i = idx; i < item_cnt - 1; i++)
            strcpy(items[i], items[i + 1]);
        item_cnt--;
    }
    pthread_mutex_unlock(&list_mtx);
}

char *get_item(int idx) {
    static char buf[MAX_LEN];
    buf[0] = '\0';
    pthread_mutex_lock(&list_mtx);
    if (idx >= 0 && idx < item_cnt)
        strcpy(buf, items[idx]);
    pthread_mutex_unlock(&list_mtx);
    return buf;
}

void format_list(char *out, size_t out_len) {
    out[0] = '\0';
    pthread_mutex_lock(&list_mtx);
    if (item_cnt == 0) {
        snprintf(out, out_len, "Liste ist leer");
    } else {
        for (int i = 0; i < item_cnt; i++) {
            char line[MAX_LEN + 20];
            snprintf(line, sizeof(line), "[%d] %s\n", i, items[i]);
            strncat(out, line, out_len - strlen(out) - 1);
        }
    }
    pthread_mutex_unlock(&list_mtx);
}

void process_pdu(PDU *req, PDU *res) {
    memset(res, 0, sizeof(PDU));
    char cmd[32];
    strncpy(cmd, req->cmd, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    for (int i = 0; cmd[i]; i++) cmd[i] = toupper(cmd[i]);

    if (strcmp(cmd, "READ") == 0) {
        int idx = atoi(req->data);
        char *val = get_item(idx);
        if (val[0]) {
            strcpy(res->data, val);
            strcpy(res->cmd, "OK");
        } else {
            snprintf(res->data, sizeof(res->data), "Index %d nicht gefunden", idx);
            strcpy(res->cmd, "ERR");
        }
    } else if (strcmp(cmd, "WRITE") == 0) {
        if (strlen(req->data) > 0) {
            add_item(req->data);
            snprintf(res->data, sizeof(res->data), "hinzugefuegt, jetzt %d", item_cnt);
            strcpy(res->cmd, "OK");
        } else {
            strcpy(res->data, "keine Daten");
            strcpy(res->cmd, "ERR");
        }
    } else if (strcmp(cmd, "DELETE") == 0) {
        int idx = atoi(req->data);
        del_item(idx);
        snprintf(res->data, sizeof(res->data), "gel. %d", idx);
        strcpy(res->cmd, "OK");
    } else if (strcmp(cmd, "LIST") == 0) {
        char tmp[1024];
        format_list(tmp, sizeof(tmp));
        strcpy(res->data, tmp);
        strcpy(res->cmd, "OK");
    } else {
        snprintf(res->data, sizeof(res->data), "unbekannt: %s", req->cmd);
        strcpy(res->cmd, "ERR");
    }
    res->data[sizeof(res->data) - 1] = '\0';
    res->cmd[sizeof(res->cmd) - 1] = '\0';
}

int send_pdu(int fd, PDU *p) {
    return send(fd, p, sizeof(PDU), 0) == sizeof(PDU) ? 0 : -1;
}

int recv_pdu(int fd, PDU *p) {
    memset(p, 0, sizeof(PDU));
    int n = recv(fd, p, sizeof(PDU), 0);
    if (n > 0) {
        p->cmd[31] = '\0';
        p->data[255] = '\0';
    }
    return n;
}

void broadcast_pdu(PDU *p, int except_fd) {
    pthread_mutex_lock(&peer_mtx);
    for (int i = 0; i < peer_cnt; i++)
        if (peer_fds[i] > 0 && peer_fds[i] != except_fd)
            send_pdu(peer_fds[i], p);
    pthread_mutex_unlock(&peer_mtx);
}

void *client_handler(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    PDU req, res;

    while (1) {
        int n = recv_pdu(fd, &req);
        if (n <= 0) break;

        process_pdu(&req, &res);

        if (strcasecmp(req.cmd, "WRITE") == 0 || strcasecmp(req.cmd, "DELETE") == 0)
            broadcast_pdu(&req, fd);

        send_pdu(fd, &res);
    }

    close(fd);
    pthread_mutex_lock(&peer_mtx);
    for (int i = 0; i < peer_cnt; i++)
        if (peer_fds[i] == fd) { peer_fds[i] = 0; break; }
    pthread_mutex_unlock(&peer_mtx);
    return NULL;
}

void *accept_loop(void *arg) {
    int srv_fd = *(int *)arg;
    free(arg);
    while (1) {
        int client = accept(srv_fd, NULL, NULL);
        if (client < 0) continue;
        int *f = malloc(sizeof(int));
        *f = client;
        pthread_t t;
        pthread_create(&t, NULL, client_handler, f);
        pthread_detach(t);
    }
    return NULL;
}

int connect_to_peer(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) <= 0) { close(s); return -1; }

    if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) { close(s); return -1; }

    pthread_mutex_lock(&peer_mtx);
    if (peer_cnt < MAX_PEERS) {
        peer_fds[peer_cnt++] = s;
    } else {
        close(s);
        s = -1;
    }
    pthread_mutex_unlock(&peer_mtx);
    return s;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Aufruf: %s <port> [ip:port ...]\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(port);

    if (bind(srv, (struct sockaddr *)&a, sizeof(a)) < 0) { perror("bind"); close(srv); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); close(srv); return 1; }
    printf("Peer hoert auf Port %d\n", port);

    int *srv_ptr = malloc(sizeof(int));
    *srv_ptr = srv;
    pthread_t st;
    pthread_create(&st, NULL, accept_loop, srv_ptr);
    pthread_detach(st);

    for (int i = 2; i < argc; i++) {
        char *s = argv[i];
        char *colon = strchr(s, ':');
        if (!colon) continue;
        *colon = '\0';
        int p = atoi(colon + 1);
        int fd = connect_to_peer(s, p);
        if (fd >= 0) printf("verbunden mit %s:%d\n", s, p);
        else fprintf(stderr, "Verbindung zu %s:%d fehlgeschlagen\n", s, p);
        *colon = ':';
    }

    char line[512];
    printf("> ");
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strlen(line) == 0) { printf("> "); fflush(stdout); continue; }

        char cmd[32], data[256];
        int n = sscanf(line, "%31s %255[^\n]", cmd, data);
        if (n < 1) { printf("> "); fflush(stdout); continue; }

        PDU req, res;
        memset(&req, 0, sizeof(req));
        strcpy(req.cmd, cmd);
        if (n > 1) strcpy(req.data, data);

        process_pdu(&req, &res);
        printf("%s\n", res.data);

        if (strcasecmp(cmd, "WRITE") == 0 || strcasecmp(cmd, "DELETE") == 0)
            broadcast_pdu(&req, -1);

        printf("> ");
        fflush(stdout);
    }

    close(srv);
    return 0;
}
