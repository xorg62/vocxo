#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h> /* va_* use */
#include <netdb.h>
#include <sys/types.h> /* lol */
#include <netinet/in.h>
#include <sys/socket.h>

#include <ncurses.h> /* INTERFACE */

/* What port do you want to use? */
#define PORT 1234
#define MAXBUFFER 512
#define SERVADDR "127.0.0.1"

/* Global var */
int Sock;

int
send_raw(int sock, char *buffer, ...) /* <- here */
{
  char buf[MAXBUFFER];
  va_list va;

  /* Test if sock is ok */
  if(!sock)
    return 1; /* returning 1 mean ERROR */

  /* This is part is for make a format function */
  /* example: send_raw(s, "blibli %d", 3); */
  /* It's why i put '...' in function argument */
  va_start(va, buffer);
  vsnprintf(buf, MAXBUFFER, buffer, va);
  va_end(va);

  /* Real function to send buffer via socket */
  write(sock, buf, strlen(buf));

  refresh();

  return 0;
}

int
init_connection(char *nick)
{
  struct hostent *host;
  struct sockaddr_in a;

  if((Sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Can't init socket\n");
    return -1;
  }

  if(!(host = gethostbyname(SERVADDR)))
  {
    fprintf(stderr, "Can't get host by name\n");
    return 1;
  }
  /* Clean struct, copy adress in connection struct */
  bzero((char*)&a, sizeof(a));
  /*bcopy((char*)host->h_addr, (char*)&a.sin_addr.s_addr, host->h_length);*/

  a.sin_family = AF_INET;
  a.sin_port   = htons(PORT);
  memcpy(&a.sin_addr, host->h_addr_list[0], (size_t)host->h_length);

  if(connect(Sock, (const struct sockaddr*)&a, sizeof(a)) < 0)
  {
    fprintf(stderr, "Can't connect\n");
    return 1;
  }

  /* Identify to the server */
  send_raw(Sock, "IDENTIFY :%s", nick);

  return 1;
}

int
main(int argc, char **argv)
{
  /* We can't use 'socket' as a name, it's a function */
  char buffer[MAXBUFFER] = { 0 };
  char recbuf[MAXBUFFER] = { 0 };
  int n, maxfd, running = 1;
  int pos = 0;
  char c;
  struct timeval tv;
  fd_set iset;

  if(argc < 2)
  {
    fprintf(stderr, "%s Usage: %s <nickname>\n", argv[0], argv[0]);
    return 1;
  }

  /* Ncurses init */
  initscr();
  raw();    /* non-blocking keyboard stuff */
  cbreak(); /* ^C enable */

  if(init_connection(argv[1]) < 0)
    return 1; 
  
  /* Alternate connection/interface for typing */
  while(running)
  {
    tv.tv_sec  = 0;
    tv.tv_usec = 250000;

    FD_ZERO(&iset);
    FD_SET((maxfd = STDIN_FILENO), &iset);

    if(Sock > 0)
    {
      maxfd = (maxfd < Sock) ? Sock : maxfd;
      FD_SET(Sock, &iset);
    }
    
    if(select(maxfd + 1, &iset, NULL, NULL, &tv) > 0)
    {
      /* typing, WILL BE REPLACED BY NCURSES INTERFACE */
      if(FD_ISSET(STDIN_FILENO, &iset))
      {
        switch((c = getch()))
        {
          case 10:
            send_raw(Sock, "SAY %s :%s", argv[1], buffer);
            bzero(buffer, MAXBUFFER);
            pos = 0;
            addstr(buffer);
            addch('\n');
            break;
          default:
            buffer[pos++] = c;
            break;
        }
      }
      /* Reception */
      else
      {
        bzero(recbuf, MAXBUFFER);
        if((n = read(Sock, recbuf, MAXBUFFER)))
          printw("(from server): %s\n", recbuf);
      }
    }

    refresh();
  }

  /* close socket */
  close(Sock);

  /* Close ncurses */
  endwin();

  return 0;
}

