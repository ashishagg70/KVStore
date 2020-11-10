#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include <limits.h>
#include "KVCache.h"

KVNode * lfu_getEvictionNode(PolicyList* list){
    KVNode *node;
    KVNode *minFrequecy;
    int freq;
    
    while(true)
    {
        if(list->tail->prev==list->head)
            return NULL;
        minFrequecy=NULL;
        freq=INT_MAX;
        node=list->tail;

        while(node->prev != list->head)
        {
            node=node->prev;
            if(node->frequency < freq)
            {
                freq=node->frequency;
                minFrequecy=node;
            }
            if(freq==1)
                break;

        }
        
        if(minFrequecy){
            pthread_rwlock_wrlock(&(minFrequecy->rwlock));
            break;
        }
    }
    return minFrequecy;
}


void lfu_updateNode(PolicyList *list, KVNode *node){
    DEBUG(("lfu_updateNode start policy:%d\n",list->policy));

    if(node->next != NULL){
        pthread_mutex_lock(&(node->mutexPListUpdate));
        node->frequency++;
        pthread_mutex_unlock(&(node->mutexPListUpdate));
    }
    if(node->next == NULL)
    {
        pthread_mutex_lock(&(node->mutexPListUpdate));
        cep_addNode(list,node);
        pthread_mutex_unlock(&node->mutexPListUpdate);
    }
    DEBUG(("lfu_updateNode end\n"));

}