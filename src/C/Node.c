#include "../Headers/MasterLib.h"
#include "../Headers/NodeLib.h"

int m_msqid, semaphoreID, *idBlocksSM, poolCount, p_pid = 0;
Block *book;
Transaction *transactionPool;


int attachIDBSharedMemory(int sm_id){
    int res;
    res = (uintptr_t) shmat(sm_id, NULL, 0);
    if(res < 0)
        perror("Node int shmat failed");
    return res;
}

/* send you pid and the pool elements to the Master and die */
void notifyAndDie_Node(){
    Message message;
    message.mesg_type = 2;
    message.info.pid = getpid();
    message.info.value = poolCount;
    sem_lock(semaphoreID, 0);
    if((msgsnd(m_msqid, &message, sizeof(message), 0)) == -1){ /* send the recap to the master */
        perror("Node: message queue failed");
        exit(EXIT_FAILURE);
    }
    sem_release(semaphoreID, 0);
    free(transactionPool);
}

void node_signal_handler(int sig){
    switch(sig){
        case SIGUSR1:
            sem_lock(semaphoreID, 0);
            notifyAndDie_Node();
            sem_release(semaphoreID, 0);
            sem_remove(semaphoreID);
            exit(EXIT_SUCCESS);
    }
}

void processBlock(Block *book){
    int i, totalReward;
    struct timespec start, timeSleep, timeSleep2;
    Transaction rewardTrans;
    Block block;
    totalReward = 0;
    srand(time(NULL) + getpid());
    timeSleep.tv_sec = 0;
    timeSleep.tv_nsec = random_in_range(settingsSet.SO_MIN_TRANS_PROC_NSEC, settingsSet.SO_MAX_TRANS_PROC_NSEC);

    for(i = 0; i < SO_BLOCK_SIZE - 1; i++){
        block.transactions[i] = transactionPool[i];
        totalReward += transactionPool[i].reward;
        memset(&transactionPool[i], 0, sizeof(Transaction)); /* reset the slots of transPool to 0 and set his bytes to a Transaction */
    }
    poolCount = poolCount - (SO_BLOCK_SIZE-1);
    /* Create the trans with rewards for the Node */
    rewardTrans.quantity = totalReward;
    rewardTrans.sender = sender(-1); 
    rewardTrans.receiver = getpid();
    if(clock_gettime(CLOCK_REALTIME, &start) == -1) {
        perror("clock gettime on Node");
        exit(EXIT_FAILURE);
    }
    /* create the reward transaction for the block */
    rewardTrans.timestamp = start.tv_nsec;
    rewardTrans.reward = 0;
    block.transactions[SO_BLOCK_SIZE-1] = rewardTrans;
    if(nanosleep(&timeSleep, &timeSleep2) < 0){ /* Node sleep for a random number of nsec */
      printf("Nano sleep system call failed in Node\n");
      exit(EXIT_FAILURE);
    }
    block.id = *idBlocksSM;
    book[*idBlocksSM] = block;
    *idBlocksSM = *idBlocksSM + 1;
}

/* check if the registry is full or the transaction already exist on it */
int checkBook(Block *book,  int *idBlocksSM, Transaction t){
    int i,k;
    if(*idBlocksSM < SO_REGISTRY_SIZE){ /* enough space */
        for(i = 0; i<*idBlocksSM; i++){
            for(k = 0; k<SO_BLOCK_SIZE; k++){
                if(book[i].transactions[k].sender == t.sender 
                && book[i].transactions[k].receiver == t.receiver 
                && book[i].transactions[k].timestamp == t.timestamp ){
                    printf("[Node %d]: TRANS ALREADY EXIST IN THE REGISTRY", getpid());
                    return -1;
                }
            }
        }
    }else{ /* send signal to stop everything */
        return -2; 
    }
    return 1;
}

void waitConnection(int *users){
    TransactionQueue message;
    Message response;
    int i;
    int transCheck = 1;
    poolCount = 0;
    while(1){ /* wait messages */
        if(msgrcv(m_msqid, &message, sizeof(message), getpid(), 0) == -1 && errno != ENOMSG && errno != EINTR){
            perror("Node msgrcv failed");
            exit(EXIT_FAILURE);
        }else if(errno == ENOMSG) continue; /* no data on the queue */
        else{ /* data on the queue */
            response.info.pid = getpid();
            if(poolCount < settingsSet.SO_TP_SIZE){ /* check if pool is full */
                sem_lock(semaphoreID, 0);
                if(*idBlocksSM > 0) transCheck = checkBook(book, idBlocksSM, message.transaction);
                if(transCheck == -2){ /* no more space, send to master */
                    printf("Sending signal to MASTER %d\n", p_pid);
                    kill(p_pid, SIGUSR2);
                }
                sem_release(semaphoreID, 0);
                if(transCheck == 1){ /* the transaction does not exist in the registry and there's enough space */
                    transactionPool[poolCount] = message.transaction;
                    poolCount = poolCount + 1;
                    response.info.value = 1;
                    if(poolCount >= SO_BLOCK_SIZE - 1){ /* there are sufficient transactions to process a block */
                        sem_lock(semaphoreID, 0);
                        processBlock(book);
                        sem_release(semaphoreID, 0);
                    }
                }else response.info.value = transCheck; /* trans already exist in the registry */         
            }else response.info.value = 0; /* pool full */
            response.mesg_type = (long)message.transaction.sender;

            sem_lock(semaphoreID, 0);
            if((msgsnd(m_msqid, &response, sizeof(response), 0)) == -1){ /* send response to the User's queue */
                perror("Node msgsnd to User failed");
                exit(EXIT_FAILURE);
            }
            sem_release(semaphoreID, 0);
            sem_remove(semaphoreID);
        }
    }
}

int* createNodes(int sm_idBook, int sm_idBlocks,  int sm_idUsers, int *msqid, int ppid){
    int capacity = 0, i;
    int* users;
    pid_t pid;
    static int *children;
    children = malloc(sizeof(int) * settingsSet.SO_NODES_NUM);
    transactionPool = malloc(settingsSet.SO_TP_SIZE * sizeof(Transaction));
    book = attachSharedMemory(sm_idBook);
    idBlocksSM = attachArraysSharedMemory(sm_idBlocks);
    users = attachArraysSharedMemory(sm_idUsers); /* save into the list the users in shared memory */
    m_msqid = *msqid;
    signal(SIGUSR1, node_signal_handler);
    printf("[Nodes]:\n");
    semaphoreID = sem_create(IPC_PRIVATE, 1);
    if(semaphoreID == -1) perror("sem create in parent error");
    if(sem_set_val(semaphoreID, 0, 1) == -1){
        perror("error in sem set val");
        sem_remove(semaphoreID);
    }
    for(i = 0; i < settingsSet.SO_NODES_NUM; ++i){
        switch(pid = fork()){
        case -1: /* error */
            perror("\nCould not create child Node.\n");
            exit(EXIT_FAILURE);
            break;
        case 0: /* child */  
            printf("- %d\n", getpid());
            raise(SIGSTOP); /* freeze the child waiting for SIGCONT */
            p_pid = ppid;
            waitConnection(users);
            exit(EXIT_SUCCESS);
            break;
        default: /* parent */
            waitpid(pid, NULL, WUNTRACED); /* wait until child is frozen with SIGSTOP */
            children[i] = pid;
            break;
        }
    }
    free(transactionPool);
    return children;
}