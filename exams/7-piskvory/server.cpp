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
#include <sys/mman.h>

#define STR_CLOSE "close"
#define STR_QUIT "quit"
#define STR_UNLOCK "UNLOCK"
#define STR_LOCK "LOCK"

//***************************************************************************
// log messages

#define LOG_ERROR 0 // errors
#define LOG_INFO 1  // information and notifications
#define LOG_DEBUG 2 // debug messages

// debug flag
int g_debug = LOG_INFO;
sem_t *mutex_thread;
sem_t *mutex_lock[2];

#define SHM_NAME "/shm_example"

// data structure for shared memory
struct shm_data
{
    char pole[5][6];
    int thread_status[2];
};

// pointer to shared memory
shm_data *g_glb_data = nullptr;

int client_count = 0;

void log_msg(int t_log_level, const char *t_form, ...)
{
    const char *out_fmt[] = {
        "ERR: (%d-%s) %s\n",
        "INF: %s\n",
        "DEB: %s\n"};

    if (t_log_level && t_log_level > g_debug)
        return;

    char l_buf[1024];
    va_list l_arg;
    va_start(l_arg, t_form);
    vsprintf(l_buf, t_form, l_arg);
    va_end(l_arg);

    switch (t_log_level)
    {
    case LOG_INFO:
    case LOG_DEBUG:
        fprintf(stdout, out_fmt[t_log_level], l_buf);
        break;

    case LOG_ERROR:
        fprintf(stderr, out_fmt[t_log_level], errno, strerror(errno), l_buf);
        break;
    }
}

//***************************************************************************
// help

void help(int t_narg, char **t_args)
{
    if (t_narg <= 1 || !strcmp(t_args[1], "-h"))
    {
        printf(
            "\n"
            "  Socket server example.\n"
            "\n"
            "  Use: %s [-h -d] port_number\n"
            "\n"
            "    -d  debug mode \n"
            "    -h  this help\n"
            "\n",
            t_args[0]);

        exit(0);
    }

    if (!strcmp(t_args[1], "-d"))
        g_debug = LOG_DEBUG;
}

void handle_chat(int l_sock_client)
{
    int my_id = -1;

    sem_wait(mutex_thread);
    for (int i = 0; i < 2; i++)
    {
        if (g_glb_data->thread_status[i] == -1)
        {
            g_glb_data->thread_status[i] = l_sock_client;
            my_id = i;
            break;
        }
    }
    sem_post(mutex_thread);

    if (my_id == -1)
    {
        log_msg(LOG_DEBUG, "Client %d could not be added into list.", l_sock_client);
        close(l_sock_client);
        return;
    }

    pollfd walker[1];
    walker[0].fd = l_sock_client;
    walker[0].events = POLLIN;

    char my_message[50];
    sprintf(my_message, "( %d ) Request of message to send recieved\n", my_id);

    while (1)
    { // communication
        int row = 10, col = 10;
        char l_buf[128] = {0};
/*
        while (sem_trywait(mutex_lock[my_id]) != 0)
        {
            if (poll(walker, 1, 500))
            {
                char flushbfr[255];
                read(l_sock_client, flushbfr, 255);
                printf("flush!\n");
            }
        }*/
        sem_wait(mutex_lock[my_id]);

        write(l_sock_client, "jsi na tahu!\n", sizeof("jsi na tahu!\n"));

        for (int i = 0; i < 5; i++)
            write(l_sock_client, g_glb_data->pole[i], sizeof(g_glb_data->pole[i]));

        while (row >= 5 || col >= 5)
        {
            write(l_sock_client, "input turn in format CHAR-NUMBER\n", sizeof("input turn in format CHAR-NUMBER\n"));
            int l_len = read(l_sock_client, l_buf, sizeof(l_buf));
            printf("after read!\n");

            if (!l_len)
            {
                log_msg(LOG_DEBUG, "Client closed socket!");
                close(l_sock_client);
                break;
            }
            else if (l_len < 0)
                log_msg(LOG_DEBUG, "Unable to read data from client.");
            else
            {
                log_msg(LOG_DEBUG, "Read %d bytes from client.", l_len);
                l_len = write(STDOUT_FILENO, my_message, strlen(my_message));

                if (l_len < 0)
                {
                    log_msg(LOG_DEBUG, "Unable to write data to STDOUT.");
                    continue;
                }

                if (!strncasecmp(l_buf, STR_CLOSE, strlen(STR_CLOSE)))
                {
                    log_msg(LOG_INFO, "Client sent 'close' request to close connection.");
                    close(l_sock_client);
                    log_msg(LOG_INFO, "Connection closed. Waiting for new client.");
                    break;
                }

                sscanf(l_buf, "%c-%d", &col, &row);
                col -= 'A';
                row--;
                printf("inputed: %d %d\n", row, col);
            }
        }
        g_glb_data->pole[row][col] = my_id ? 'O' : 'X';
        sem_post(mutex_lock[!my_id]);
    } // while communication

    sem_wait(mutex_thread);
    g_glb_data->thread_status[my_id] = -1;
    sem_post(mutex_thread);
    close(l_sock_client);
}

