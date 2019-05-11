#include "../utils/config.h"


#define FILEEXT_MAX_SIZE 8

#define DEVICE_FREE -1
#define MAIN_REPOSITORY "./../repository"

struct file_info
{
  char name[FILENAME_MAX_SIZE];
  char extension[FILEEXT_MAX_SIZE];
  char last_modified[20];
  time_t lst_modified;
  int size;
  pthread_mutex_t file_mutex;
};

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