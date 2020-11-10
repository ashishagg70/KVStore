#define MESSAGESIZE 513
#define MESSAGE_KEY_SIZE 256
#define MESSAGE_VALUE_SIZE 256
#define PROCESS_COUNT 6

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include<fcntl.h> 
#include<sys/wait.h>

void encodeMessage(char* code,char* key,char* value,char* readyMessage)
{
    char message[MESSAGESIZE];

    memset(message, '\0', MESSAGESIZE);

    strcpy(message, code);
    strcpy(message+1,key);

    //printf("%s",message);
    if(value != NULL && atoi(code)== 2)
    {
        strcpy(message+257,value);
    }
    memcpy(readyMessage,message, MESSAGESIZE);
}

void decodeMessage(FILE** fp,unsigned char* readyMessage)
{
    if((int)readyMessage[0]==200)
    {
        fprintf(*fp," ===> %d(Success) : %s",(int)readyMessage[0],readyMessage+1);
    }
    else
    {
        fprintf(*fp," ===> %d(Error) : %s",(int)readyMessage[0],readyMessage+1);
    }
    
}