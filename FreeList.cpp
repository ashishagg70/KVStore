#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include "KVCache.h"


int initializeFreeNodes(int keyCount, KVNode **head, KVNode **tail)
{
    int i=0;
      //creating all kv nodes and adding to free list
    KVNode *prev=NULL;
    for(i=0;i<keyCount;i++)
    {
        KVNode *node = (KVNode *) malloc(sizeof(KVNode));
        if(node==NULL)
            return -1;

        node->key[0]='\0';
        node->value[0]='\0';
        node->next=NULL;
        node->htNext=NULL;
        node->prev=prev;
        node->frequency=0;
        
        if(prev != NULL)
            prev->next=node;
        else
        {
            *head=node;
        }        

        node->isDirty=false;
        if(pthread_rwlock_init(&(node->rwlock),NULL) != 0)
            return -1;
        if(pthread_mutex_init(&(node->mutexPListLinks), NULL) != 0)
            return -1;
        if(pthread_mutex_init(&(node->mutexPListUpdate), NULL) != 0)
            return -1;
        prev=node;
    }
    *tail=prev;
    return 0;
    
}

int frl_init(CFreeNodes *freeNodes, int keyCount){
    if(initializeFreeNodes(keyCount, &(freeNodes->head),&(freeNodes->tail)))
        return -1;
    if(pthread_mutex_init(&(freeNodes->mutexHead), NULL) != 0)
        return -1;
    if(pthread_mutex_init(&(freeNodes->mutexTail), NULL) != 0)
        return -1;
    return 0;
}

int frl_get(CFreeNodes *freeNodes, KVNode ** node)
{
    DEBUG(("frl_get started \n"));

    pthread_mutex_lock(&(freeNodes->mutexTail));

    if(freeNodes->head == NULL){
        DEBUG(("frl_get ended free node null\n"));
        pthread_mutex_unlock(&(freeNodes->mutexTail));
        return -1;
    }
        
    KVNode *kvn = freeNodes->tail;
    freeNodes->tail = kvn->prev;
    if(kvn->prev == NULL)
        freeNodes->head=NULL;
    else{
        freeNodes->tail->next=NULL;
        kvn->prev=NULL;
    }
    pthread_rwlock_wrlock( &(kvn->rwlock));
    pthread_mutex_unlock(&(freeNodes->mutexTail));

    kvn->isDirty=false;
    kvn->next=NULL;
    kvn->frequency=1;
    
    *node=kvn;

    DEBUG(("frl_get ended \n"));

    return 0;
}

int frl_put(CFreeNodes *freeNodes, KVNode * node)
{
    DEBUG(("frl_put started \n"));

    node->prev=NULL;
    node->isDirty=false;
    node->htNext=NULL;
    node->key[0]='\0';
    node->value[0]='\0';
    node->frequency=0;

    pthread_mutex_lock(&(freeNodes->mutexHead));
    node->next=freeNodes->head;
    freeNodes->head=node;
    if(freeNodes->tail == NULL)
        freeNodes->tail=node;
    else
    {
        node->next->prev=node;
    }
    
    pthread_mutex_unlock(&(freeNodes->mutexHead));
    pthread_rwlock_unlock(&(node->rwlock));

    DEBUG(("frl_put ended \n"));

    return 0;
}
