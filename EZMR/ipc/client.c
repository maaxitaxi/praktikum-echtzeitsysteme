#include "common.h"
#include <strings.h>

int shmid, semid;
SharedList *shm;

void connect_ipc() {
    shmid = shmget(SHM_KEY, sizeof(SharedList), 0666);
    if (shmid < 0) {
        perror("shmget (laeuft der Server?)");
        exit(1);
    }
    shm = (SharedList *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    semid = semget(SEM_KEY, 1, 0666);
    if (semid < 0) { perror("semget"); exit(1); }
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

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Aufruf: %s <WRITE|READ|DELETE|LIST> [text/index]\n", argv[0]);
        return 1;
    }

    connect_ipc();

    char *cmd = argv[1];
    char *data = (argc > 2) ? argv[2] : "";

    if (strcasecmp(cmd, "WRITE") == 0) {
        add_item(data);
        printf("hinzugefuegt: \"%s\" (jetzt %d Eintraege)\n", data, shm->count);
    } else if (strcasecmp(cmd, "READ") == 0) {
        int idx = atoi(data);
        char *v = get_item(idx);
        if (v[0]) printf("[%d] %s\n", idx, v);
        else printf("Index %d gibt es nicht\n", idx);
    } else if (strcasecmp(cmd, "DELETE") == 0) {
        int idx = atoi(data);
        del_item(idx);
        printf("Index %d geloescht\n", idx);
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char tmp[1024];
        format_list(tmp);
        printf("%s", tmp);
    } else {
        printf("unbekanntes Kommando: %s\n", cmd);
    }

    shmdt(shm);
    return 0;
}
