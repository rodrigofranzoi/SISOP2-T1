#include <netdb.h> 
#include <pwd.h>
#include <sys/inotify.h>
#include "../utils/config.h"
#include "../utils/client.h"
#include "../utils/utils.h"


char username[USER_MAX_NAME];
char *host;
int port;
char directory[FILENAME_MAX_SIZE + 50];
int sockfd = -1, sync_socket = -1;
int notifyfd;
int watchfd;

int main(int argc, char *argv[])
{

    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
    
    //Verifica os parametros e configura o host
    if(initHost(argv, argc) == -1) exit(0);

    if ((connect_server(host, port)) > 0) {
		// sincroniza diretório do servidor com o do cliente
		sync_client_first();

		// espera por um comando de usuário
		client_interface();
    }


    return 0;
}

int initHost(char *argv[], int argc) {
    int resp = 1;
    
    if (argc < 3) {
		printf("Insufficient arguments: user, hostname or port \n");
		return -1;
    }

    // primeiro argumento nome do usuário
	if (strlen(argv[1]) <= USER_MAX_NAME) {
        strcpy(username, argv[1]);
    } else {
        printf("User Name Is Too long \n");
        return -1;
    };

    //segundo arumento hostname do server	
	host = malloc(sizeof(argv[2]));
	strcpy(host, argv[2]);

	// terceiro argumento porta
    port = atoi(argv[3]);

    return resp;
}

int connect_server (char *host, int port) {

	int socketByteSize, sockfd;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char buffer[256];

    packet connected;

    // Inicializa Server
	server = gethostbyname(host);
	if (server == NULL) {
        printf("ERROR, no such host\n");
        return -1;
    }

	// Inicializa Socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("ERROR opening socket\n");
		return -1;
	}

	// Inicializa Addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(server_addr.sin_zero), 8);

    // Testa Socket
	if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0) {
  		printf("ERROR connecting\n");
		return -1;
	}

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = SHOULD_CREATE_THREAD;
	write(sockfd, &threadSetter, sizeof(struct packet));

    packet userPacket;
    userPacket.type = RESP;
    strcpy(userPacket._payload, username);

	//envia username para o servidor
    socketByteSize = write(sockfd, &userPacket, sizeof(struct packet));
    if (socketByteSize < 0) {
 	    printf("ERROR sending username to server\n");
        return -1;
    }

	socketByteSize = read(sockfd, &connected, sizeof(struct packet));

    if(connected.type ==  RESP){
        printf("respo payload %d\n", connected.payloadCommand);
        if (socketByteSize < 0){
            printf("ERROR receiving connected message\n");
            return -1;
        } else if (connected.payloadCommand) {
            printf("connected\n");
            return 1;
        } else {
            printf("You already have two devices connected\n");
            return -1;
        }
    } else return -1;
}

void sync_client_first() {
	char *homedir;
	char fileName[FILE_MAX + 10] = "sync_dir_";
	pthread_t syn_th;

	if ((homedir = getenv("HOME")) == NULL) {
        printf("entrei aqui e n sei pq");
        homedir = getpwuid(getuid())->pw_dir;
    }
	// nome do arquivo
	strcat(fileName, username);

	// forma o path do arquivo
	strcpy(directory, homedir);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0) {
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else {
		printf("Creating %s directory in your home\n", fileName);
	}

	initializeNotifyDescription();

	//cria thread para sincronização
	if(pthread_create(&syn_th, NULL, sync_thread, NULL)) {
		printf("ERROR creating thread\n");
	}
}

void *sync_thread() {
	int length, i = 0;
    char buffer[BUF_LEN];
	char path[200];

	create_sync_sock();
	get_all_files();

	// while(1)
	// {
	//   length = read( notifyfd, buffer, BUF_LEN );

	//   if ( length < 0 ) {
	//     perror( "read" );
	//   }

	//   while ( i < length ) {
	//     struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
	//     if ( event->len ) {
	// 			if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
	// 				strcpy(path, directory);
	// 				strcat(path, "/");
	// 				strcat(path, event->name);
	// 				if(exists(path) && (event->name[0] != '.'))
	// 				{
	// 					upload_file(path, sync_socket);
	// 				}
	// 			}
	// 			else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
	// 			{
	// 				strcpy(path, directory);
	// 				strcat(path, "/");
	// 				strcat(path, event->name);
	// 				if(event->name[0] != '.')
	// 				{
	// 					delete_file_request(path, sync_socket);
	// 				}
	// 			}
	//     }
	//     i += EVENT_SIZE + event->len;
  	// }
	// 	i = 0;

	// 	sleep(10);
	// }
}

