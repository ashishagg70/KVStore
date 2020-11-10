#ifndef FileOperations
#define FileOperations
#include "ServerEpoll.h"
#define CONFIG_VALUE_SIZE 256
#define CONFIG_KEYSIZE 100
ServerConfiguration * parseServerConfig();
unsigned int customHash( char *str);
int readFileKey(char *key, char * retValue);
int writeFileKey(char *key, char * value);
int deleteFileKey(char *key);
void initFileOperations(int keyCount);
int isFileExist(char * fileName);
enum status{SUCCESS, FAILURE, KEY_DOES_NOT_EXIST}; 
#endif