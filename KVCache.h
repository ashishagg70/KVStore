#ifndef KVCache
#define KVCache

#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<pthread.h>
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

#define KEY_SIZE 257
#define VALUE_SIZE 257

#define LRU "LRU"
#define LFU "LFU"

typedef struct kvnode 
{
	char key[KEY_SIZE];
	char value[VALUE_SIZE];
	bool isDirty;
	int frequency;
	struct kvnode *prev;
	struct kvnode *next;
	struct kvnode *htNext;
	pthread_rwlock_t rwlock;
	pthread_mutex_t mutexPListLinks;
	pthread_mutex_t mutexPListUpdate;

} KVNode;

typedef struct htnode
{
	pthread_rwlock_t rwlock;
	KVNode *head;
	KVNode *tail;
}HTNode;

typedef struct policylist{
	KVNode *head;
	KVNode *tail;
	int policy;
}PolicyList;

typedef struct cachefreenodes{
	KVNode *head;
	KVNode *tail;
	pthread_mutex_t mutexHead;
	pthread_mutex_t mutexTail;
}CFreeNodes;


int ht_init(HTNode *ht, int keyCount);
int ht_get(HTNode *ht, char *key, KVNode **node, bool isWrite);
KVNode * ht_put(HTNode *ht, int index, KVNode *node);
int ht_deleteNode(HTNode *ht, KVNode *delNode, HTNode **htReturn);
int ht_deleteKey(HTNode *ht, char *key, HTNode **htnode, KVNode **kvnode);

int frl_init(CFreeNodes *freeNodes, int keyCount);
int frl_get(CFreeNodes *freeNodes, KVNode **node);
int frl_put(CFreeNodes *freeNodes, KVNode * node);


int kvc_init(int keyCount,char *evictionPolicy);
KVNode * kvc_get(char * key, int *status,FILE* fpw);
KVNode * kvc_put(char *key, char * value,FILE* fpw);
int kvc_delete(char *key,FILE* fpw);
int kvc_ack_get(KVNode *node,FILE* fpw);
int kvc_ack_put(KVNode *node,FILE* fpw);
int kvc_free();

int cep_init(PolicyList *list, KVNode *head, KVNode *tail, char * evictionPolicy);
KVNode * cep_evict(PolicyList* list);
int cep_deleteNode(PolicyList* list, KVNode *node);
int cep_addNode(PolicyList *list, KVNode *node);
void cep_updateNode(PolicyList *list, KVNode *node);

KVNode * lru_getEvictionNode(PolicyList* list);
void lru_updateNode(PolicyList *list, KVNode *node);

KVNode * lfu_getEvictionNode(PolicyList* list);
void lfu_updateNode(PolicyList *list, KVNode *node);

#endif