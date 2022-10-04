#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <talk.h>
#include <pwd.h>
#include <unistd.h>
#include <arpa/inet.h>

#define LOCAL 0
#define REMOTE 1
#define MAXLINE 1000
#define THREE 3

/* function prototypes */
void checkPort(int portno);
void client(char* hostname, int portno);
void chatClient(int sockfd, struct hostent* host);
void server(int portno);
void chatServer(int sockfd);

/* flags */
int vflag = 0;
int aflag = 0;
int nflag = 0;

int main(int argc, char* argv[])
{

    /* variable declarations */
    int opt = 0;
    int portno = 0;
    int cflag = 0;
    char* hostname = NULL;

    /* check for optional args */
    while( (opt = getopt(argc, argv, "vaN")) != -1)
    {
        switch(opt)
        {
	    /* set flags */
            case 'v':
		vflag = 1;
                break;
            case 'a':
		aflag = 1;
                break;
            case 'N':
		nflag = 1;
                break;
            default:
                printf("Unknown option: %c\n", optopt);
                exit(EXIT_FAILURE);
        }
    }
        
    /* check if port number is given */
    if(optind >= argc)
    {
        fprintf(stderr, "Expected argument after options.\n");
        exit(EXIT_FAILURE);
    }

    /* hostname is present set flag to client mode */ 
    if( (argc - optind) == 2)
    {
        cflag = 1;
    }
   
    /* check if user is client or server */ 
    if(cflag == 1)
    {
        /* client mode */
        hostname = argv[optind];
        optind++;
        portno = atoi(argv[optind]);    

        /* check for valid port */
        checkPort(portno);

	/* call client function */
	client(hostname, portno);

    }
    else
    {
        /* server mode */
        portno = atoi(argv[optind]);

        /* check for valid port */
        checkPort(portno);

	/* call server function */
	server(portno);
    }

    return 0;
}

/* check for valid port number */
void checkPort(int portno)
{
    const int portlow = 1;
    const int porthigh = 65535;
    
    /* check for valid port number */
    if( !(portno >= portlow && portno <= porthigh) )
    {
        fprintf(stderr, "Invalid port number.\n");
        exit(EXIT_FAILURE);
    }
}

