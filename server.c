#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sleep() */
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h> /* fcntl() */

#define PORT       1234
#define MAXBUFFER  512
#define MAXNICKLEN 24 

typedef struct
{
  int sock;
  char nick[MAXNICKLEN];
  struct sockaddr_in si;
} ClientStruct;

typedef struct
{
  int sock;
  struct sockaddr_in srvaddr;
  ClientStruct client[10]; /* temporary size nigger, linked list for client later instead of table */
  unsigned int nclients;
} ServStruct;

/* Global variable */
ServStruct *srv;

/*
 * Init and alloc a server structure 
 */
ServStruct*
server_init(void)
{
  ServStruct *s;

  /* Alloc serv */
  s = calloc(1, sizeof(ServStruct));

  /* Init serv socket */
  if((s->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Can't init socket\n");
    return NULL;
  }

  fcntl(s->sock, F_SETFL, O_NONBLOCK);
  /* Init serv addr (sockaddr_in) */
  bzero((char*)&s->srvaddr, sizeof(s->srvaddr)); /* Clean whole struct */
  s->srvaddr.sin_family      = AF_INET;
  s->srvaddr.sin_port        = htons(PORT);
  s->srvaddr.sin_addr.s_addr = INADDR_ANY;       /* Accept any adress sending message to serv */

  /* Bind serv on adress */
  if(bind(s->sock, (struct sockaddr*)&s->srvaddr, sizeof(s->srvaddr)) < 0)
  {
    fprintf(stderr, "Can't bind server adress\n");
    return NULL;
  }
 
  /* Listen for scoket connection, limit queue of incoming connections */
  listen(s->sock, SOMAXCONN);

  /* Init client */
  s->nclients = 0;

  return s;
}

/* Check identification */
ClientStruct*
check_ident(int sock, ServStruct *s, struct sockaddr_in si, char *buffer)
{
  char nick[MAXNICKLEN];
  int i;

  if(!s || !buffer)
    return NULL;

  if(sscanf(buffer, "IDENTIFY :%s", &nick) == 1)
  {
    for(i = 0; i < s->nclients; ++i)
      if(!strcmp(nick, s->client[i].nick)) /* nick already taken */
        return NULL;

    i = s->nclients++;
    strcpy(s->client[i].nick, nick);
    s->client[i].si = si;
    s->client[i].sock = sock;

    return &s->client[i];
  }

  return NULL;
}

/* Check say message from clients */
char*
check_say(ServStruct *s, char *buffer, ClientStruct *c)
{
  int i;
  char nick[MAXNICKLEN];
  static char buf[MAXBUFFER];

  if(!s || !buffer)
    return NULL;

  if(sscanf(buffer, "SAY %s :%s", &nick, &buf) == 2)
  {
    for(i = 0; i < s->nclients; ++i)
      if(!strcmp(nick, s->client[i].nick))
      {
        *c = s->client[i];
        return buf;
      }
  }

  return NULL;
}

int
main(int argc, char **argv)
{
  int i, newsock, tmpsock;
  struct sockaddr_in ca, clientaddr[16];
  int recbytes, addrlen;
  char buffer[MAXBUFFER] = { 0 }; /* fill buffer with 0 */
  char tmpbuf[MAXBUFFER] = { 0 }; /* stuff to send to clients */
  char *tmpbuf2 = NULL;
  socklen_t slen;

  int maxfd, running = 1;

  static struct timeval tv;
  fd_set iset;
  ServStruct *s;
  ClientStruct *c, cc;


  /* Server init */
  if(!(s = server_init()))
  {
    fprintf(stderr, "Can't init server\n");
    return 1;
  }

  slen = sizeof(s->srvaddr);

  /* Accept a new connection on a socket */
  while(running)
  {
     usleep(100000);

    /* Something happen on serv */
    if((i = accept(s->sock, (struct sockaddr*)&ca, &slen)) >= 0)
      fcntl((newsock = i), F_SETFL, O_NONBLOCK);

    bzero(buffer, MAXBUFFER);
    bzero(tmpbuf, MAXBUFFER);

    if(!(recbytes = read(newsock, buffer, MAXBUFFER)) || !strlen(buffer))
      continue;

    /* Server even don't print this with "SAY" <= !! so client doesn't send SAY*/
    printf("READ: %s\n", buffer);

    /* Check identification from clients connecting */
    if((c = check_ident(newsock, s, ca, buffer)))
      sprintf(tmpbuf, "Join: %s\n", c->nick);
    
    /* Check SAY message from clients */
    if((tmpbuf2 = check_say(s, buffer, &cc)))
      sprintf(tmpbuf, "<%s>: %s\n", cc.nick, tmpbuf2);
    
    /* Send message to different client */
    if(tmpbuf)
      for(i = 0; i < s->nclients; ++i)
        /* We have to look about another function to send message to every
         * client */
        write(s->client[i].sock, tmpbuf, strlen(tmpbuf));

   }

  close(s->sock);
  close(newsock);
  free(s);
  
  return 0;
}

