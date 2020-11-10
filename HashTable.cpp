#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<unistd.h>
#include<pthread.h>
#include "KVCache.h"
//cpp headers
#include <string> 
#include <functional> 
#include <iostream> 
using namespace std; 
//cpp headers end

int keys=0;

int ht_init(HTNode *ht, int keyCount){

    int i=0;

    for(i=0;i<keyCount;i++)
    {
        if(pthread_rwlock_init(&(ht[i].rwlock),NULL) !=0)
            return -1;
        
        ht[i].head=NULL;
        ht[i].tail=NULL;
    }
    keys=keyCount;
    return 0;
}

unsigned int gethash( char *str)
{
    unsigned int hashValue = 0;
    std::hash<std::string> hashfunc;
    hashValue=hashfunc(str)%keys;
    return hashValue;
    return hashValue;
}

KVNode * getNodeinList(KVNode *head,char *key)
{    
    KVNode *node=head;
    while(node)
    {        
        if(strcmp(key,node->key) == 0)
            return node;
        node=node->htNext;
    }    
    return NULL;
}

KVNode * ht_put(HTNode *ht,int index, KVNode *node)
{
    //DEBUG(("ht_put started index:%d \n",index));

    node->htNext=NULL;
    KVNode *hcn=NULL;
    HTNode *htn=&(ht[index]);
   
    pthread_rwlock_wrlock(&(htn->rwlock));
    if(htn->head == NULL){
        htn->head=node;       
        htn->tail=node;
        node->htNext=NULL;
    }    
    else
    {
        char *key = node->key;
        hcn = getNodeinList( htn->head, key);
        
        if(hcn){
            pthread_rwlock_wrlock(&(hcn->rwlock));  
            node=hcn;
        }
        else{
            htn->tail->htNext = node;
            htn->tail=node;
            node->htNext=NULL;
        }
    }
   
    pthread_rwlock_unlock(&(htn->rwlock));
    
    //DEBUG(("ht_put ended index:%d \n",index));

    return node;
}

int ht_get(HTNode *ht, char *key, KVNode **node, bool isWrite)
{ 
   DEBUG(("ht_get started key: %s\n",key));

    unsigned index= gethash(key);
   
    HTNode *htn = &(ht[index]);
   
    pthread_rwlock_rdlock(&(htn->rwlock));
    KVNode *kvn = getNodeinList( htn->head,key);
   
    if(kvn){
        if(isWrite)
            pthread_rwlock_wrlock(&(kvn->rwlock));
        else
            pthread_rwlock_rdlock(&(kvn->rwlock));
    }
    pthread_rwlock_unlock(&(htn->rwlock));
    *node=kvn;

    DEBUG(("ht_get ended key: %s node:%lu\n",key,(unsigned long)kvn));
    
    return index;    
}

int ht_deleteNode(HTNode *ht, KVNode *delNode, HTNode **htReturn){
    //DEBUG(("ht_deleteNode started node:%lu\n",(unsigned long)delNode));
    int index = gethash(delNode->key);
    HTNode *htn = &(ht[index]);
    pthread_rwlock_wrlock(&(htn->rwlock));
    KVNode *node = htn->head;
    if(htn->head==delNode && htn->tail == delNode){
        
        htn->head=NULL;
        htn->tail=NULL;
    }
    else if(htn->head == delNode)
    {
        htn->head=delNode->htNext;
    }
    else
    {
        while(node->htNext!=delNode){
            node=node->htNext;
        }
        node->htNext=delNode->htNext;
        if(delNode->htNext==NULL)
            htn->tail=node;
    }
    *htReturn=htn;
    delNode->htNext=NULL;

    return 0;
}

int ht_deleteKey(HTNode *ht, char *key, HTNode **htnode, KVNode **kvnode){
    
    DEBUG(("ht_deleteKey started key:%s\n",key));

    unsigned index= gethash(key);
    HTNode *htn = &(ht[index]);
    pthread_rwlock_wrlock(&(htn->rwlock));
    KVNode *node = htn->head;
    KVNode *delNode = NULL;

    if(node == NULL)
        delNode=NULL;
    else if(strcmp(node->key,key)==0){
        htn->head=node->htNext;
        if(node->htNext == NULL)
            htn->tail=NULL;
        delNode=node;
    }
    else
    {
        KVNode *next = node->htNext;
        while(next){
            if(strcmp(next->key,key)==0){
                delNode=next;
                node->htNext=next->htNext;
                if(delNode->htNext==NULL)
                    htn->tail=node;
                break;
            }
            node=next;
            next=node->htNext;          
        }       
    }
    if(delNode){
        delNode->htNext=NULL;
        pthread_rwlock_wrlock(&(delNode->rwlock));
    }
    *htnode=htn;
    *kvnode=delNode;

    DEBUG(("ht_deleteKey ended key:%s\n",key));
    return index;
}

