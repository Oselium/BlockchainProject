#include "../Headers/MasterLib.h"
#include "../Headers/UserLib.h"
#include "../Headers/NodeLib.h"

/* shared memory */
int sm_idBook, sm_idUsers, sm_idBlocks, msqid = 0, receivedSpecialTrans = 0;
int* nodes, *idRegistry, *arrayUsers, *totalUsers;
Block *registry;

/* global vars for the signal handling */
int volatile mstr_exit_sig;
char reason[50] = "Lack of money";

/* struct of settings read from a file */
Settings settingsSet = {0,0,0,0,0,0,0,0,0,0,0,0,0};

int createRegistrySM(key_t key){
    int sm_id;
    if((sm_id = shmget(key, SO_REGISTRY_SIZE * sizeof(Block), IPC_CREAT|0666)) == -1) { /*CREAT|0666 defines that the process will be created with access for r/w */
 		perror("Master shmget1 failed");
        destroySharedMemory(sm_id);
        exit(EXIT_FAILURE);
 	} 
    return sm_id;
}

int createArraySM(key_t key){
    int sm_id;
    if((sm_id = shmget(key, settingsSet.SO_USERS_NUM * sizeof(int), IPC_CREAT|0666)) == -1) { /*CREAT|0666 defines that the process will be created with access for r/w */
 		perror("Master shmget2 failed");
        destroySharedMemory(sm_id);
        exit(EXIT_FAILURE);
 	} 
    return sm_id;
}

int createIdSM(key_t key){
    int sm_id;
    if((sm_id = shmget(key, sizeof(int), IPC_CREAT|0666)) == -1) { /*CREAT|0666 defines that the process will be created with access for r/w */
 		perror("Master shmget3 failed");
        destroySharedMemory(sm_id);
        exit(EXIT_FAILURE);
 	} 
    return sm_id;
}

void destroySharedMemory(int id){
    int result;
    if((result = shmctl(id, IPC_RMID, NULL)) < 0)
        perror("Master: error deleting sm");
}

/* create semaphore */
int sem_create(key_t key, int nsems){
    return semget(key, nsems, IPC_CREAT | IPC_EXCL | 0666);
}

/* deallocate semaphore */
void sem_remove(int sem_id){
    semctl(sem_id, 0, IPC_RMID);
}

/* set a semaphore */
int sem_set_val(int sem_id, int sem_index, int sem_val){
    return semctl(sem_id, sem_index, SETVAL, sem_val);
}

/* get a semaphore */
int sem_get_val(int sem_id, int sem_index){
    return semctl(sem_id, sem_index, GETVAL);
}

/* lock a resource */
int sem_lock(int sem_id, int sem_index){
    struct sembuf sem;
    sem.sem_num = sem_index;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    return semop(sem_id, &sem, 1);
}

/* unlock a resource */
int sem_release(int sem_id, int sem_index){
    struct sembuf sem;
    sem.sem_num = sem_index;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    return semop(sem_id, &sem, 1);
}

/* Initialize the struct for the settings with values taken from file. It also gives error if there's a random line not contemplated. It does NOT check for the right syntax.*/
int read_string(FILE *file) {
    int ret;
    char name[128];
    char val[128];
    ret = 0;
    while(fscanf(file, "%127[^=]=%127[^\n]%*c", name, val) == 2){
        ret = 1;
        if(name[0] == '#'){
            /* comment found, skipping the line */
        }else if(strcmp(name, "SO_USERS_NUM") == 0){
            settingsSet.SO_USERS_NUM = atoi(val);
        }else if(strcmp(name, "SO_NODES_NUM") == 0){
            settingsSet.SO_NODES_NUM = atoi(val);
        }else if(strcmp(name, "SO_BUDGET_INIT") == 0){
            settingsSet.SO_BUDGET_INIT = atoi(val);
        }else if(strcmp(name, "SO_REWARD") == 0){
            settingsSet.SO_REWARD = atoi(val);
        }else if(strcmp(name, "SO_MIN_TRANS_GEN_NSEC") == 0){
            settingsSet.SO_MIN_TRANS_GEN_NSEC = atol(val);
        }else if(strcmp(name, "SO_MAX_TRANS_GEN_NSEC") == 0){
            settingsSet.SO_MAX_TRANS_GEN_NSEC = atol(val);
        }else if(strcmp(name, "SO_RETRY") == 0){
            settingsSet.SO_RETRY = atoi(val);
        }else if(strcmp(name, "SO_TP_SIZE") == 0){
            settingsSet.SO_TP_SIZE = atoi(val);
        }else if(strcmp(name, "SO_MIN_TRANS_PROC_NSEC") == 0){
            settingsSet.SO_MIN_TRANS_PROC_NSEC = atol(val);
        }else if(strcmp(name, "SO_MAX_TRANS_PROC_NSEC") == 0){
            settingsSet.SO_MAX_TRANS_PROC_NSEC = atol(val);
        }else if(strcmp(name, "SO_SIM_SEC") == 0){
            settingsSet.SO_SIM_SEC = atoi(val);
        }else if(strcmp(name, "SO_FRIENDS_NUM") == 0){
            settingsSet.SO_FRIENDS_NUM = atoi(val);
        }else if(strcmp(name, "SO_HOPS") == 0){
            settingsSet.SO_HOPS = atoi(val);
        }else{
            ret = -1;
            break;
        }
    }
    return ret;
}

