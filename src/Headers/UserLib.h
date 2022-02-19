#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED
#include "../Headers/MasterLib.h"

void notifyAndDie(int balance);
int createUsersSharedMemory();
int calcBalance(Transaction *sendedTrans, int *st_size);
int calcReward(int moneyToSend);
void user_signal_handler(int sig);
int* attachArraysSharedMemory(int sm_id);
Block* attachSharedMemory(int sm_id);
int createCommunication(TransactionQueue t, int node, Transaction *sendedTrans, int *size);
int sendTransaction(int node, int *users, Transaction *sendedTrans, int *size);
int chooseNode(int *nodes);
int chooseReceiver(int *users);
void dynamicArraySize(Transaction **sendedTrans, int *sizeST);
void createUsers(int sm_idBook, int sm_idUsers, int sm_idBlocks,  int *msqid, int *nodes);
void specialTransaction(int *nodes);

#endif