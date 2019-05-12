#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#define KBYTE 1024
#define FILEEXT_MAX_SIZE 8
#define USER_MAX_NAME 20
#define PAYLOAD_SIZE 1024
#define FILE_MAX 20
#define FILENAME_MAX_SIZE 20

#define DELETE 6
#define DOWNLOADALL 5
#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0

// server
#define PORT 4000

//Tipos de pacote
#define DATA 1
#define CMD  2
#define RESP 3

struct client_request
{
  char file[200];
  int command;
};

typedef struct packet{
    uint16_t type;               //Tipo do pacote(p.ex. DATA| CMD)
    uint16_t seqn;               //Número de sequência
    uint32_t total_size;         //Número total de fragmentos
    uint16_t length;             //Comprimento do payload 
    char _payload[PAYLOAD_SIZE]; //Dados do pacote
    int payloadCommand;
} packet;

struct file_info {
  char name[FILENAME_MAX_SIZE];
  char extension[FILEEXT_MAX_SIZE];
  char last_modified[20];
  time_t lst_modified;
  int size;
  pthread_mutex_t file_mutex;
};
