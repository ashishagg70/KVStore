#ifndef ServerEpoll
#define ServerEpoll

#include<pthread.h>
#include"KVCache.h"
#define LISTENBACKLOGQUEUE 5
#define MESSAGESIZE 513
#define MESSAGE_KEY_SIZE 256
#define MESSAGE_VALUE_SIZE 256
#define NUMBER_OF_API_SUPPORTED 3
#define DEBUG_ENABLED 0
#if DEBUG_ENABLED
  #define DEBUG(a) printf a
#else
  #define DEBUG(a) (void)0
#endif

#if DEBUG_ENABLED
  #define DEBUG2(a) fprintf a
#else
  #define DEBUG2(a) (void)0
#endif

typedef struct  ServerConfiguration
{
    char * host;
    char * port;
    unsigned long  threadPoolCount;
    unsigned long  threadPoolGrowth;
    unsigned long  clientsPerThread;
    char * cacheReplacement;
    unsigned long  cacheSize;
    unsigned long noOfFiles;
    unsigned long fileGrow;
    
}ServerConfiguration;

typedef struct ThreadNode{
    int id;
    int noOfClient;
    int epfd;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    struct ThreadNode * next;
    pthread_t threadHandle;
    struct epoll_event *event;
}ThreadNode;

void signalHandler(int signal);
ThreadNode * getThreadNode(int* id);
void sendResponse(int fd, unsigned char returnStatus, char * message);
void growThreadPool( int oldCount);
void* workerThread(void* index);
#endif