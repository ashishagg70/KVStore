#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "FileOperations.h"
#include "ServerEpoll.h"
#include <signal.h>
#include <errno.h>
ThreadNode * turn=0;
pthread_mutex_t  globalMutex;
int currentWorkerThreadCount;
int clientPerWorkerThread;
int totalclientConnections=0;
ThreadNode * head=NULL;
ThreadNode * tail=NULL;


void __attribute__((destructor)) dtor(){
    ThreadNode * curr=head;
    while(curr!=tail){
        DEBUG(("destroying id: %d\n", curr->id));
        pthread_kill( curr->threadHandle, SIGUSR1);
        //close(curr->epfd);
        curr->noOfClient=-1;
        pthread_cond_signal(&curr->cv);
        pthread_cond_destroy(&curr->cv);
        pthread_mutex_destroy(&curr->mutex);
        pthread_join(curr->threadHandle, NULL);
        DEBUG(("destroyed id: %d\n", curr->id));
        curr=curr->next;
    }
    DEBUG(("Thread destoyed...\n"));
    kvc_free();
    DEBUG(("Keys saved to file successfully...Bye...Bye...\n"));
}
void signalHandler(int signal){
    DEBUG(("Signal id: %d\n", signal));
    DEBUG(("graceful destroying of server... total threads was: %d\n", currentWorkerThreadCount));
    exit(0);
}
int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    ServerConfiguration* serverConfig =  parseServerConfig();

    currentWorkerThreadCount = serverConfig->threadPoolCount;
    clientPerWorkerThread = serverConfig ->clientsPerThread;
    DEBUG(("%s %s %lu \n", serverConfig->host, serverConfig->port, serverConfig->threadPoolCount));
    DEBUG(("%s %lu %lu \n", serverConfig->cacheReplacement, serverConfig->threadPoolGrowth, serverConfig->cacheSize));
    initFileOperations(serverConfig->noOfFiles);
    kvc_init(serverConfig->cacheSize, serverConfig->cacheReplacement);

    //starting server to accept connections
    //create socket address info 
    struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
    int status = -1;
    if(strcmp(serverConfig->host,"localhost")==0)
        status=getaddrinfo(NULL, serverConfig->port, &hints, &result);
    else
        status=getaddrinfo(serverConfig->host, serverConfig->port, &hints, &result);
    if (status != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
	}

    //created socket fd
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    //bind socket to host address given in address info object(localhost here)
    if ( bind(socket_fd, result->ai_addr, result->ai_addrlen) != 0 ){
		perror("bind()");
		exit(1);
	}

    if ( listen(socket_fd, LISTENBACKLOGQUEUE) != 0 ){
		perror("listen()");
		exit(1);
	}
    DEBUG(("creating worker threads...\n"));
    pthread_mutex_init(&globalMutex, NULL);

    int *id=(int *)malloc(sizeof(int));
    *id=0;
    ThreadNode* curr =  getThreadNode(id);
    tail=curr;
    head=curr;
    tail->next=head;
    
    
     
    for (unsigned long  i=1; i<currentWorkerThreadCount; i++) {
        int *id=(int *)malloc(sizeof(int));
        *id=i;
        ThreadNode* threadNode=getThreadNode(id);
        tail=threadNode;
        curr->next=threadNode;
        tail->next=head;
        curr=threadNode;
    }
    DEBUG(("worker threads created successfully...\n"));
    DEBUG(("Waiting for connection...\n"));
    struct sockaddr_in client;
    turn=head;
    while(1){
        static struct epoll_event ev;
        memset(&client, 0, sizeof (client));
        socklen_t addrlenClient = sizeof(client);
        ev.data.fd = accept(socket_fd,(struct sockaddr*)&client, &addrlenClient);
        ev.events = EPOLLIN | EPOLLOUT;
        pthread_mutex_lock(&globalMutex);
        totalclientConnections+=1;
        if(totalclientConnections>=currentWorkerThreadCount*serverConfig->clientsPerThread)
        {
            DEBUG(("server need more Worker Threads...\n"));
            currentWorkerThreadCount+=serverConfig->threadPoolGrowth;
            growThreadPool(currentWorkerThreadCount-serverConfig->threadPoolGrowth);
        }
        pthread_mutex_unlock(&globalMutex);
        DEBUG(("finding next worker thread which is not full...\n"));
        while(1){
            pthread_mutex_lock(&turn->mutex);
            if(turn->noOfClient<serverConfig->clientsPerThread){
                pthread_mutex_unlock(&turn->mutex);
                break;
            }
            else{
                pthread_mutex_unlock(&turn->mutex); 
            }
            turn=turn->next;
               
        }
        DEBUG(("found the next worker thread which is not full: %d...\n", turn->id));
        pthread_mutex_lock(&turn->mutex);
        if(epoll_ctl(turn->epfd, EPOLL_CTL_ADD, ev.data.fd, &ev)<0)
            DEBUG(("error occurred when adding new client fd to epoll context...\n")); 
        DEBUG(("Added new client fd to %d worker thread...\n", turn->id));
        turn->noOfClient+=1;
        pthread_mutex_unlock(&turn->mutex);
        pthread_cond_signal(&turn->cv);
        turn=turn->next;
        
    }
    return 0;
}
void userSignalHandler(int sig){
    DEBUG(("thread sig action...\n"));
    return ;
}

