#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<unistd.h>
#include<pthread.h>
#include "KVCache.h"
#include "FileOperations.h"
PolicyList list;
HTNode *ht;
CFreeNodes freeNodes;

pthread_mutex_t mlock;


int kvc_init(int keyCount,char *evictionPolicy)
{
    int i=0;
    if(frl_init(&freeNodes,keyCount+2) != 0)
        return -1;

    //Hash Table initialization
    ht=(HTNode *) malloc(sizeof(HTNode)* keyCount);
    if(ht_init(ht,keyCount) !=0)
        return -1;

    //KV lru list initialization and KVNodes

    KVNode *head, *tail;
    frl_get(&freeNodes,&head);
    frl_get(&freeNodes,&tail);
    pthread_rwlock_unlock(&(head->rwlock));
    pthread_rwlock_unlock(&(tail->rwlock));
    if(cep_init(&list,head,tail,evictionPolicy) != 0)
        return -1;
    
    if(pthread_mutex_init(&mlock, NULL) != 0)
        return -1;
    return 0;
}

int getDataFromFile(char * key, char * value){
    return readFileKey(key, value);
}

int  saveDatainFile(KVNode *node){
    return writeFileKey(node->key, node->value);
}
int deletefromfile(char *key){
    return deleteFileKey(key);
}

KVNode * evictNode()
{
    KVNode *node;
    node=cep_evict(&list);
    HTNode *htn;
    ht_deleteNode(ht,node,&htn);
    if(node->isDirty)
        saveDatainFile(node);
    pthread_rwlock_unlock(&( htn->rwlock));
    return node;    
}

KVNode * kvc_get(char * key, int *status,FILE* fpw)
{
    /*
    1. get node from hash table with read lock enabled
    2. return node with read lock enabled on node
    3. If node not present get node from free list
    4. write lock it
    5. add node to ht  
    6. Get kv from file and update data, release write lock and apply read lock
    7. return node with read lock enabled on node

    8. Server after replying with value, will call update lru with this node
    9. update lru will release read lock
    */

    pthread_mutex_lock(&mlock);
    DEBUG(("kvc_get started key: %s\n",key));
    //DEBUG2((fpw,"kvc_get started key: %s\n",key));
    KVNode *node;
    int hash =  ht_get(ht, key, &node, false);
    if(node){ 
        DEBUG(("kvc_get ended node found key: %s\n",key));
        if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
        *status=0;   
        return node;
    }
    char * value = (char *)malloc(VALUE_SIZE);
    *status=getDataFromFile(key,value);
    if(status == 0){
        if(frl_get(&freeNodes,&node) !=0)
        {
            node=evictNode();
        }
        strncpy(node->key,key,KEY_SIZE-1);
        node->key[KEY_SIZE-1]='\0';
        strncpy(node->value,value,VALUE_SIZE-1);
        node->value[VALUE_SIZE-1]='\0';
        KVNode *kvn=ht_put(ht, hash, node);
        if(kvn != node){
            frl_put(&freeNodes,node);
            node=kvn;
            if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
        }
        else
        {
            if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
        }
        
    }
   DEBUG(("kvc_get ended key:%s node:%lu\n",key,(unsigned long) node));
    return node;
}

KVNode * kvc_put(char *key, char * value,FILE* fpw)
{
    pthread_mutex_lock(&mlock);

    DEBUG(("kvc_put started key: %s value:%s\n",key,value));
    DEBUG2((fpw,"kvc_put started key: %s value:%s\n",key,value));
    KVNode *node;
    int hash =  ht_get(ht, key, &node, true);
   
    if(!node)
    {
        if(frl_get(&freeNodes,&node) !=0)
        {
           node=evictNode();
        }
        strncpy(node->key,key,KEY_SIZE-1);
        node->key[KEY_SIZE-1]='\0';
        KVNode *putNode=ht_put(ht, hash, node);

        if(putNode!=node){
            frl_put(&freeNodes,node);
            node=putNode;
            if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
        }
        else
        {
            if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
        }
        
    }
    else{
        if(list.policy == 0)
            {
                if(pthread_mutex_trylock(&(node->mutexPListUpdate)) == 0)
                    cep_deleteNode(&list,node);
            }
    }

    node->isDirty=true;
    strncpy(node->value,value,VALUE_SIZE-1);
    node->value[VALUE_SIZE-1]='\0';

    DEBUG(("kvc_put end key: %s value:%s node:%lu\n",key,value,(unsigned long)node));
    DEBUG2((fpw,"kvc_put end key: %s value:%s node:%lu\n",key,value,(unsigned long)node));

    return node;
}

int kvc_delete(char *key,FILE* fpw)
{
    pthread_mutex_lock(&mlock);

    DEBUG(("kvc_delete started key: %s\n",key));
    DEBUG2((fpw,"kvc_delete started key: %s\n",key));

    KVNode *node;
    HTNode *htn;
    int hash = ht_deleteKey(ht, key,&htn, &node);
    int status = deletefromfile(key);
    
    pthread_rwlock_unlock(&( htn->rwlock));
    if(node){
        pthread_mutex_lock(&(node->mutexPListUpdate));
        cep_deleteNode(&list,node);
        frl_put(&freeNodes, node);
        pthread_mutex_unlock(&(node->mutexPListUpdate));
        status=0;
    }
    DEBUG(("kvc_delete ended key: %s\n",key));
    DEBUG2((fpw,"kvc_delete ended key: %s\n",key));

    pthread_mutex_unlock(&mlock);

    return status;
}

int kvc_ack_get(KVNode *node,FILE* fpw)
{
    if(node){
        DEBUG(("kvc_ack_get key:%s value:%s\n",node->key, node->value));
        cep_updateNode(&list,node);
        pthread_rwlock_unlock(&( node->rwlock));
    }
   pthread_mutex_unlock(&mlock);
    return 0;
}

int kvc_ack_put(KVNode *node,FILE* fpw)
{
   if(node){
        DEBUG(("kvc_ack_put key:%s value:%s\n",node->key, node->value));
        cep_updateNode(&list,node);
        pthread_rwlock_unlock(&( node->rwlock));
    }
    pthread_mutex_unlock(&mlock);
    return 0;
}

int kvc_free(){
    KVNode *node;
    node=list.head;
    while(node)
    {
        if(node->isDirty)
            saveDatainFile(node);
        node=node->next;
    }
}
