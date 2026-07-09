#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 9000
#define BUF_SIZE 1024

volatile sig_atomic_t terminate = 0;

static void handler(int signum)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    terminate = 1;
}

int main(int argc, char *argv[])
{
    int opt;
    int daemon = 0;
    while ((opt = getopt(argc, argv, "d")) != -1) 
    {
        switch (opt) {
            case 'd':
                daemon = 1;
                break;
            case '?':
                return 1;
        }
    }
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    int server_fd;
    struct sockaddr_in server_addr;
    
    // create socket fd
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //set sock options
    int opt_val = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (daemon)
    {
        pid_t child = fork();
        if (child < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (child > 0)
        {
            exit(EXIT_SUCCESS);
        }
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //open fds
    FILE *append_fd;
    FILE *readback_fd;
    append_fd = fopen("/var/tmp/aesdsocketdata", "w");
    readback_fd = fopen("/var/tmp/aesdsocketdata", "r");
    uint8_t *datBuf = malloc(sizeof(uint8_t) * BUF_SIZE);
    uint8_t *datBuf_echo = malloc(sizeof(uint8_t) * BUF_SIZE);
    while (!terminate)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (terminate)
            {
                break;
            }
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        ssize_t bytes_recv;
        while ((bytes_recv = read(client_fd, datBuf, BUF_SIZE)) > 0)
{
    ssize_t offset = 0;

    while (offset < bytes_recv)
    {
        uint8_t *nl = memchr(datBuf + offset, '\n', bytes_recv - offset);

        if (nl != NULL)
        {
            size_t chunk_len = (nl - (datBuf + offset)) + 1; // include the '\n'
            fwrite(datBuf + offset, sizeof(uint8_t), chunk_len, append_fd);
            fflush(append_fd);

            fseek(readback_fd, 0, SEEK_SET);
            size_t bytes_read;
            while ((bytes_read = fread(datBuf_echo, sizeof(uint8_t), BUF_SIZE, readback_fd)) > 0)
            {
                ssize_t total_written = 0;
                while (total_written < (ssize_t)bytes_read)
                {
                    ssize_t n = write(client_fd, datBuf_echo + total_written, bytes_read - total_written);
                    if (n < 0) { perror("write"); break; }
                    total_written += n;
                }
            }

            offset += chunk_len;
        }
        else
        {
            fwrite(datBuf + offset, sizeof(uint8_t), bytes_recv - offset, append_fd);
            fflush(append_fd);
            offset = bytes_recv;
        }
    }
}
        close(client_fd);
    }
    close(server_fd);
    fclose(append_fd);
    fclose(readback_fd);
    free(datBuf);
    free(datBuf_echo);
}