/* set up client */
void client(char* hostname, int portno)
{
    struct hostent* hostent;
    int sockfd = 0;
    struct sockaddr_in sa;

    /* check for valid host name */
    if( (hostent = (struct hostent*)gethostbyname(hostname)) == NULL)
    {
            perror("gethostname");
            exit(EXIT_FAILURE);
    }

    /* create socket and error check */
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* assign internet, portno and address */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    sa.sin_addr.s_addr = *(uint32_t*)hostent->h_addr_list[0];

    /* connect to server */
    if( (connect(sockfd, (struct sockaddr *)&sa, sizeof(sa))) < 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
 
    /* verbose flag set print information */   
    if(vflag)
    {
	printf("Options:\n");
	printf("%4s %14s %2s %d\n", "int", "opt_verbose", "=", vflag);
	printf("%9s %9s %2s client\n", "talkmode", "opt_mode", "=");
	printf("%4s %14s %2s %d\n", "int", "opt_port", "=", portno);
	printf("%5s %13s %2s (none)\n", "char", "*opt_host", "=");
	printf("%4s %14s %2s %d\n", "int", "opt_accept", "=", aflag);
	printf("%4s %14s %2s %d\n", "int", "opt_windows", "=", nflag);
    }

    /* call client chat */
    chatClient(sockfd, hostent);
} 

void chatClient(int sockfd, struct hostent* host)
{
    struct pollfd fds[2];
    int len = 0;
    int mlen = 0;
    char buffer[MAXLINE];
    int print = 1;
    char error[THREE] = "^C";
    struct passwd* pw;
    uid_t uid;
    char pass[MAXLINE];

    /* set up STDIN and sockfd fd */
    fds[LOCAL].fd = STDIN_FILENO;
    fds[LOCAL].events = POLLIN;
    fds[LOCAL].revents = 0;
    fds[REMOTE] = fds[LOCAL];
    fds[REMOTE].fd = sockfd;

     /* get username of user of client */
    if( (pw = getpwuid(uid = getuid())) == NULL)
    {
	perror("getpwuid");
	exit(EXIT_FAILURE);
    }

    /* pass username to pass buffer */
    sprintf(pass, "%s", pw->pw_name); 

    /* send pass  buffer to server */
    mlen = send(sockfd, pass, sizeof(pass), 0);
    if( mlen == -1)
    {
	perror("send password");
	exit(EXIT_FAILURE);
    }

    /* ask for connection if a flag is set */ 
    if(!aflag)
    { 
      printf("Waiting for response from %s.\n", host->h_name);
   
      /* receive message from server */ 
      mlen = recv(sockfd, buffer, sizeof(buffer), 0);

      if(mlen == -1)
      {
        perror("recv");
        exit(EXIT_FAILURE);
      }

      /* if NOT ok host declined connection */
      if( strcmp(buffer, "ok") != 0)
      {
	printf("%s declined connection.\n", host->h_name);
	close(sockfd);
	exit(EXIT_FAILURE);
      }
    }

    /* if nflag is not set */
    if(!nflag)
    {
      start_windowing();
    }

    /* when EOF is hit stop */
    while(1)
    {
	/* if hit eof exit program */
	if(has_hit_eof())
	{
	  stop_windowing();
	  mlen = send(sockfd, error, sizeof(error), 0);
	  if( mlen == -1)
	  {
	    break;
	  }

	  exit(EXIT_FAILURE);
	}
	/* poll for stdin or sockfd */
	if( (poll(fds, sizeof(fds) / sizeof(struct pollfd), -1)) < 0)
	{
	   perror("poll");
	   stop_windowing();
	   exit(EXIT_FAILURE);
	}

	/* read a line in and send it to remote */
	if(fds[LOCAL].revents & POLLIN)
	{
	   /* update buffer */
	   update_input_buffer();

	   /* if has whole line read and send */
	   if(has_whole_line())
	   {
	      len = read_from_input(buffer, MAXLINE);
	      mlen = send(sockfd, buffer, len, 0);
	      if(mlen == -1)
	      {
		break;
	      }
	      
	   }
	}

	/* receive a line and write it to output */
	if(fds[REMOTE].revents & POLLIN)
	{
	  mlen = recv(sockfd, buffer, sizeof(buffer), 0);

	  /* check if ^C is received */	
	  if( (mlen == 0) )
	  {
	    break;
	  }

	  /* check if ^D is received */
	  if( strcmp(error, buffer) == 0)
	  {
	    break;
	  }

	  write_to_output(buffer, mlen);

	}	    
    }

    /* if server does ^C */
    while(1)
    {
	if(print == 1)
	{
	  fprint_to_output("Connection Closed. ^C to terminate.\n");
	  print++;
	}
    }

    stop_windowing();

}

/* server function */
void server(int portno)
{
    int sockfd = 0;
    struct sockaddr_in sa;

    /* create socket and error check */
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* attach to address */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    /* bind to address */
    if( (bind(sockfd, (struct sockaddr*)&sa, sizeof(sa))) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
   
    /* wait for connection */  
    if( (listen(sockfd, 1)) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* verbose flag print information */
    if(vflag)
    {
	printf("Options:\n");
	printf("%4s %14s %2s %d\n", "int", "opt_verbose", "=", vflag);
	printf("%9s %9s %2s server\n", "talkmode", "opt_mode", "=");
	printf("%4s %14s %2s %d\n", "int", "opt_port", "=", portno);
	printf("%5s %13s %2s (none)\n", "char", "*opt_host", "=");
	printf("%4s %14s %2s %d\n", "int", "opt_accept", "=", aflag);
	printf("%4s %14s %2s %d\n", "int", "opt_windows", "=", nflag);
    }


    /* call chat server function */
    chatServer(sockfd);
}

void chatServer(int sockfd)
{
    struct sockaddr_in client;
    socklen_t clen = 0;
    char input[100];
    int newsock = 0;
    struct pollfd fds[2];
    int len = 0;
    int mlen = 0;
    char buffer[MAXLINE];
    int print = 1;
    char ct[MAXLINE];
    int resc = 0;
    char ok[THREE] = "ok";
    char no[THREE] = "no";
    char address[MAXLINE];
    char error[THREE] = "^C";
 
    /* if verbose flag is set */
    if(vflag)
    {
	printf("VERB: Waiting for connection...");
    }

    /* accept a connection */
    clen = sizeof(client);
    if( (newsock = accept(sockfd, (struct sockaddr*)&client, &clen)) < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }
   
    /* get ip address and print when verbose flag is set */
    inet_ntop(AF_INET, &(client.sin_addr), address, MAXLINE);
    if(vflag)
    {
	printf("New connection from: %s\n", address);
    }

    /* get name of client host */
    if( (resc = getnameinfo((struct sockaddr*)&client, sizeof(client),
	ct, sizeof(ct), NULL, 0, NI_NAMEREQD)) < 0)
    {
	perror("getnameinfo");
	exit(EXIT_FAILURE);
    }

    /* recieve username from client */   
    mlen = recv(newsock, buffer, sizeof(buffer), 0);
    if( mlen == -1)
    {
       perror("recv");
       exit(EXIT_FAILURE);
    }	
 
    /* append hostname to username */
    strcat(buffer, "@");
    strcat(buffer, ct);

    /* ask for connection if aflag is not set */
    if(!aflag)
    {
      printf("Mytalk request from %s. Accept (y/n)? ", buffer);
      scanf("%99s", input);

      /* compare input to yes and y */
      if( (strcmp(input, "yes") != 0) && (strcmp(input, "y") != 0) )
      {
	/* send no message to client */
        mlen = send(newsock, no, sizeof(no), 0);
	
	if(mlen == -1)
	{
	  perror("send");
	  exit(EXIT_FAILURE);
	}
	
	exit(EXIT_FAILURE);
      }

      /* send ok message to client */
      mlen = send(newsock, ok, sizeof(ok), 0);
      if(mlen == -1)
      {
	perror("send");
	exit(EXIT_FAILURE);
      }
     } 

    /* set up stdin and newsock fd */ 
    fds[LOCAL].fd = STDIN_FILENO;
    fds[LOCAL].events = POLLIN;
    fds[LOCAL].revents = 0;
    fds[REMOTE] = fds[LOCAL];
    fds[REMOTE].fd = newsock;

    /* do not start windowing if nflag is set */   
    if(!nflag)
    { 
      start_windowing();
    }

    
   /* while(has_hit_eof() != 1)*/
    while(1)
    {
	/* if hit eof exit program */
	if(has_hit_eof())
	{
	  stop_windowing();
	  mlen = send(newsock, error, sizeof(error), 0);
	  if( mlen == -1)
	  {
	    break;
	  }

	  exit(EXIT_FAILURE);
	}

	/* poll local and remote */
	if( (poll(fds, sizeof(fds) / sizeof(struct pollfd), -1)) < 0)
	{
	   perror("poll");
	   stop_windowing();
	   exit(EXIT_FAILURE);
	}

	/* read a line and send it to remote */
	if(fds[LOCAL].revents & POLLIN)
	{
	   /* update internal buffer */
	   update_input_buffer();

 	   /* if has whole line read and send */
	   if(has_whole_line())
	   {
	      len = read_from_input(buffer, MAXLINE);

	      mlen = send(newsock, buffer, len, 0);

	      if(mlen == -1)
	      {
		break;
	      }

	   }
	}

	/* recieve line and write to ouput */
	if(fds[REMOTE].revents & POLLIN)
	{
	  mlen = recv(newsock, buffer, sizeof(buffer), 0);

	  /* check if ^C is received */	
	  if( (mlen == 0) )
	  {
	    break;
	  }

	  /* check if ^D is received */
	  if( strcmp(error, buffer) == 0)
	  {
	    break;
	  }

	  write_to_output(buffer, mlen);

	}	    
    }

    /* when client presses ^C */
    while(1)
    {
	if(print == 1)
	{
	  fprint_to_output("Connection closed. ^C to terminate.\n");
    	  print++;
	}
    }

    stop_windowing();
}
