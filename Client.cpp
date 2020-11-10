
#include "Client.h"
#include "FileOperations.cpp"
#include <string.h>

int main(int argc, char** argv)
{
	int s,i,status,parent;
    int client;
	char buffer[MESSAGESIZE];
	char readyMessage[MESSAGESIZE];
	char resp[1000];
	char *code;
	char *key;
	char *value;
	FILE* filePointerREAD;
	FILE* filePointerWRITE;


	char const *inputfile,*outputfile;

	char const *in_const = "input";
	char const *out_const = "output";
	char const *csv = ".csv";
	char const *txt = ".txt";
	char in_file_path[100];
	char out_file_path[100];


	ServerConfiguration* serverConfig =  parseServerConfig();


	for(i=0;i<PROCESS_COUNT;i++)
	{
		parent = fork();
		if(parent == 0)
		{
			sprintf(in_file_path,"%s%d%s",in_const,i+1,csv);  //Assumed file input*.csv  where * starts from 1
			inputfile = in_file_path;

			sprintf(out_file_path,"%s%d%s",out_const,i+1,txt);
			outputfile = out_file_path;
			break;
		}
	}


	if(parent == 0) // Enter if you are child
	{
		client = socket(AF_INET, SOCK_STREAM, 0);

		struct addrinfo hints, *result;
		
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		s = getaddrinfo(NULL, serverConfig->port, &hints, &result);
		if (s != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			exit(1);
		}

		connect(client, result->ai_addr, result->ai_addrlen);

		filePointerREAD = fopen(inputfile, "r"); 
		filePointerWRITE = fopen(outputfile, "w+");

		fprintf(filePointerWRITE,"Request are in following Format\n");
		fprintf(filePointerWRITE,"RequestCode(int),Key(string),Message(string)*_optional\n");
		setvbuf (stdout, NULL, _IONBF, 0);


		while(fgets(buffer, 1000, filePointerREAD))
		{	
			fprintf(filePointerWRITE,"\n\n%s",buffer);
			//buffer[strlen(buffer)-1]='\0';
			code = strtok(buffer,",");
			key = strtok(NULL,",");
			value = strtok(NULL,",");
			if(value!=NULL && value[0] == '\n')
			{
				value = NULL;
			}

			fprintf(filePointerWRITE,"Before encode : code:(%d)  key:(%s) value: (%s)\n",atoi(code),key, value);
			encodeMessage(code,key,value,readyMessage);
			fprintf(filePointerWRITE,"After encode : (%s) (%s) (%s)\n",readyMessage, readyMessage+1, readyMessage+257 );
			write(client, readyMessage, MESSAGESIZE);
			int len = read(client, resp, 999);
			resp[len] = '\0';
			decodeMessage(&filePointerWRITE,(unsigned char*)resp);
		}

	}
	else
	{
		while(waitpid(-1,&status,0)>0);
	}
    
	return 0;
}
