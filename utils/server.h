#include "../utils/config.h"


#define DEVICE_FREE -1
#define MAIN_REPOSITORY "./../repository"

struct client
{
  int devices[2];
  char username[USER_MAX_NAME];
  struct file_info file_info[FILE_MAX];
  int logged;
};

struct client_list
{
  struct client client;
  struct client_list *next;
};

void insertList(struct client_list **client_list, struct client client);
int initializeClientList();
void *client_thread (void *socket);
int initializeClient(int client_socket, char *username, struct client *client);
int findNode(char *username, struct client_list *client_list, struct client_list **client_node);
void *sync_thread_sv(void *socket);
void listen_sync(int client_socket, char *username);
void send_all_files(int client_socket, char *username);
int getFileSize(FILE *ptrfile);
void receive_file(struct packet responseThread, int socket, char*username);
void updateFileInfo(char *username, struct file_info file_info);
void delete_file(char *file, int socket, char *username);
void listen_client(int client_socket, char *username);
void show_files();
void send_file_info(int socket, char *username);
void close_client_connection(int socket, char *username);
void send_file(char *file, int socket, char *username);
