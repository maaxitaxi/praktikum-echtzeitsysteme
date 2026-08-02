#include "common.h"
#include <signal.h>
#include <errno.h>
#include <ctype.h>

int shmid, semid;
SharedList *shm;

void init_ipc() {
    shmid = shmget(SHM_KEY, sizeof(SharedList), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }

    shm = (SharedList *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); exit(1); }
    semctl(semid, 0, SETVAL, 1);
}

void sem_lock() {
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void sem_unlock() {
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

void add_item(const char *s) {
    sem_lock();
    if (shm->count < MAX_ITEMS) {
        strncpy(shm->items[shm->count], s, MAX_LEN - 1);
        shm->items[shm->count][MAX_LEN - 1] = '\0';
        shm->count++;
    }
    sem_unlock();
}

void del_item(int idx) {
    sem_lock();
    if (idx >= 0 && idx < shm->count) {
        for (int i = idx; i < shm->count - 1; i++)
            strcpy(shm->items[i], shm->items[i + 1]);
        shm->count--;
    }
    sem_unlock();
}

char *get_item(int idx) {
    static char buf[MAX_LEN];
    buf[0] = '\0';
    sem_lock();
    if (idx >= 0 && idx < shm->count)
        strcpy(buf, shm->items[idx]);
    sem_unlock();
    return buf;
}

void format_list(char *out) {
    out[0] = '\0';
    sem_lock();
    if (shm->count == 0) {
        strcpy(out, "leer");
    } else {
        for (int i = 0; i < shm->count; i++) {
            char line[MAX_LEN + 20];
            snprintf(line, sizeof(line), "[%d] %s\n", i, shm->items[i]);
            strcat(out, line);
        }
    }
    sem_unlock();
}

void process_pdu(PDU *req, PDU *res) {
    memset(res, 0, sizeof(PDU));
    char c[32];
    strcpy(c, req->cmd);
    for (int i = 0; c[i]; i++) c[i] = toupper(c[i]);

    if (!strcmp(c, "READ")) {
        int idx = atoi(req->data);
        char *v = get_item(idx);
        if (v[0]) {
            strcpy(res->data, v);
            strcpy(res->cmd, "OK");
        } else {
            sprintf(res->data, "kein Eintrag bei %d", idx);
            strcpy(res->cmd, "ERR");
        }
    } else if (!strcmp(c, "WRITE")) {
        if (strlen(req->data)) {
            add_item(req->data);
            sprintf(res->data, "hinzugefuegt (%d)", shm->count);
            strcpy(res->cmd, "OK");
        } else {
            strcpy(res->data, "keine Daten");
            strcpy(res->cmd, "ERR");
        }
    } else if (!strcmp(c, "DELETE")) {
        int idx = atoi(req->data);
        del_item(idx);
        sprintf(res->data, "geloescht %d", idx);
        strcpy(res->cmd, "OK");
    } else if (!strcmp(c, "LIST")) {
        char tmp[1024];
        format_list(tmp);
        strcpy(res->data, tmp);
        strcpy(res->cmd, "OK");
    } else {
        sprintf(res->data, "unbekanntes Kommando: %s", req->cmd);
        strcpy(res->cmd, "ERR");
    }

    res->data[sizeof(res->data) - 1] = '\0';
    res->cmd[sizeof(res->cmd) - 1] = '\0';
}

void cleanup() {
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

int main() {
    init_ipc();
    printf("Server gestartet (shmid=%d, semid=%d)\n", shmid, semid);
    printf("Kommandos: WRITE <text>, READ <idx>, DELETE <idx>, LIST, exit\n");

    signal(SIGINT, cleanup);

    char line[512];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, "exit")) break;

        char cmd[32], data[256];
        int n = sscanf(line, "%31s %255[^\n]", cmd, data);
        if (n < 1) continue;

        PDU req, res;
        memset(&req, 0, sizeof(req));
        strcpy(req.cmd, cmd);
        if (n > 1) strcpy(req.data, data);

        process_pdu(&req, &res);
        printf("%s\n", res.data);
    }

    cleanup();
    return 0;
}
