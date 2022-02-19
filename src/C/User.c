#include "../Headers/UserLib.h"
#include "../Headers/MasterLib.h"

int sem_id, balance, mstr_msqid, sizeST = 100, elements_in_st = 0;
int *idBlocksSMU, *users;
Block *bookU;
Transaction *sendedTrans;

/* send pid and balance to the Master and die */
void notifyAndDie(int balance){
    Message message;
    message.mesg_type = 1;
    message.info.pid = getpid();
    message.info.value = balance;
    if((msgsnd(mstr_msqid, &message, sizeof(message), 0)) == -1){
        perror("User: message queue failed");
        exit(EXIT_FAILURE);
    }
    free(sendedTrans);
}

void user_signal_handler(int sig){
    switch(sig){
        case SIGUSR1:
            sem_lock(sem_id, 0);
            notifyAndDie(balance);
            sem_release(sem_id, 0);
            sem_remove(sem_id);
            exit(EXIT_SUCCESS);
    }
}

int* attachArraysSharedMemory(int sm_id){
    int* res;
    res = (int*) shmat(sm_id, NULL, 0);
    if(res < (int*) (0))
        perror("User arr shmat failed");
    return res;
}

Block* attachSharedMemory(int sm_id){
    Block* res;
    res = (Block*) shmat(sm_id, NULL, 0);
    if( res < (Block*) (0))
        perror("User shmat failed");
    return res;
}

int calcBalance(Transaction *sendedTrans, int *elements_in_st){
    int i, j, k;
    int sum = settingsSet.SO_BUDGET_INIT;
    for(i = 0; i < *idBlocksSMU; i++){ /* transactions in registry where the User is the receiver */
        for(k = 0; k<SO_BLOCK_SIZE;k++){
            if(bookU[i].transactions[k].receiver == getpid()){ 
                sum = sum + bookU[i].transactions[k].quantity;
            }
        }
    }
    for(j = 0; j < *elements_in_st; j++){ /* transactions in pool */
        sum = sum - (sendedTrans[j].quantity + sendedTrans[j].reward);
    }
    return sum;
}

int calcReward(int moneyToSend){
    int reward = (int) ((moneyToSend * settingsSet.SO_REWARD) / 100);
    if(reward < 1) reward = 1;
    return reward;
}

int createCommunication(TransactionQueue t, int node, Transaction *sendedTrans, int *size){
    int i, ret;
    struct timespec ts, ts2;
    Message response;
    ts.tv_sec = 0;
    ts.tv_nsec = random_in_range(settingsSet.SO_MIN_TRANS_GEN_NSEC, settingsSet.SO_MAX_TRANS_GEN_NSEC);
    sem_lock(sem_id, 0);
    if((msgsnd(mstr_msqid, &t, sizeof(t), 0)) == -1 && errno != ENOMSG){ /* send transaction */
        perror("User msgsnd failed");
        exit(EXIT_FAILURE);
    }
    sem_release(sem_id, 0);

    if(nanosleep(&ts, &ts2) < 0){  /* simulating a wait for the Node's answer */
      printf("Nano sleep system call failed\n");
      return -1;
    }

    while(1){ /* waiting response */
        if(msgrcv(mstr_msqid, &response, sizeof(response), getpid(), 0) == -1 && errno != ENOMSG){ /* only the messages for this User */
            perror("User msgrcv users failed");
            exit(EXIT_FAILURE);
        }else if(errno == ENOMSG) continue; /* still waiting a message */
        else{ /* save the value for checking later */
            ret = response.info.value;
            break;
        }
    }
    return ret;
}

int sendTransaction(int node, int *users, Transaction *sendedTrans, int *size){
    int moneyToSend, response_value;
    struct timespec requestStart;
    TransactionQueue tq;
    Transaction t;
    if(clock_gettime(CLOCK_REALTIME, &requestStart) == -1) perror("clock gettime");
    moneyToSend = random_in_range(2, balance); /* random number between 2 and value of balance  */
    t.reward = calcReward(moneyToSend);
    t.quantity = moneyToSend - t.reward;
    t.receiver = chooseReceiver(users);
    t.sender = sender(1);
    t.timestamp = requestStart.tv_nsec;
    tq.mesg_type = node;
    tq.transaction = t;
    response_value = createCommunication(tq, node, sendedTrans, size);

    if(response_value == 1){
        sendedTrans[*size] = t;
        *size = *size + 1;
    }
    return response_value;
}