void initializeNotifyDescription() {
	notifyfd = inotify_init();

	watchfd = inotify_add_watch(notifyfd, directory, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
}


void get_all_files() {
	int byteCount, bytesLeft, fileSize, fileNum, i;
	FILE* ptrfile;
	char dataBuffer[KBYTE], file[FILENAME_MAX_SIZE], path[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor

    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DOWNLOADALL;
	byteCount = write(sync_socket, &threadRequest, sizeof(struct packet));

	if (byteCount < 0)
		printf("Error sending DOWNLOAD message to server\n");

	packet connection;
	byteCount = read(sync_socket, &connection, sizeof(struct packet));
	fileNum = connection.payloadCommand;

	for(i = 0; i < fileNum; i++) {
		// lê nome do arquivo do servidor
		packet connectionFile;
		byteCount = read(sync_socket, &connectionFile, sizeof(struct packet));
		if (byteCount < 0) {
			printf("Error receiving filename\n");
		}
		strcpy(file, connectionFile._payload);

		strcpy(path, directory);
		strcat(path, "/");
		strcat(path, file);

		// cria arquivo no diretório do cliente
		ptrfile = fopen(path, "wb");

		packet connectionFileSize;
		read(sync_socket, &connectionFileSize, sizeof(struct packet));
		fileSize = connectionFileSize.length;
		// número de bytes que faltam ser lidos
		bytesLeft = fileSize;

		if (fileSize > 0)
		{
			while(bytesLeft > 0)
			{
				// lê 1kbyte de dados do arquivo do servidor
				packet connectionFileData;
				byteCount = read(sync_socket, &connectionFileData, sizeof(struct packet));
				strcpy(dataBuffer, connectionFileData._payload);

				// escreve no arquivo do cliente os bytes lidos do servidor
				if(bytesLeft > KBYTE)
				{
					byteCount = fwrite(dataBuffer, KBYTE, 1, ptrfile);
				}
				else
				{
					fwrite(dataBuffer, bytesLeft, 1, ptrfile);
				}
				// decrementa os bytes lidos
				bytesLeft -= KBYTE;
			}
		}

		fclose(ptrfile);
	}
}

int create_sync_sock()
{
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 0;
	char buffer[256];

	server = gethostbyname(host);

	if (server == NULL)
	{
  	return -1;
  }

	// tenta abrir o socket
	if ((sync_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sync_socket,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0)
	{
		  return -1;
	}

    packet threadSetter;
    threadSetter.type = RESP;
    threadSetter.payloadCommand = 0;
	write(sync_socket, &threadSetter, sizeof(struct packet));

    packet userPacket;
    userPacket.type = RESP;
    // userPacket.payloadCommand = SHOULD_CREATE_THREAD;
    strcpy(userPacket._payload, username);
	//envia username para o servidor
    byteCount = write(sync_socket, &userPacket, sizeof(struct packet));
    if (byteCount < 0) {
 	    printf("ERROR sending username to server\n");
        return -1;
    }


}

// função que extrai o nome do arquivo a partir de um pathname
void getFilename(char *pathname, char *filename)
{
	char *filenameAux;

	filenameAux = strtok(pathname, "/");

	strcpy(filename, filenameAux);

	while(filenameAux != NULL)
	{
		strcpy(filename, filenameAux);

		filenameAux = strtok(NULL, "/");
	}
}

time_t getFileModifiedTime(char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    return 0;
}

int exists(const char *fname)
{
    FILE *file;
    if (file = fopen(fname, "rb"))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

void delete_file_request(char* file, int socket)
{
	int byteCount;

    packet threadRequest;
    threadRequest.type = CMD;
    threadRequest.payloadCommand = DELETE;

	getFilename(file, threadRequest._payload);

	byteCount = write(socket, &threadRequest, sizeof(struct packet));

	if (byteCount < 0)
		printf("ERROR sending delete file request\n");
}

void upload_file(char *file, int socket)
{
	int byteCount, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE];
    packet threadRequest;

	if (ptrfile = fopen(file, "rb"))
	{
			getFilename(file, threadRequest._payload);
            threadRequest.type = RESP;
            threadRequest.payloadCommand = UPLOAD;
            threadRequest.length = getFileSize(ptrfile);
            write(socket, &threadRequest, sizeof(struct packet));


			// escreve número de bytes do arquivo
			byteCount = write(socket, &fileSize, sizeof(fileSize));

			if (fileSize == 0)
			{
				fclose(ptrfile);
				return;
			}

			while(!feof(ptrfile))
			{
					fread(dataBuffer, sizeof(dataBuffer), 1, ptrfile);

					byteCount = write(socket, dataBuffer, KBYTE);
					if(byteCount < 0)
						printf("ERROR sending file\n");
			}
			fclose(ptrfile);

			if (socket != sync_socket)
				printf("the file has been uploaded\n");
	}
	// arquivo não existe
	else
	{
		printf("ERROR this file doesn't exist\n\n");
	}
}

int getFileSize(FILE *ptrfile) {
	int size;

	fseek(ptrfile, 0L, SEEK_END);
	size = ftell(ptrfile);
	rewind(ptrfile);

	return size;
}

void client_interface()
{
	int command = 0;
	char request[200], file[200];

    while(1){

    }

	// printf("\nCommands:\nupload <path/filename.ext>\ndownload <filename.ext>\nlist\nget_sync_dir\nexit\n");
	// do
	// {
	// 	printf("\ntype your command: ");

	// 	fgets(request, sizeof(request), stdin);

	// 	command = commandRequest(request, file);

	// 	// verifica requisição do cliente
	// 	switch (command)
	// 	{
	// 		case LIST: show_files(); break;
	// 		case EXIT: close_connection();break;
	// 		case SYNC: get_all_files();break;
	// 		case DOWNLOAD: get_file(file);break;
	// 	  case UPLOAD: upload_file(file, sockfd); break;

	// 		default: printf("ERROR invalid command\n");
	// 	}
	// }while(command != EXIT);
}