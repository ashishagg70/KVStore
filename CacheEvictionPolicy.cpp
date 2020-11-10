#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include <string.h>
#include "KVCache.h"


int cep_init(PolicyList *list, KVNode *head, KVNode *tail, char * evictionPolicy){
    list->head=head;
    list->tail=tail;
    head->next=tail;
    tail->prev=head;

    if(strcmp(evictionPolicy,LRU)==0)
        list->policy=0;
    else
        list->policy=1;
    
    return 0;
}


int cep_deleteNode(PolicyList* list, KVNode *node) {
    
    DEBUG(("cep_deleteNode started \n"));
    if(node->next == NULL)
        return 0;
    
    KVNode *prev,*next;
    while(true){
        prev=node->prev;
        pthread_mutex_lock(&(prev->mutexPListLinks));
        if(node->prev == prev)
            break;
        pthread_mutex_unlock(&(prev->mutexPListLinks));   
    }
    pthread_mutex_unlock(&(node->mutexPListLinks));
    while(true){
        next=node->next;
        pthread_mutex_lock(&(next->mutexPListLinks));
        if(node->next == next)
            break;
        pthread_mutex_unlock(&(next->mutexPListLinks));   
    }
    prev->next=next;
    next->prev=prev;

    pthread_mutex_unlock(&(next->mutexPListLinks));
    pthread_mutex_unlock(&(prev->mutexPListLinks));   

    node->prev=NULL;
    node->next=NULL;

    pthread_mutex_unlock(&(node->mutexPListLinks));
    DEBUG(("cep_deleteNode ended \n"));
    
    return 0;
}  

KVNode * cep_evict(PolicyList* list){  
    DEBUG(("cep_evict started \n"));
    KVNode *node;
    if(list->policy==0)
        node=lru_getEvictionNode(list);
    else
        node=lfu_getEvictionNode(list);

    if(node){
        cep_deleteNode(list,node);
    }
    DEBUG(("cep_evict ended node:%lu \n",(unsigned long)node ));
    
    return node; 
}

int cep_addNode(PolicyList *list, KVNode *node)
{
    DEBUG(("cep_addNode started \n"));
     KVNode *head,*next;
     head = list->head;
    pthread_mutex_lock(&(head->mutexPListLinks));
    pthread_mutex_lock(&(node->mutexPListLinks));
    while(true){
        next=head->next;
        pthread_mutex_lock(&(next->mutexPListLinks));
        if(head->next == next)
            break;
        pthread_mutex_unlock(&(next->mutexPListLinks));   
    }
    node->next=next;
    next->prev=node;
    head->next=node;
    node->prev=head; 
    pthread_mutex_unlock(&(next->mutexPListLinks));
    pthread_mutex_unlock(&(node->mutexPListLinks));
    pthread_mutex_unlock(&(head->mutexPListLinks));
    DEBUG(("cep_addNode ended \n"));
    
    return 0;
    
}

void cep_updateNode(PolicyList *list, KVNode *node)
{
    if(list->policy == 0)
        lru_updateNode(list,node);
    else
        lfu_updateNode(list,node);

}