Team Name: Team Rocket
Roll no. : 203050015, Ashish Aggarwal
Roll no. : 203059009, Ajaykumar Shyamsunder
Roll no. : 203050039, Keshav Agarwal


Compilation steps for server:
1. run make command to compile the server files.
2. run ./server with execute permissions to run the server.
3. change server.cnf to run server on default port and change the initial no of threads like 
    configurations.

Default server configurations:
HOST : localhost
LISTENING_PORT : 25600
THREAD_POOL_SIZE_INITIAL : 10
CLIENTS_PER_THREAD : 2
CACHE_REPLACEMENT : LRU
THREAD_POOL_GROWTH : 10
CACHE_SIZE : 32
NO_OF_FILES : 50
FILES_GROW : 10

Compilation steps for client:
1. runt the command: g++ Client.cpp -o client
2. execute ./client with execute permissions to run the server.
3. Requests are in the input{client_id}.csv in the format {opcode,key,value, } 
    eg: 2,ajay,GoodMorning, means put request for key: ajay and value: GoodMorning
4. Responses for different clients are in file output{client_id}.txt
5. Increase PROCESS_COUNT macro in client.h file to add your own input file and response will be
    generated in corresponding output file.
6. For more information please refer inputFIleDec.txt.


Report.pdf contains the analysis of the performance and design details.