/* choose randomly the Nodes excluding the one passed */
int chooseNode(int *nodes){
    int index;
    index = rand() % settingsSet.SO_NODES_NUM;
    return nodes[index];
}

int chooseReceiver(int *users){
    int index;
    do{
        index = rand() % settingsSet.SO_USERS_NUM;
    }while(users[index] == getpid() || users[index] <= 0); /* if the randomly picked user is himself or is dead or still being created, pick another */
    return users[index];
}

void dynamicArraySize( Transaction **sendedTrans, int *sizeST){
    *sizeST = *sizeST * 2;
    *sendedTrans = realloc(*sendedTrans, sizeof(Transaction) * (*sizeST));
    if(*sendedTrans == NULL) perror("error incrementing array size");
}

/* main function that generates and run the Users */
void createUsers(int sm_idBook, int sm_idUsers, int sm_idBlocks,  int *msqid, int *nodes){
    int i, node, k = 0, trans_response; /* keep track of the sendedTrans array size */
    Message message;
    sendedTrans = malloc(sizeof(Transaction) * sizeST); /* array for the User to check his transactions starting from a custom value */
    bookU = attachSharedMemory(sm_idBook);
    idBlocksSMU = attachArraysSharedMemory(sm_idBlocks);
    users = attachArraysSharedMemory(sm_idUsers);
    signal(SIGUSR1, user_signal_handler);
    mstr_msqid = *msqid; /* save into the global var the Master queue */
    balance = settingsSet.SO_BUDGET_INIT; /* set the initial balance */

    printf("[Users]:\n");
    sem_id = sem_create(IPC_PRIVATE, 1); /* create the semaphore */
    if(sem_id == -1) perror("sem create in parent error");
    if(sem_set_val(sem_id, 0, 1) == -1){
        perror("error in sem set val");
        sem_remove(sem_id);
    }
    for(i = 0; i < settingsSet.SO_USERS_NUM; i++){
        pid_t pid = fork();
        if(pid == -1){ /* fork has failed */
            perror("Fork failed");
            exit(0);
        }else if(pid == 0){ /* child process */
            srand(time(NULL) + getpid()); /* seed for generating random numbers */
            printf("- %d\n", getpid());            
            sem_lock(sem_id, 0);
            users[i] = getpid();
            sem_release(sem_id, 0);
            raise(SIGSTOP); /* process creation done, waiting the signal to proceed */
            while(k <= settingsSet.SO_RETRY){
                sem_lock(sem_id, 0);
                balance = calcBalance(sendedTrans, &elements_in_st);
                sem_release(sem_id, 0);
                if(balance >= 2){
                        node = chooseNode(nodes); /* pick a random index node */
                        trans_response = sendTransaction(node, users, sendedTrans, &elements_in_st);
                        if(trans_response == 0 || trans_response == -1) k++; /* the pool was full, try again excluding the last Node */
                        if(sizeST == elements_in_st) dynamicArraySize(&sendedTrans, &sizeST); /* if sendedTrans is full, expand it */
                }else k++;                
            }
            sem_lock(sem_id, 0);
            users[i] = -1; /* remove user from the shared memory */
            notifyAndDie(balance); /* the balance is < 2 so notify the Master with the termination info */
            sem_release(sem_id, 0);
            sem_remove(sem_id);
            exit(EXIT_SUCCESS);
        }else{ /* parent process */
            waitpid(pid, NULL, WUNTRACED);
        }
    }
    free(sendedTrans);
}

void specialTransaction(int *nodes){
    int node;
    node = chooseNode(nodes);
    sendTransaction(node, users, sendedTrans, &elements_in_st);
    kill(0,SIGCONT);
} 