int* copyArray(int const * arrayUsers){
    int * p = malloc(settingsSet.SO_USERS_NUM * sizeof(int));
    memcpy(p, arrayUsers, settingsSet.SO_USERS_NUM * sizeof(int));
    return p;
}

void freeMemory(Block *registry, int *idRegistry, int *arrayUsers){
    if(msgctl(msqid, IPC_RMID, NULL) == -1){ /* delete the queue */
        perror("Master: msgctl can't delete queue");
        exit(EXIT_FAILURE);
    }
    free(nodes);
    destroySharedMemory(sm_idBlocks);
    destroySharedMemory(sm_idBook);
    destroySharedMemory(sm_idUsers);
}

void addDataToUser(Info *usersData, Transaction t){
    int i;
    for(i=0; i < settingsSet.SO_USERS_NUM; i++){
        if(usersData[i].pid == t.sender) usersData[i].value = usersData[i].value - t.quantity;
        else if(usersData[i].pid == t.receiver) usersData[i].value = usersData[i].value + t.quantity;
    }
}

void addDataToNode(Info *nodesData, Transaction t){
    int i;
    for(i = 0; i < settingsSet.SO_NODES_NUM; i++){
        if(nodesData[i].pid == t.receiver) nodesData[i].value = nodesData[i].value + t.quantity;
    }
}

void orderArray(Info *usersData){
    int i, k;
    Info tmp;
    for(i = 0; i < settingsSet.SO_USERS_NUM; i++){
        for (k = i + 1; k < settingsSet.SO_USERS_NUM; k++){
            if (usersData[i].value > usersData[k].value){
                tmp =  usersData[i];
                usersData[i] = usersData[k];
                usersData[k] = tmp;
            }
        }
    }
}

