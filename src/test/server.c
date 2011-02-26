/*
*      Redistribution and use in source and binary forms, with or without
*      modification, are permitted provided that the following conditions are
*      met:
*
*      * Redistributions of source code must retain the above copyright
*        notice, this list of conditions and the following disclaimer.
*      * Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following disclaimer
*        in the documentation and/or other materials provided with the
*        distribution.
*      * Neither the name of the  nor the names of its
*        contributors may be used to endorse or promote products derived from
*        this software without specific prior written permission.
*
*      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*      "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*      OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*      SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*      LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*      DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*      THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "server.h"

/* do not hesitate to give me idea nigger */

/* Vocxo server text protocol:
 *
 *  [Client side] (what client have to send)
 *  (nick is client's nick)
 *
 *  -Connection:
 *    packet: 'IDENTIFY :nick'
 *  -Deconnection:
 *    packet: 'QUIT nick :quitmsg'
 *  -Text message:
 *    packet: 'SAY nick :message'
 *  -Private message:
 *    packet: 'PRIVSAY nick recnick :message'
 *  -Names list request:
 *    packet: 'NAMES nick'
 *
 *
 *  [Server side] (what server has to send)
 *
 *  -Join event:
 *    packet: 'JOIN :nick'
 *  -Quit event:
 *    packet: 'QUIT nick :quitmsg'
 *  -Text message:
 *    packet: 'SAY nick :message'
 *  -Private message:
 *    packet: 'PRIVSAY nick :message'
 *  -Names list of server:
 *    packet: 'NAMES :names list'
 *
 */

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

    /* Make connection function non-blocking */
    fcntl(s->sock, F_SETFL, O_NONBLOCK);

    /* Init serv addr (sockaddr_in) */
    /* Clean whole struct */
    bzero((char*)&s->srvaddr, sizeof(s->srvaddr));
    s->srvaddr.sin_family      = AF_INET;
    s->srvaddr.sin_port        = htons(PORT);

    /* Accept any adress sending message to serv */
    s->srvaddr.sin_addr.s_addr = INADDR_ANY;

    /* Bind serv on adress */
    if(bind(s->sock, (struct sockaddr*)&s->srvaddr, sizeof(s->srvaddr)) < 0)
    {
         fprintf(stderr, "Can't bind server adress\n");
         return NULL;
    }

    /* Listen for socket connection, limit queue of incoming connections */
    listen(s->sock, SOMAXCONN);

    /* Init client */
    s->nclients = 0;

    return s;
}

/* Check identification */
ClientStruct*
(int sock, ServStruct *s, struct sockaddr_in si, char *buffer)
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
server_parse_buffer(char *buffer)
{

    return;
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
                 * connected client. */
                write(s->client[i].sock, tmpbuf, strlen(tmpbuf));

    }

    close(s->sock);
    close(newsock);
    free(s);

    return 0;
}

