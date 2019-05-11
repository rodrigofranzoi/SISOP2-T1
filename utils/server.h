#include "../utils/config.h"

#define FILENAME_MAX_SIZE 20
#define FILEEXT_MAX_SIZE 8
#define FILE_MAX 20

#define DEVICE_FREE -1

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
