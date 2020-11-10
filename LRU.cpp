#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include "KVCache.h"


KVNode * lru_getEvictionNode(PolicyList* list){
    KVNode *node;
    while(true)
    {
        if(list->tail->prev==list->head)
            return NULL;
        node=list->tail;
        while(node->prev != list->head)
        {
            node=node->prev;
            if(pthread_rwlock_trywrlock(&(node->rwlock)) ==0){
                return node;
            }
        }
    }
}

void lru_updateNode(PolicyList *list, KVNode *node)
{
    if(node->next == NULL){
        cep_addNode(list,node);
        pthread_mutex_unlock(&node->mutexPListUpdate);    
    }
}