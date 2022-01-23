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
#include <iostream>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string>

#define STR_CLOSE "close"
#define STR_QUIT "quit"

//***************************************************************************
// log messages

#define LOG_ERROR 0 // errors
#define LOG_INFO 1  // information and notifications
#define LOG_DEBUG 2 // debug messages
using namespace std;
// debug flag
int g_debug = LOG_INFO;

sem_t *g_sem = nullptr;

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

void func(int socket)
{

    char l_buf[256];
    char header[256] =
        "HTTP/1.1 200 OK\n"
        "Server: OSY/1.1.1 (Ubuntu)\n"
        "Accept-Ranges: bytes\n"
        "Vary: Accept-Encoding\n"
        "Content-Type: image/png\n\n";
    int l_len = read(socket, l_buf, sizeof(l_buf));

    printf("%s", l_buf);

    if ((strstr(l_buf, "HTTP") == NULL) || (strstr(l_buf, "GET") == NULL))
    {
        close(socket);
        exit(0);
    }
    l_len = write(socket, header, strlen(header));

    char command[10];
    char *arguments[10];
    char *token;
    int batteryLvl;
    char stri[50];

    memcpy(stri, &l_buf[5], 50);
    token = strtok(stri, " ");

    printf("%s\n", token);
    if (strstr(token, "/"))
    {
        printf("if good, token: %s\n", token);
        char bs[1024];
        sscanf(token, "level-%d/%s", &batteryLvl, bs);
        arguments[2] = bs;

        arguments[0] = "convert";
        arguments[1] = "-resample";
        if (batteryLvl < 10)
            arguments[3] = "battery-00.png";
        else if (batteryLvl < 30)
            arguments[3] = "battery-00.png";
        else if (batteryLvl < 50)
            arguments[3] = "battery-60.png";
        else if (batteryLvl < 70)
            arguments[3] = "battery-60.png";
        else if (batteryLvl < 90)
            arguments[3] = "battery-100.png";
        else
            arguments[3] = "battery-100.png";
        arguments[4] = "-";
        arguments[5] = NULL;
        //	for (int i=0;i<5;i++) cout << arguments[i] << endl;

        sem_wait(g_sem);
        int child = fork();
        if (child == 0)
        {
            dup2(socket, 1);
            execvp(arguments[0], arguments);
            exit(0);
        }
        sem_post(g_sem);
    }
    else{
        printf("if bad\n");
        char bs[1024] = {0};
        sscanf(token, "level-%d", &batteryLvl);

        arguments[0] = "convert";
        if (batteryLvl < 10)
            arguments[1] = "battery-00.png";
        else if (batteryLvl < 30)
            arguments[1] = "battery-00.png";
        else if (batteryLvl < 50)
            arguments[1] = "battery-60.png";
        else if (batteryLvl < 70)
            arguments[1] = "battery-60.png";
        else if (batteryLvl < 90)
            arguments[1] = "battery-100.png";
        else
            arguments[1] = "battery-100.png";
        arguments[2] = "-";
        arguments[3] = NULL;
        //	for (int i=0;i<5;i++) cout << arguments[i] << endl;

        sem_wait(g_sem);
        int child = fork();
        if (child == 0)
        {
            dup2(socket, 1);
            execvp(arguments[0], arguments);
            exit(0);
        }
        sem_post(g_sem);
    }
}

//***************************************************************************

int main(int t_narg, char **t_args)
{
    if (t_narg <= 1)
        help(t_narg, t_args);

    int l_port = 0;

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

        g_sem = sem_open("\someSemaphore", O_RDWR | O_CREAT, 0600, 1);
        sem_init(g_sem, 1, 1);

        while (1) // wait for new client
        {
            // select from fds

            sockaddr_in l_rsa;
            int l_rsa_size = sizeof(l_rsa);
            // new connection
            l_sock_client = accept(l_sock_listen, (sockaddr *)&l_rsa, (socklen_t *)&l_rsa_size);
            int pid = fork();
            if (pid == 0)
            {
                func(l_sock_client);
                exit(0);
            }

        } // while wait for client

    } // while ( 1 )

    return 0;
}