//***************************************************************************

int main(int t_narg, char **t_args)
{
    printf("Before arr\n");
    if (t_narg <= 1)
        help(t_narg, t_args);

    int l_first = 0;

    int l_fd = shm_open(SHM_NAME, O_RDWR, 0660);
    if (l_fd < 0)
    {
        log_msg(LOG_ERROR, "Unable to open file for shared memory.");
        l_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0660);
        if (l_fd < 0)
        {
            log_msg(LOG_ERROR, "Unable to create file for shared memory.");
            exit(1);
        }
        ftruncate(l_fd, sizeof(shm_data));
        log_msg(LOG_INFO, "File created, this process is first");
        l_first = 1;
    }
    printf("After mem\n");

    // share memory allocation
    g_glb_data = (shm_data *)mmap(nullptr, sizeof(shm_data), PROT_READ | PROT_WRITE,
                                  MAP_SHARED, l_fd, 0);
    printf("After mem alloc\n");

    if (!g_glb_data)
    {
        log_msg(LOG_ERROR, "Unable to attach shared memory!");
        exit(1);
    }
    else
        log_msg(LOG_INFO, "Shared memory attached.");

    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 6; j++)
        {
            if (j == 5)
            {
                g_glb_data->pole[i][j] = '\n';
                continue;
            }
            g_glb_data->pole[i][j] = '.';
        }

    mutex_thread = sem_open("/Sem_vole", O_CREAT, 0660, 0);
    sem_init(mutex_thread, 1, 1);
    mutex_lock[0] = sem_open("/Sem_lock", O_CREAT, 0660, 0);
    mutex_lock[1] = sem_open("/Sem_lock2", O_CREAT, 0660, 0);
    sem_init(mutex_lock[0], 1, 1);
    sem_init(mutex_lock[1], 1, 0);
    printf("After sem init\n");

    int l_port = 0;

    for (int i = 0; i < 2; i++)
        g_glb_data->thread_status[i] = -1;
    printf("After status\n");

    // parsing arguments
    for (int i = 1; i < t_narg; i++)
    {
        if (!strcmp(t_args[i], "-d"))
            g_debug = LOG_DEBUG;

        if (!strcmp(t_args[i], "-h"))
            help(t_narg, t_args);

        if (*t_args[i] != '-' && !l_port)
        {
            l_port = atoi(t_args[i]);
            break;
        }
    }

    if (l_port <= 0)
    {
        log_msg(LOG_INFO, "Bad or missing port number %d!", l_port);
        help(t_narg, t_args);
    }

    log_msg(LOG_INFO, "Server will listen on port: %d.", l_port);

    // socket creation
    int l_sock_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (l_sock_listen == -1)
    {
        log_msg(LOG_ERROR, "Unable to create socket.");
        exit(1);
    }

    in_addr l_addr_any = {INADDR_ANY};
    sockaddr_in l_srv_addr;
    l_srv_addr.sin_family = AF_INET;
    l_srv_addr.sin_port = htons(l_port);
    l_srv_addr.sin_addr = l_addr_any;

    // Enable the port number reusing
    int l_opt = 1;
    if (setsockopt(l_sock_listen, SOL_SOCKET, SO_REUSEADDR, &l_opt, sizeof(l_opt)) < 0)
        log_msg(LOG_ERROR, "Unable to set socket option!");

    // assign port number to socket
    if (bind(l_sock_listen, (const sockaddr *)&l_srv_addr, sizeof(l_srv_addr)) < 0)
    {
        log_msg(LOG_ERROR, "Bind failed!");
        close(l_sock_listen);
        exit(1);
    }

    // listenig on set port
    if (listen(l_sock_listen, 1) < 0)
    {
        log_msg(LOG_ERROR, "Unable to listen on given port!");
        close(l_sock_listen);
        exit(1);
    }

    log_msg(LOG_INFO, "Enter 'quit' to quit server.");

    // go!
    while (1)
    {
        int l_sock_client = -1;

        // list of fd sources
        pollfd l_read_poll[2];

        l_read_poll[0].fd = STDIN_FILENO;
        l_read_poll[0].events = POLLIN;
        l_read_poll[1].fd = l_sock_listen;
        l_read_poll[1].events = POLLIN;

        while (1) // wait for new client
        {
            // select from fds
            int l_poll = poll(l_read_poll, 2, -1);

            if (l_poll < 0)
            {
                log_msg(LOG_ERROR, "Function poll failed!");
                exit(1);
            }

            if (l_read_poll[0].revents & POLLIN)
            { // data on stdin
                char buf[128];
                int len = read(STDIN_FILENO, buf, sizeof(buf));
                if (len < 0)
                {
                    log_msg(LOG_DEBUG, "Unable to read from stdin!");
                    exit(1);
                }

                log_msg(LOG_DEBUG, "Read %d bytes from stdin");
                // request to quit?
                if (!strncmp(buf, STR_QUIT, strlen(STR_QUIT)))
                {
                    log_msg(LOG_INFO, "Request to 'quit' entered.");
                    close(l_sock_listen);
                    exit(0);
                }
            }

            if (l_read_poll[1].revents & POLLIN)
            { // new client?
                sockaddr_in l_rsa;
                int l_rsa_size = sizeof(l_rsa);
                // new connection
                l_sock_client = accept(l_sock_listen, (sockaddr *)&l_rsa, (socklen_t *)&l_rsa_size);
                if (l_sock_client == -1)
                {
                    log_msg(LOG_ERROR, "Unable to accept new client.");
                    close(l_sock_listen);
                    exit(1);
                }
                uint l_lsa = sizeof(l_srv_addr);
                // my IP
                getsockname(l_sock_client, (sockaddr *)&l_srv_addr, &l_lsa);
                log_msg(LOG_INFO, "My IP: '%s'  port: %d",
                        inet_ntoa(l_srv_addr.sin_addr), ntohs(l_srv_addr.sin_port));
                // client IP
                getpeername(l_sock_client, (sockaddr *)&l_srv_addr, &l_lsa);
                log_msg(LOG_INFO, "Client IP: '%s'  port: %d",
                        inet_ntoa(l_srv_addr.sin_addr), ntohs(l_srv_addr.sin_port));

                printf("creating thread for id: %d\n", l_sock_client);

                // Adam povedal(0): pthread_t bez hvezdicky
                pthread_t tmp;
                // Adam povedal(1): pthread_t s & at dostanes pointer
                if (!fork())
                    handle_chat(l_sock_client);
            }

        } // while wait for client
        // Adam povedal(-1): Nezavirej klienta kdyz pouzivas pthread

        // change source from sock_listen to sock_client
        // l_read_poll[ 1 ].fd = l_sock_client;

    } // while ( 1 )

    return 0;
}
