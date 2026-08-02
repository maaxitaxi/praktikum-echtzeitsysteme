#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

#define SHM_KEY 1234
#define SEM_KEY 1235
#define MAX_ITEMS 100
#define MAX_LEN 256

typedef struct {
    char items[MAX_ITEMS][MAX_LEN];
    int count;
} SharedList;

int shmid, semid;
SharedList *shm;

void sem_lock() {
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void sem_unlock() {
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

void init_ipc() {
    shmid = shmget(SHM_KEY, sizeof(SharedList), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    shm = (SharedList*)shmat(shmid, NULL, 0);
    if (shm == (void*)-1) { perror("shmat"); exit(1); }

    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    semctl(semid, 0, SETVAL, 1);
}

void cleanup() {
    shmdt(shm);
}

void add_item(const char *s) {
    sem_lock();
    if (shm->count < MAX_ITEMS) {
        strncpy(shm->items[shm->count], s, MAX_LEN-1);
        shm->items[shm->count][MAX_LEN-1] = '\0';
        shm->count++;
    }
    sem_unlock();
}

void del_item(int idx) {
    sem_lock();
    if (idx >= 0 && idx < shm->count) {
        for (int i = idx; i < shm->count-1; i++)
            strcpy(shm->items[i], shm->items[i+1]);
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

void list_items() {
    sem_lock();
    if (shm->count == 0) {
        printf("(leer)\n");
    } else {
        for (int i = 0; i < shm->count; i++)
            printf("[%d] %s\n", i, shm->items[i]);
    }
    sem_unlock();
}

int main() {
    init_ipc();
    printf("Peer gestartet - shmid %d, semid %d\n", shmid, semid);
    printf("Kommandos: WRITE <text>, READ <index>, DELETE <index>, LIST, exit\n");

    char line[512];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, "exit") == 0) break;
        if (strlen(line) == 0) continue;

        char cmd[32], data[256];
        int n = sscanf(line, "%31s %255[^\n]", cmd, data);
        if (n < 1) continue;

        for (int i = 0; cmd[i]; i++) cmd[i] = toupper(cmd[i]);

        if (strcmp(cmd, "WRITE") == 0) {
            if (n > 1) {
                add_item(data);
                printf("hinzugefuegt: %s\n", data);
            } else {
                printf("Fehler: Text fehlt\n");
            }
        } else if (strcmp(cmd, "READ") == 0) {
            if (n > 1) {
                int idx = atoi(data);
                char *v = get_item(idx);
                if (v[0]) printf("[%d] %s\n", idx, v);
                else printf("Index %d gibt es nicht\n", idx);
            } else {
                printf("Fehler: Index fehlt\n");
            }
        } else if (strcmp(cmd, "DELETE") == 0) {
            if (n > 1) {
                int idx = atoi(data);
                del_item(idx);
                printf("Index %d geloescht\n", idx);
            } else {
                printf("Fehler: Index fehlt\n");
            }
        } else if (strcmp(cmd, "LIST") == 0) {
            list_items();
        } else {
            printf("unbekanntes Kommando: %s\n", cmd);
        }
    }

    cleanup();
    return 0;
}