/* print informations every second and read the queue of messages until it receives a signal */
int processData(char* ptr, int buffersize){
    Message message;
    char string[8192], temp[30];
    int count = 0, i, k, usersAlive, seconds = 0, max = 0;
    pid_t pid;
    struct timespec timeSleep, timeSleep2;
    Info* usersData = malloc(sizeof(Info) * settingsSet.SO_USERS_NUM);
    Info* nodesData = malloc(sizeof(Info) * settingsSet.SO_NODES_NUM);
    if(MAX_PRINT < settingsSet.SO_USERS_NUM) max = 1;
    timeSleep.tv_sec = 1;
    timeSleep.tv_nsec = 0;
    string[0] = '\0'; /* initialize the string */
    for(i=0; i < settingsSet.SO_USERS_NUM; i++) usersData[i].pid = totalUsers[i];
    for(i=0; i < settingsSet.SO_NODES_NUM; i++) nodesData[i].pid = nodes[i];
    switch(pid = fork()){
        case -1: /* error */
            perror("Could not create child");
            exit(EXIT_FAILURE);
            break;
        case 0: /* child */
            while(mstr_exit_sig == 0){ /* wait message */
                if(receivedSpecialTrans){
                    specialTransaction(nodes);
                    receivedSpecialTrans = 0;
                }
                for(i=0; i < settingsSet.SO_USERS_NUM; i++){ usersData[i].value = settingsSet.SO_BUDGET_INIT; }
                for(i=0; i < settingsSet.SO_NODES_NUM; i++){ nodesData[i].value = 0; }
                usersAlive = calcUsersAlive();
                for(i = 0; i < *idRegistry; i++){ /* calculating balances */
                    for(k = 0; k<SO_BLOCK_SIZE;k++){
                        if(k == (SO_BLOCK_SIZE-1)) addDataToNode(nodesData, registry[i].transactions[k]); 
                        else addDataToUser(usersData, registry[i].transactions[k]);                 
                    }
                }
                printf("------------- ");
                printf("%d second passed\n", seconds);
                seconds++;

                printf("\nUsers alive: %d\n", usersAlive);
                if(max == 0){
                    for(i = 0; i < settingsSet.SO_USERS_NUM; i++) printf("[%d]: %d\n", usersData[i].pid, usersData[i].value);
                }else{
                    printf("(Printing only the %d most/less important one)\n", MAX_PRINT);
                    orderArray(usersData); /* order the array to get the most/less important values */
                    for(i = 0; i < MAX_PRINT/2; i++) printf("[%d]: %d\n", usersData[settingsSet.SO_USERS_NUM - i - 1].pid, usersData[settingsSet.SO_USERS_NUM - i - 1].value); /* the most important */
                    for(i = 0; i < MAX_PRINT/2; i++) printf("[%d]: %d\n", usersData[(MAX_PRINT/2) - 1 - i].pid, usersData[(MAX_PRINT/2) - 1 - i].value);/* the less important */
                }
                printf("\nNodes:\n\n");
                for(i=0; i < settingsSet.SO_NODES_NUM; i++){
                    printf("[%d]: %d\n", nodesData[i].pid, nodesData[i].value);
                }
                nanosleep(&timeSleep, &timeSleep2);
            }
            exit(EXIT_SUCCESS);
            break;
        default: /* parent */
            while(mstr_exit_sig == 0){ /* wait message */
                if(msgrcv(msqid, &message, sizeof(message), 1,0) == -1 && errno != ENOMSG && errno != EINTR){
                    perror("Master msgrcv failed");
                    exit(EXIT_FAILURE);
                }else if(errno == EINTR){ /* interrupted the system call (because of SIGALRM) so exit the cycle */
                    mstr_exit_sig = 1;
                    break;
                }else if(errno == ENOMSG) continue;
                else{
                    count++;
                    snprintf(temp, sizeof(temp)-1, "[%d]: %d\n", message.info.pid, message.info.value);
                    strcat(string, temp);
                }
                if(count == settingsSet.SO_USERS_NUM) mstr_exit_sig = 1; /* all the arrayUsers are closed */
            }
            signal(SIGUSR1, SIG_IGN);
            if((kill(0, SIGUSR1)) != 0) perror("Master: error sending signal");
            strncpy(ptr, string, buffersize-1);
            ptr[buffersize-1] = '\0';
            free(usersData);
            free(nodesData);
            break;
        }   
    return count;
}



/* check the Node balance from the registry */
int calcTotal(int *pid){
    int i, balance = 0;
    for(i = 0; i < *idRegistry; i++){
        if(registry[i].transactions[SO_BLOCK_SIZE-1].receiver == *pid){
            balance = balance + registry[i].transactions[SO_BLOCK_SIZE-1].quantity;
        }
    }
    return balance;
}

/* reads the queue of messages for the termination infos */
void processFinalQueue(int *num_usr_alive){
    Message message;
    int balance;
    int usr_count = 0;
    int nodes_count = 0;
    printf("\nUsers alive: %d\n", *num_usr_alive);
    while(1){ /* cycle for Users */
        if(usr_count == *num_usr_alive) break;
        if((msgrcv(msqid, &message, sizeof(message), 1, 0)) == -1 && errno != ENOMSG && errno != EINTR){ /* only the messages with type 1 (from Users) */
            perror("Master msgrcv users failed");
            exit(EXIT_FAILURE);
        }else{
            usr_count++;
            printf("[%d]: %d\n", message.info.pid, message.info.value);
        }
    }
    printf("\nNodes:\n");
    while(1){ /* cycle for Nodes */
        if(nodes_count == settingsSet.SO_NODES_NUM) break;
        if(msgrcv(msqid, &message, sizeof(message), 2, 0) == -1 && errno != ENOMSG && errno != EINTR){ /* only the messages with type 2 (from Nodes) */
            perror("Master msgrcv nodes failed");
            exit(EXIT_FAILURE);
        }else{
            nodes_count++;
            printf("[%d]: %d, with %d elements in pool\n", message.info.pid, calcTotal(&message.info.pid), message.info.value);
        }
    }
    printf("\nRegistry: %d/%d blocks full\n\n", *idRegistry, SO_REGISTRY_SIZE);
}

