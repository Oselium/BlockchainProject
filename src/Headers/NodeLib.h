#ifndef H_INCLUDED
#define H_INCLUDED

int attachIDBSharedMemory(int sm_id);
void notifyAndDie_Node();
void node_signal_handler(int sig);
void processBlock(Block *book);
int checkBook(Block *book, int *idBlocksSM, Transaction t);
void waitConnection(int *users);
int* createNodes(int sm_idBook, int sm_idBlocks,  int sm_idUsers, int *msqid, int ppid);

#endif
