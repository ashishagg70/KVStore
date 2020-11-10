#include "FileOperations.h"
#include "ServerEpoll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
//cpp headers
#include <string> 
#include <functional> 
#include <iostream> 
using namespace std; 
//cpp headers end
int noOfFiles=0;
pthread_mutex_t *mutex;
ServerConfiguration * parseServerConfig(){
    FILE *fptr;
    char filePath[50]="server.cnf";
    ServerConfiguration * config = (ServerConfiguration *)malloc(sizeof(ServerConfiguration)); 
    char readBuffer[CONFIG_KEYSIZE+CONFIG_VALUE_SIZE];
    if ((fptr = fopen(filePath, "r")) == NULL) {
        printf("Error while opening server configuration file");
        exit(1);
    }

    while(fgets(readBuffer, 200, fptr)) {
    //while(getline(readBuffer, &len, fptr)!= -1){
        char key[CONFIG_KEYSIZE];
        char value[CONFIG_VALUE_SIZE];
        char sep[2];
        //printf("%s", readBuffer);
        sscanf(readBuffer,"%s%s%s", key, sep, value);
        //printf("%s and %s\n", key, value);
        if(strcmp(key,"HOST")==0){
            config->host = (char *)malloc(CONFIG_VALUE_SIZE);
            strcpy(config->host, value);
        }
        else if(strcmp(key,"LISTENING_PORT")==0){
            char * port= (char *)malloc(CONFIG_VALUE_SIZE);
            strcpy(port, value);
            config->port=port;
            //config->port= atoi(value);
        }
        else if(strcmp(key,"THREAD_POOL_SIZE_INITIAL")==0){
            config->threadPoolCount=atoi(value);
        }
        else if(strcmp(key,"CLIENTS_PER_THREAD")==0){
            config->clientsPerThread=atoi(value);
        }
        else if(strcmp(key,"CACHE_SIZE")==0){
            config->cacheSize=atoi(value);
        }
        else if(strcmp(key,"THREAD_POOL_GROWTH")==0){
            config->threadPoolGrowth=atol(value);
        }
        else if(strcmp(key,"CACHE_REPLACEMENT")==0){
            config->cacheReplacement = (char *)malloc(CONFIG_VALUE_SIZE);
            strcpy(config->cacheReplacement, value);
        }
        else if(strcmp(key,"NO_OF_FILES")==0){
            config->noOfFiles=atol(value);
        }
        else if(strcmp(key,"FILES_GROW")==0){
            config->fileGrow=atol(value);
        }
    }
    fclose(fptr);
    return config;
}

void initFileOperations(int keyCount){
    noOfFiles=keyCount;
    mutex = (pthread_mutex_t *)malloc(keyCount*sizeof(pthread_mutex_t));
	for(int i=0;i<keyCount;i++){
		pthread_mutex_init(&mutex[i], NULL);
	}
}

unsigned int customHash( char *str)
{
    unsigned int hashValue = 0;
    std::hash<std::string> hashfunc;
    hashValue=hashfunc(str)%noOfFiles;
    return hashValue;
}

int readFileKey(char *key, char * retValue){
    unsigned int fileIndex = customHash(key);
    char * fileName=(char*)malloc(255*sizeof(char));
    char readBuffer[MESSAGESIZE];
    sprintf(fileName,"KVDB/%d.dat",fileIndex);
    FILE *fptr;
    pthread_mutex_lock(&mutex[fileIndex]);
    if((fptr = fopen(fileName,"r+"))==NULL){
        pthread_mutex_unlock(&mutex[fileIndex]);
        return KEY_DOES_NOT_EXIST;
    }
    //check if key already pesent in file.
    fseek(fptr, 0, SEEK_SET);
    while(fread(readBuffer, MESSAGESIZE, 1, fptr)!=0){
        char keyRead[MESSAGE_KEY_SIZE+1];
        keyRead[MESSAGE_KEY_SIZE]='\0';
        memcpy(keyRead, readBuffer+1, MESSAGE_KEY_SIZE);
        //key already present updated the value
        if(readBuffer[0]=='1' && strcmp(keyRead, key)==0){
            memcpy(retValue, readBuffer+MESSAGE_KEY_SIZE+1, MESSAGE_VALUE_SIZE);
            retValue[MESSAGE_VALUE_SIZE]='\0';
            fclose(fptr);
            pthread_mutex_unlock(&mutex[fileIndex]);
            return SUCCESS;
        }
    }
    fclose(fptr);
    pthread_mutex_unlock(&mutex[fileIndex]);
    return KEY_DOES_NOT_EXIST;
}