void* workerThread(void* node){
    ThreadNode * threadNode= (ThreadNode *)node;
    FILE* filePointerWRITE;
    char * fileName=(char*)malloc(255*sizeof(char));
    sprintf(fileName,"CacheDEBUG%d.txt",threadNode->id);
    filePointerWRITE = fopen(fileName, "w+");
   
    struct sigaction sact;
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = 0; 
    sact.sa_handler = userSignalHandler; 
    sigaction(SIGUSR1, &sact, NULL);
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    //pthread_sigmask(SIG_SETMASK, &sigset, NULL);
    char buffer[MESSAGESIZE+1];
    char *key =  (char * )malloc(sizeof(char)*(MESSAGE_KEY_SIZE+1));
    char *value =  (char * )malloc(sizeof(char)*(MESSAGE_VALUE_SIZE+1));
    /*pthread_mutex_lock(&mutex[rank]);
    epfd[rank][1] =  epoll_create(clientPerWorkerThread);
    pthread_mutex_unlock(&mutex[rank]);*/
    threadNode->event = (struct epoll_event *)malloc(clientPerWorkerThread * sizeof(struct epoll_event ));
    while(1){
        DEBUG(("%d worker thread waiting for first connection...\n", threadNode->id));
        pthread_mutex_lock(&threadNode->mutex);
        //DEBUG(("%d worker thread lock acquired... no of clients: %d\n", threadNode->id, threadNode->noOfClient));
        while(threadNode->noOfClient==0)
            pthread_cond_wait(&threadNode->cv, &threadNode->mutex);
        DEBUG(("%d worker thread sleep ended...\n", threadNode->id));
        pthread_mutex_unlock(&threadNode->mutex);
        if(threadNode->noOfClient==-1)
            pthread_exit(0);
        DEBUG(("first connection arrived for worker thread %d...\n", threadNode->id));
        int eventCount;
        int bytesRead;
        while(1){   
            DEBUG(("server %d waiting for events...\n", threadNode->id));
            eventCount= epoll_pwait(threadNode->epfd, threadNode->event, clientPerWorkerThread, -1, &sigset);
            DEBUG(("server %d epoll wait returned...\n", threadNode->id));
            if(eventCount==-1 && EINTR == errno)
            {
                DEBUG(("epoll wait interrupted due to pthread kill signal...\n"));
                pthread_exit(0);
            }
            DEBUG(("server %d events received...\n", threadNode->id));
            for(int i=0; i<eventCount; i++) {
                if(threadNode->event[i].events & EPOLLIN){
                    DEBUG(("epoll ready to read...\n"));
                }
                if(threadNode->event[i].events & EPOLLOUT){
                    DEBUG(("epoll ready to write...\n"));
                }
                
                memset(buffer,0,MESSAGESIZE);
                bytesRead=recv(threadNode->event[i].data.fd, buffer, MESSAGESIZE, 0);
                char *key1=buffer;
                char *value1=&buffer[strlen(buffer)+1];
                DEBUG(("key1: %s value1: %s\n", key1, value1));
                DEBUG2((filePointerWRITE,"key1: %s value1: %s\n", key1, value1));
                buffer[MESSAGESIZE]='\0';
                if(!bytesRead)
                {
                    pthread_mutex_lock(&threadNode->mutex);
                    int s=epoll_ctl(threadNode->epfd, EPOLL_CTL_DEL, threadNode->event[i].data.fd, &threadNode->event[i]); 
                    if(!s)
                        threadNode->noOfClient-=1;
                    pthread_mutex_unlock(&threadNode->mutex);
                    pthread_mutex_lock(&globalMutex);
                    if(!s)
                        totalclientConnections-=1;
                    pthread_mutex_unlock(&globalMutex);
                    DEBUG(("one client closed connection from %d worker thread...\n", threadNode->id));

                }
                else
                {
                    int status=buffer[0]-'0';
                    
                    memcpy(key, &buffer[1], MESSAGE_KEY_SIZE);
                    memcpy(value, &buffer[MESSAGE_KEY_SIZE+1], MESSAGE_VALUE_SIZE);
                    DEBUG(("key: %s, value: %s\n", key, value));
                    /*for (int i=0,c=1;i<MESSAGE_KEY_SIZE;c++,i++){
                        key[i]=buffer[c];
                    }
                    
                    for (int i=0,c=MESSAGE_KEY_SIZE+1;i<MESSAGE_VALUE_SIZE;c++,i++){
                        value[i]=buffer[c];
                    }*/
                    key[MESSAGE_KEY_SIZE]='\0';
                    value[MESSAGE_VALUE_SIZE]='\0';

                    if(status==1){
                        DEBUG(("\nGET request to server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"\nGET request to server %d...\n", threadNode->id));
                        int getStatus=0;
                        KVNode *kvnode =  kvc_get(key,&getStatus, filePointerWRITE);
                        if(kvnode!=NULL){
                            sendResponse(threadNode->event[i].data.fd,200,kvnode->value);                            
                        }
                        else{
                            if(getStatus==FAILURE)
                                sendResponse(threadNode->event[i].data.fd,240,"ERROR!Some error occured while fetching response from KVStore.");
                            else
                                sendResponse(threadNode->event[i].data.fd,240,"Key not found while fetching response from KVStore.");
                            
                        }
                        kvc_ack_get(kvnode, filePointerWRITE);

                        DEBUG(("GET request Submitted to client for server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"GET request submitted to client for server %d...\n", threadNode->id));
                        
                    }
                    else  if(status==2){
                        DEBUG(("PUT request to server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"PUT request to server %d...\n", threadNode->id));
                         KVNode *kvnode =  kvc_put(key, value, filePointerWRITE);
                         if(kvnode==NULL){
                            sendResponse(threadNode->event[i].data.fd,240,"ERROR!Some error occured while puting key value into KVStore.");
                        }
                        else{
                            sendResponse(threadNode->event[i].data.fd,200,"success.");
                        }
                        kvc_ack_put(kvnode,filePointerWRITE);
                        DEBUG(("PUT request completed to client for server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"PUT request completed to client for server %d...\n", threadNode->id));

                    }
                    else  if(status==3){
                        DEBUG(("DEL request to server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"DEL request to server %d...\n", threadNode->id));
                        int getStatus = kvc_delete(key, filePointerWRITE);
                        if(getStatus==FAILURE){
                            sendResponse(threadNode->event[i].data.fd,240,"ERROR!Some error occured while deleting key from KVStore.");
                        }
                        if(getStatus==KEY_DOES_NOT_EXIST){
                            sendResponse(threadNode->event[i].data.fd,240,"deleting key does not exist in KVStore.");
                        }
                        else{
                            sendResponse(threadNode->event[i].data.fd,200,"success.");
                        }
                        DEBUG(("DEL request completed to client for server %d...\n", threadNode->id));
                        DEBUG2((filePointerWRITE,"DEL request completed to client for server %d...\n", threadNode->id));

                    }
                    else{
                        sendResponse(threadNode->event[i].data.fd,240,"Invalid status code from client.");
                    }
                    DEBUG(("response send back to client with fd %d...\n", threadNode->event[i].data.fd));
                }
            }
            pthread_mutex_lock(&threadNode->mutex);
            if(threadNode->noOfClient==0)
            {
                 pthread_mutex_unlock(&threadNode->mutex);
                 break;
            }
            else{
                 pthread_mutex_unlock(&threadNode->mutex);
            }
            
        }
    }
    return NULL;
}

void growThreadPool(int oldCount){
    DEBUG(("old worker threads count: %d...\n", oldCount));
    DEBUG(("growing worker threads...\n"));
    ThreadNode* curr=tail;
    for (unsigned long  i=oldCount; i<currentWorkerThreadCount; i++) {
        int *id=(int *)malloc(sizeof(int));
        *id=i;
        ThreadNode* threadNode=getThreadNode(id);
        tail=threadNode;
        curr->next=threadNode;
        tail->next=head;
        curr=threadNode;
    }
    DEBUG(("worker threads count increased successfully...\n"));
    
}


void sendResponse(int fd, unsigned char returnStatus, char * message){
    unsigned char returnMessage[MESSAGESIZE+1];
    returnMessage[MESSAGESIZE]='\0';
    returnMessage[0]=returnStatus;
    int i=0;
    for(i=0;message[i]!='\0';i++){
        returnMessage[1+i]=message[i];
    }
    returnMessage[1+i]=message[i];
    DEBUG(("Response: %s\n", returnMessage));
    if((send(fd, returnMessage, strlen((char*)returnMessage), MSG_NOSIGNAL)<0))
        DEBUG(("error while writing back. seems client has alredy closed connection...\n"));
    else
        DEBUG(("no error while writing back...\n"));

}

ThreadNode * getThreadNode(int* id){
    ThreadNode* threadNode=(ThreadNode *)malloc(sizeof(ThreadNode));
    threadNode->next=NULL;
    threadNode->noOfClient=0;
    threadNode->id=*id;
    free(id);
    pthread_mutex_init(&threadNode->mutex, NULL);
    pthread_cond_init(&threadNode->cv, NULL);
    threadNode->epfd=epoll_create(clientPerWorkerThread);
    pthread_create(&threadNode->threadHandle,NULL, workerThread, (void *)threadNode);
    return threadNode;
    
}