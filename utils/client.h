#define USER_MAX_NAME 20
#define FILE_MAX_SIZE 20
#define SHOULD_CREATE_THREAD "1"

// struct client {
//   int devices[2];
//   char username[USER_MAX_NAME];
//   struct file_info file_info[FILE_MAX_SIZE];
//   int logged;
// };

int initHost(char *argv[], int argc);
int connect_server (char *host, int port);
char *itoa_simple(char *dest, int i);
