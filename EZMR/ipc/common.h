#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY 1234
#define SEM_KEY 1235

#define MAX_ITEMS 100
#define MAX_LEN   256

typedef struct {
    char items[MAX_ITEMS][MAX_LEN];
    int  count;
} SharedList;

typedef struct {
    char cmd[32];
    char data[256];
    int  len;
} PDU;

#endif
