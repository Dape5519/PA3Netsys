#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<netdb.h>
#include<signal.h>
#include<fcntl.h>

// Set packet size and limit for max client connect
#define MAXCLIENTS 1000
#define BUFSIZE 1024

int listenfd;
int listenfd1;
int clientConnection[MAXCLIENTS];
int serverConnection[MAXCLIENTS];
char *WWW;

// The fork function
void response_handler(int);

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
  perror(msg);
  exit(1);
}

// function to combine to strings into one
char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

// function to get size of the file from https://www.includehelp.com/c-programs/find-size-of-file.aspx*/
long int findSize(const char *file_name)
{
    struct stat st; /*declare stat variable*/
    /*get the size using stat()*/
    if(stat(file_name,&st)==0)
        return (st.st_size);
    else
        return -1;
}

// function to get file extension from https://stackoverflow.com/questions/5309471/getting-file-extension-in-c
const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}



int main(int argc , char **argv)
{
  int optval = 1;
  int spot = 0; // client number we are servicing
  struct sockaddr_in clientaddr;
  socklen_t addrlen;
  struct addrinfo hints, *res, *p;

  void  INThandler(int sig)
  {
       char  c;
       printf("df\n");
       shutdown(clientConnection[spot], SHUT_RDWR);
       close(clientConnection[spot]);
       signal(sig, SIG_IGN);
       exit(0);
  }

  signal(SIGINT, INThandler);

  //Make sure the port number is there
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Set all client connections to -1 for unused
	for (int i=0; i<MAXCLIENTS; i++)
		clientConnection[i] = -1;

  //Get address
  bzero((char *) &hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo( NULL, argv[1], &hints, &res) != 0)
  {
	  perror ("getaddrinfo() error");
	  exit(1);
 	}
  // socket() and bind()

  for (p = res; p != NULL; p = p -> ai_next)
  {
    listenfd = socket (p->ai_family, p -> ai_socktype, 0);
    if (listenfd == -1) continue;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (bind(listenfd, p -> ai_addr, p -> ai_addrlen) == 0) break;
  }
  if (p == NULL)
  {
    perror ("socket() and bind()");
    exit(1);
  }

  //Set connection directory
  WWW = concat(getenv("PWD"),"/www");
  freeaddrinfo(res);

  // listen for incoming connections
  if ( listen (listenfd, 1000000) != 0 )
  {
	  perror("listen() error");
    exit(1);
  }

  //Accept the incoming connections
  while (1)
  {
    addrlen = sizeof(clientaddr);
    clientConnection[spot] = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);
    if (clientConnection[spot] < 0)
      perror("accept() error");
    else
    {
      //Handle multiple clients with new procces via fork()
      if ( fork() == 0 )
      {
        response_handler(spot);
        exit(0);
      }
    }

    while (clientConnection[spot] !=-1 ) spot = (spot + 1) % MAXCLIENTS;
  }
  return 0;
}
void response_handler(int n)
{
  int bytes_read;
  char data_to_send[BUFSIZE];
  int host_not_found; // complex bool for parsing hostname
  char hostname[100000]; // Name of host
	char message[100000]; // Incoming message
  char *requestline[3]; // For message parsing
  char buffer[BUFSIZE]; //Holds data we send
  char path[100000]; // File path
  char strSize[100000]; //File size
  char *content;//both extension and filesize content information
  char contents[100000];
	int rcvd; //Client received
  int fd; //File in directory found
  int BUFSIZE_read;
  long int size; //File size
  int optval = 1;
  int spot = 0; // client number we are servicing
  struct sockaddr_in clientaddr;
  socklen_t addrlen;
  struct addrinfo hints, *res, *p;
  char ip[1000];

  //Clear message
  bzero(message,100000);
  //Recieve the client
	rcvd = recv(clientConnection[n], message, 100000, 0);
	if (rcvd < 0)         // error
		perror("recv failed");
	else if (rcvd == 0)   // receive socket closed
		perror("Client upexpectedly disconnected");
	else                  // message received
	{
    //printf("%s\n",message);
    content = strndup(message,sizeof(message));
		requestline[0] = strtok(message, " :\t\n");
    //printf("requestline0 %s of size %ld \n", requestline[0],strlen(requestline[0]));
    host_not_found = 2;
    while(host_not_found)
    {
      requestline[1] = strtok (NULL, " :\t\n");
      if (requestline[1] == NULL)
        host_not_found = -1;
      if ( strncmp(requestline[1], "Host\0", 5) == 0 )
      {
        host_not_found = 0;
        requestline[1] = strtok (NULL, " :\t\n");
        //printf ("%s\n",requestline[1]);
      }
    }
    if ( strncmp(requestline[0], "GET\0", 4) == 0 )
    {
      printf("Content = %s\n", content);
      snprintf(hostname, strlen(requestline[1]), "%s", requestline[1]);
      bzero(message, 100000);
      printf("host %s2\n", hostname);
      //Get address
      bzero((char *) &hints, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;
      if (getaddrinfo( hostname, "http", &hints, &res) != 0)
      {
        perror ("getaddrinfo() error");
        exit(1);
      }
      // socket() and bind()
      for (p = res; p != NULL; p = p -> ai_next)
      {
        listenfd1 = socket (p->ai_family, p -> ai_socktype, p-> ai_protocol);
        if (listenfd1 == -1) continue;
        printf("listenFD: %d\n", listenfd1);
        //setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
        if (connect(listenfd1, p -> ai_addr, p -> ai_addrlen) == 0)
        {
          //p->ai_addr
          break;
        }
      }
      if (p == NULL)
      {
        perror ("socket() and bind()");
        exit(1);
      }
      //printf("\n%s\n",ip );
      freeaddrinfo(res);
      //else printf("htt777p\n");
      send(listenfd1, content, strlen(content) , 0 );
      while ( (bytes_read=read(listenfd1, data_to_send, BUFSIZE))>0 )
        write (clientConnection[n], data_to_send, bytes_read);
    }

	}


	// Close socket and shutdown
	shutdown(clientConnection[n], SHUT_RDWR);
	close(clientConnection[n]);
	clientConnection[n]=-1;
}