int calcUsersAlive(){
    int i, num_usr_alive = 0;
    for(i = 0; i < settingsSet.SO_USERS_NUM; i++){ /* count the remaining users */
            if(arrayUsers[i] != -1) num_usr_alive++;
    }
    return num_usr_alive;
}

void signal_handler(int sig){
    pid_t user;
    switch(sig){
        case SIGINT: /* Ctrl+C */
            printf("\n[TERMINATION]: SIGINT received, manual TERMINATION");
            freeMemory(registry, idRegistry, arrayUsers);
            kill(0, SIGQUIT); /* kill processes */
            break;
        case SIGALRM: /* the timer ended */
            strcpy(reason, "SIGALRM received (SO_SIM_SEC have passed)\n");
            mstr_exit_sig = 1;
            break;
        case SIGUSR2: /* no space in Registry */
            strcpy(reason, "Registry has no more space\n");
            mstr_exit_sig = 1;
            break;
        case SIGTSTP:
            printf("\n[INTERRUPTION]: SIGTSTP received\n");
            receivedSpecialTrans = 1;
            break;
    }
}

void startSimulation(int sm_idBook, int sm_idUsers, int sm_idBlocks){
    int i, num_usr_alive = 0;
    char pre_usr[8192];
    signal(SIGUSR2, signal_handler);
    signal(SIGALRM, signal_handler);
    signal(SIGCONT, SIG_IGN); /* block the Master from receiving the SIGCONT signal */
    registry = attachSharedMemory(sm_idBook);
    arrayUsers = attachArraysSharedMemory(sm_idUsers);
    idRegistry = attachArraysSharedMemory(sm_idBlocks);
    *idRegistry = 0;

    /* ------- CREATION ------- */
    printf("\nList of %d Nodes and %d Users for this simulation:\n", settingsSet.SO_NODES_NUM, settingsSet.SO_USERS_NUM);
    nodes = createNodes(sm_idBook, sm_idBlocks,  sm_idUsers, &msqid, getpid());
    createUsers(sm_idBook, sm_idUsers, sm_idBlocks, &msqid, nodes);
    
    totalUsers = copyArray(arrayUsers); /* used for print every second */

    /* ------- START SIMULATION ------- */
    printf("\nStarting simulation, timer set for %d seconds\n", settingsSet.SO_SIM_SEC);
    alarm(settingsSet.SO_SIM_SEC); /* start the timer */
    kill(0, SIGCONT); /* resume child process */
    processData(pre_usr, sizeof(pre_usr)); /* the Master will remain here until a specific signal */

    /* ------- STOP SIMULATION ------- */
    num_usr_alive = calcUsersAlive();
    printf("\n[ TERMINATION ]: %s\n", reason);
    printf("\nUsers terminated prematurely: %d\n%s\n", settingsSet.SO_USERS_NUM - num_usr_alive, pre_usr);
    processFinalQueue(&num_usr_alive);
}

/* main */
int main(int argc, char const *argv[]){
    key_t key_B, key_U, key_I, key_M, key_qN;
    FILE *fp = fopen(argv[1], "r");
    if(read_string(fp) != 1){
        printf("The file contains errors or unspecified values.\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    signal(SIGTSTP,signal_handler);

    /* create the keys for the shared memory */
    key_B = ftok(".", 1);
    if ( key_B < 0 ) perror("ftok LB failed");
    key_U = ftok(".", 10);
    if ( key_U < 0) perror("ftok users failed");
    key_I = ftok(".", 20);
    if ( key_I < 0) perror("ftok ID failed");
    key_qN = ftok(".", 30);
    if ( key_qN < 0 ) perror("ftok queue failed");
    sm_idBook = createRegistrySM(key_B); /* shared memory registry */
    sm_idUsers = createArraySM(key_U); /* shared memory users */
    sm_idBlocks = createIdSM(key_I); /* shared memory n. of blocks */

    /* create the Master's public message queue */
    if((key_M = ftok(".", 65)) == -1) perror("ftok Message queue failed");
    if((msqid = msgget(key_M, 0666|IPC_CREAT)) == -1) {
      perror("msgget failed in Master");
      exit(EXIT_FAILURE);
    }

    /* start the simulation, includes the timer */
    mstr_exit_sig = 0;
    signal(SIGINT, signal_handler);
    startSimulation(sm_idBook, sm_idUsers, sm_idBlocks);

    /* clear the simulation */
    freeMemory(registry, idRegistry, arrayUsers);

    exit(EXIT_SUCCESS);
}