#ifndef TEST_H__
#define TEST_H__
#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/msg.h>

#define SO_BLOCK_SIZE 5
#define SO_REGISTRY_SIZE 1000
#define _XOPEN_SOURCE 700
#define CLOCK_REALTIME 0
#define sender(x) (x < 0 ? -1 : getpid())
#define random_in_range(lower, upper) ((rand() % (upper - lower + 1)) + lower)
#define MAX_PRINT 11 /* the maximum number of Users that will be printed */


typedef struct transaction{
    long timestamp;
    int sender;
    int receiver;
    int quantity;
    int reward;
}Transaction;

typedef struct block{
    int id;
    Transaction transactions[SO_BLOCK_SIZE];
}Block;

typedef struct settings{
    int SO_USERS_NUM;
    int SO_NODES_NUM;
    int SO_BUDGET_INIT;
    int SO_REWARD;
    long SO_MIN_TRANS_GEN_NSEC;
    long SO_MAX_TRANS_GEN_NSEC;
    int SO_RETRY;
    int SO_TP_SIZE;
    long SO_MIN_TRANS_PROC_NSEC;
    long SO_MAX_TRANS_PROC_NSEC;
    int SO_SIM_SEC;
    int SO_FRIENDS_NUM;
    int SO_HOPS;
}Settings;

/* support struct */
typedef struct info{
    int pid;
    int value;
}Info;

/* struct for the response on message queue */
typedef struct msg_buffer{
    long mesg_type;
    Info info;
}Message;

/* struct for the transaction infos on message queue */
typedef struct trsn_buffer{
    long mesg_type;
    Transaction transaction;
}TransactionQueue;

union semun{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

extern Settings settingsSet;

int createRegistrySM(key_t key);
int createArraySM(key_t key);
int createIdSM(key_t key);
void destroySharedMemory(int id);
int createSharedMemory(key_t key, int size, char* type);
Block* attachSharedMemory(int sm_id);
int* attachArraysSharedMemory(int sm_id);
void signal_handler(int sig);
int sem_set_val(int sem_id, int sem_index, int sem_val);
int sem_get_val(int sem_id, int sem_index);
int sem_lock(int sem_id, int sem_index);
int sem_release(int sem_id, int sem_index);
int sem_create(key_t key, int nsems);
void sem_remove(int sem_id);
int read_string(FILE *file);
void freeMemory(Block *registry, int *idRegistry, int *arrayUsers);
int* copyArray(int const * arrayUsers);
void addDataToUser(Info *usersData, Transaction t);
void addDataToNode(Info *nodesData, Transaction t);
void orderArray(Info *usersData);
int processData(char* ptr, int buffersize);
int calcTotal(int *pid);
void processFinalQueue(int *num_usr_alive);
int calcUsersAlive();
void startSimulation(int sm_idBook, int sm_idUsers, int sm_idBlocks);


#endif 