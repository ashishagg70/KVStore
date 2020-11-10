CFLAGS?=-g -w -Wall -Wno-unused-value
OBJECT=ServerEpoll.o FileOperations.o CacheEvictionPolicy.o LRU.o LFU.o KVCache.o FreeList.o HashTable.o
CC=g++
output: $(OBJECT)
	$(CC) $(CFLAGS) $(OBJECT) -o server -lpthread 

FileOperations.o: FileOperations.cpp FileOperations.h
	$(CC) -w -c FileOperations.cpp

ServerEpoll.o: ServerEpoll.cpp ServerEpoll.h KVCache.h
	$(CC) -w -c ServerEpoll.cpp

KVCache.o: KVCache.cpp  KVCache.h
	$(CC) -w -c KVCache.cpp

CacheEvictionPolicy.o: CacheEvictionPolicy.cpp KVCache.h
	$(CC) -w -c CacheEvictionPolicy.cpp

LRU.o: LRU.cpp KVCache.h
	$(CC) -w -c LRU.cpp

LFU.o: LFU.cpp KVCache.h
	$(CC) -w -c LFU.cpp

HashTable.o: HashTable.cpp KVCache.h
	$(CC) -w -c HashTable.cpp

FreeList.o: FreeList.cpp KVCache.h
	gcc -w -c FreeList.cpp

clean:
	rm *.o server *.h.gch
target: dependencies
	action



