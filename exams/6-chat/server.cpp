
//***************************************************************************
//
// Program example for labs in subject Operating Systems
//
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2017
//
// Example of socket server.
//
// This program is example of socket server and it allows to connect and serve
// the only one client.
// The mandatory argument of program is port number for listening.
//
//***************************************************************************

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"
#define STR_LOCK    "LOCK"
#define STR_UNLOCK    "UNLOCK"

#define MAX 3

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;

int arr_client[MAX];
int global_connestions = 0;
int position;

sem_t* semVV;
sem_t* sem;

char full[] = "Neni misto\n";

void log_msg( int t_log_level, const char *t_form, ... )
{
    const char *out_fmt[] = {
            "ERR: (%d-%s) %s\n",
            "INF: %s\n",
            "DEB: %s\n" };

    if ( t_log_level && t_log_level > g_debug ) return;

    char l_buf[ 1024 ];
    va_list l_arg;
    va_start( l_arg, t_form );
    vsprintf( l_buf, t_form, l_arg );
    va_end( l_arg );

    switch ( t_log_level )
    {
    case LOG_INFO:
    case LOG_DEBUG:
        fprintf( stdout, out_fmt[ t_log_level ], l_buf );
        break;

    case LOG_ERROR:
        fprintf( stderr, out_fmt[ t_log_level ], errno, strerror( errno ), l_buf );
        break;
    }
}

//***************************************************************************
// help

void help( int t_narg, char **t_args )
{
    if ( t_narg <= 1 || !strcmp( t_args[ 1 ], "-h" ) )
    {
        printf(
            "\n"
            "  Socket server example.\n"
            "\n"
            "  Use: %s [-h -d] port_number\n"
            "\n"
            "    -d  debug mode \n"
            "    -h  this help\n"
            "\n", t_args[ 0 ] );

        exit( 0 );
    }

    if ( !strcmp( t_args[ 1 ], "-d" ) )
        g_debug = LOG_DEBUG;
}

//***************************************************************************

void *thread_function(void* p_socket)
{

	int socket = *(int*) p_socket;
	char buffer[1024];

	printf("Socket: %d\n", socket);

	position = -1;

	sem_wait(semVV);

	for(int i = 0; i < MAX; i++)
	{
		if(arr_client[i] == -1)
		{
			arr_client[i] = socket;

		    position = i;
			break;
		}

	}


   if(position == -1)
   {
	   	log_msg(LOG_INFO, "Client could not ne added into list");
	   	close(socket);
	   	return NULL;
   }



	sprintf(buffer, "Na pozici [%d] je ulozen soket %d ", position, socket);
	log_msg(LOG_INFO, buffer);


	char message[50];
	sprintf(message, "[%d] Request of message", socket);

	while(1)
	{
		char l_buf[1024];
		char l_buf2[1024];

		int l_len = read(socket, l_buf, sizeof(l_buf));

		log_msg(LOG_INFO, "Read %d bytes", l_len);
	//	l_len = write(socket, l_buf, strlen(l_buf));

        if(!strncmp(l_buf, STR_CLOSE, strlen(STR_CLOSE)))
        {
        	log_msg(LOG_INFO, "Client sent 'close' request");
        	arr_client[socket-4] = -1;
        	close(socket);
        	break;
        }



        sprintf(l_buf2, "[%d]: %s", socket, l_buf);

        for(int i = 0; i < MAX; i++)

        {
        	printf("[%d] %d\n", i, arr_client[i]);
        }



        for(int i = 0; i < MAX; i++)
        {

        	if(i != socket - 4)
        	{
        	 l_len = write(arr_client[i], l_buf2, strlen(l_buf2));
        	}

        }

        sem_post(semVV);
	}


	close(socket);
	return NULL;
}



int main( int t_narg, char **t_args )
{

	for(int i = 0; i < MAX; i++)
		arr_client[i] = -1;

    if ( t_narg <= 1 ) help( t_narg, t_args );

    int l_port = 0;

    // parsing arguments
    for ( int i = 1; i < t_narg; i++ )
    {
        if ( !strcmp( t_args[ i ], "-d" ) )
            g_debug = LOG_DEBUG;

        if ( !strcmp( t_args[ i], "-h" ) )
            help( t_narg, t_args );

        if ( *t_args[ i ] != '-' && !l_port )
        {
            l_port = atoi( t_args[ i ] );
            break;
        }
    }

    if ( l_port <= 0 )
    {
        log_msg( LOG_INFO, "Bad or missing port number %d!", l_port );
        help( t_narg, t_args );
    }

    log_msg( LOG_INFO, "Server will listen on port: %d.", l_port );

    // socket creation
    int l_sock_listen = socket( AF_INET, SOCK_STREAM, 0 );
    if ( l_sock_listen == -1 )
    {
        log_msg( LOG_ERROR, "Unable to create socket.");
        exit( 1 );
    }

    in_addr l_addr_any = { INADDR_ANY };
    sockaddr_in l_srv_addr;
    l_srv_addr.sin_family = AF_INET;
    l_srv_addr.sin_port = htons( l_port );
    l_srv_addr.sin_addr = l_addr_any;

    // Enable the port number reusing
    int l_opt = 1;
    if ( setsockopt( l_sock_listen, SOL_SOCKET, SO_REUSEADDR, &l_opt, sizeof( l_opt ) ) < 0 )
      log_msg( LOG_ERROR, "Unable to set socket option!" );

    // assign port number to socket
    if ( bind( l_sock_listen, (const sockaddr * ) &l_srv_addr, sizeof( l_srv_addr ) ) < 0 )
    {
        log_msg( LOG_ERROR, "Bind failed!" );
        close( l_sock_listen );
        exit( 1 );
    }

    // listenig on set port
    if ( listen( l_sock_listen, 1 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( 1 );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );

    semVV = sem_open("/semVV", O_CREAT, 0660, 0);
    sem = sem_open("/sem", O_CREAT, 0600, 0);
    sem_init(&semVV, 1, 1);
    sem_init(&sem, 1, 1);

    // go!
    while ( 1 )
    {


                sockaddr_in l_rsa;
                int l_rsa_size = sizeof( l_rsa );
                // new connection
                int l_sock_client = accept( l_sock_listen, ( sockaddr * ) &l_rsa, ( socklen_t * ) &l_rsa_size );
                if ( l_sock_client == -1 )
                {
                        log_msg( LOG_ERROR, "Unable to accept new client." );
                        close( l_sock_listen );
                        exit( 1 );
                }

                pthread_t thread;
                int err = pthread_create(&thread, nullptr, thread_function, (void*) &l_sock_client);
                if(err)
                	log_msg(LOG_INFO, "Unabled create thread");
                else
                	log_msg(LOG_INFO, "Thread created");







    } // while ( 1 )

    return 0;
}