int writeFileKey(char *key, char * value){
    unsigned int fileIndex = customHash(key);
    char * fileName=(char*)malloc(255*sizeof(char));
    sprintf(fileName,"KVDB/%d.dat",fileIndex);
    char writeBuffer[MESSAGESIZE];
    writeBuffer[0]='1';
    memcpy(writeBuffer+1, key, MESSAGE_KEY_SIZE);
    memcpy(writeBuffer+1+MESSAGE_KEY_SIZE, value, MESSAGE_VALUE_SIZE);
    DEBUG(("write Buffer to file, status: %c keyInitial: %c, valueInitial %c \n", writeBuffer[0], writeBuffer[1], writeBuffer[257]));
    char readBuffer[MESSAGESIZE];
    FILE *fptr;
    DEBUG(("filename to write: %s\n", fileName));
    pthread_mutex_lock(&mutex[fileIndex]);

    //File does not exist, create and directly write to file.
    if(!isFileExist(fileName)){
        if((fptr = fopen(fileName,"a+"))==NULL){
            pthread_mutex_unlock(&mutex[fileIndex]);
            DEBUG(("Write failure while opening file\n"));
            return FAILURE;
        }
        if(fwrite(writeBuffer, MESSAGESIZE, 1, fptr)==0){
            fclose(fptr);
            DEBUG(("Write failure while saving key to empty file\n"));
            pthread_mutex_unlock(&mutex[fileIndex]);
            return FAILURE;
        }
        fclose(fptr);
        pthread_mutex_unlock(&mutex[fileIndex]);
        return SUCCESS;

    }

    if((fptr = fopen(fileName,"r+"))==NULL){
        DEBUG(("Write failure while opening file\n"));
        pthread_mutex_unlock(&mutex[fileIndex]);
        return FAILURE;
    }

    //check if key already pesent in file.
    fseek(fptr, 0, SEEK_SET);
    while(fread(readBuffer, MESSAGESIZE, 1, fptr)!=0){
        char keyRead[MESSAGE_KEY_SIZE+1];
        keyRead[MESSAGE_KEY_SIZE]='\0';
        memcpy(keyRead, readBuffer+1, MESSAGE_KEY_SIZE);
        //key already present updated the value
        if(strcmp(keyRead, key)==0){
            DEBUG(("%s key found in FS already...so updating value...\n", key));
            readBuffer[0]='1';
            memcpy(readBuffer+MESSAGE_KEY_SIZE+1, value, MESSAGE_VALUE_SIZE);
            fseek(fptr,-MESSAGESIZE,SEEK_CUR);
            if(fwrite(readBuffer, MESSAGESIZE, 1, fptr)==0){
                DEBUG(("Write failure while updating key to file\n"));
                fclose(fptr);
                pthread_mutex_unlock(&mutex[fileIndex]);
                return FAILURE;
            }
            fclose(fptr);
            pthread_mutex_unlock(&mutex[fileIndex]);
            return SUCCESS;
        }
    }

    //key doesn't present append new key to file.
    if(fwrite(writeBuffer, MESSAGESIZE, 1, fptr)==0){
        DEBUG(("Write failure while inserting key to file\n"));
        fclose(fptr);
        pthread_mutex_unlock(&mutex[fileIndex]);
        return FAILURE;
    }
    fclose(fptr);
    pthread_mutex_unlock(&mutex[fileIndex]);
    return SUCCESS;

}
int deleteFileKey(char *key){
    unsigned int fileIndex = customHash(key);
    char * fileName=(char*)malloc(255*sizeof(char));
    char readBuffer[MESSAGESIZE+1];
    readBuffer[MESSAGESIZE]='\0';
    sprintf(fileName,"KVDB/%d.dat",fileIndex);
    FILE *fptr;
    pthread_mutex_lock(&mutex[fileIndex]);
    if((fptr = fopen(fileName,"r+"))==NULL){
        pthread_mutex_unlock(&mutex[fileIndex]);
        DEBUG(("Delete failure while opening file\n"));
        return KEY_DOES_NOT_EXIST;
    }
     //check if key already pesent in file.
    fseek(fptr, 0, SEEK_SET);
    while(fread(readBuffer, MESSAGESIZE, 1, fptr)!=0){
        char keyRead[MESSAGE_KEY_SIZE+1];
        keyRead[MESSAGE_KEY_SIZE]='\0';
        memcpy(keyRead, readBuffer+1, MESSAGE_KEY_SIZE);
        //key already present updated the value
        if(readBuffer[0]=='1' && strcmp(keyRead, key)==0){
            DEBUG(("%s key found in FS to delete...\n", key));
            readBuffer[0]='0';
            fseek(fptr,-MESSAGESIZE,SEEK_CUR);
            if(fwrite(readBuffer, 1, 1, fptr)==0){
                fclose(fptr);
                pthread_mutex_unlock(&mutex[fileIndex]);
                DEBUG(("Delete failure after key found...\n"));
                return FAILURE;
            }
            fclose(fptr);
            pthread_mutex_unlock(&mutex[fileIndex]);
            return SUCCESS;
        }
    }
    fclose(fptr);
    pthread_mutex_unlock(&mutex[fileIndex]);
    DEBUG(("Delete failure key not found...\n"));
    return KEY_DOES_NOT_EXIST;
}

int isFileExist(char * fileName){
    if( access( fileName, F_OK ) != -1 ) {
        return 1;
    } else {
    return 0;
    }
}