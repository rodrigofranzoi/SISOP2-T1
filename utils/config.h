#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>


// server
#define PORT 4000

//Tipos de pacote
#define DATA 1
#define CMD  2
#define RESP 3

typedef struct packet{
    uint16_t type;               //Tipo do pacote(p.ex. DATA| CMD)
    uint16_t seqn;               //Número de sequência
    uint32_t total_size;         //Número total de fragmentos
    uint16_t length;             //Comprimento do payload 
    char _payload[256];              //Dados do pacote
} packet; 