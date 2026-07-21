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
#include <pthread.h>
#include <time.h>

#define PORT 9000
#define BUF_SIZE 1024
#if USE_AESD_CHAR_DEVICE
#define AESD_SOCKET_DATA_FILE "/dev/aesdchar"
#else
#define AESD_SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"
#endif

volatile sig_atomic_t terminate = 0;
volatile sig_atomic_t writeTime = 0;

typedef struct threadList {
    pthread_t thread_id;
    int client_fd;
    struct threadList *next_thread;
} threadList;

struct {
    FILE *append_fd;
    FILE *readback_fd;
} fileFDs;

pthread_mutex_t fd_lock;
threadList *threads = NULL;

void * thread_funct(void *fd);
void timer_write(int signum)
{
    if (signum == SIGALRM)
    {
        writeTime = 1;
    }
}

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
    

    pthread_mutex_init(&fd_lock, NULL);
    threadList *curThread;
    #if !USE_AESD_CHAR_DEVICE
    struct sigaction saTime;
    memset(&saTime, 0, sizeof(saTime));
    saTime.sa_handler = timer_write;
    sigemptyset(&saTime.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &saTime, NULL);
    alarm(10);
    #endif
    while (!terminate)
    {
        if (writeTime)
        {
            time_t now = time(NULL);
            struct tm tm_info;
            localtime_r(&now, &tm_info);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S %z", &tm_info);
            pthread_mutex_lock(&fd_lock);
            fileFDs.append_fd = fopen(AESD_SOCKET_DATA_FILE, "w");
            fprintf(fileFDs.append_fd, "timestamp:%s\n", timebuf);
            fflush(fileFDs.append_fd);
            pthread_mutex_unlock(&fd_lock);
            fclose(fileFDs.append_fd);
            alarm(10);
        }
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
        if (threads == NULL)
        {
            threads = malloc(sizeof(threadList));
            curThread = threads;
        }
        else 
        {
            curThread = threads;
            while (curThread->next_thread != NULL)
            {
                curThread = curThread->next_thread;
            }
            curThread->next_thread = malloc(sizeof(threadList));
            curThread = curThread->next_thread;
        }
        curThread->client_fd = client_fd;
        curThread->next_thread = NULL;
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        pthread_create(&(curThread->thread_id), NULL, thread_funct, &(curThread->client_fd));
    }
    while (threads != NULL)
    {
        curThread = threads;
        pthread_join(threads->thread_id, NULL);
        threads = threads->next_thread;
        free(curThread);
    }
    close(server_fd);
    //fclose(fileFDs.append_fd);
    //fclose(fileFDs.readback_fd);
    pthread_mutex_destroy(&fd_lock);
}

void *thread_funct(void *fd)
{
    int client_fd = *(int *)fd;
    uint8_t *datBuf = malloc(sizeof(uint8_t) * BUF_SIZE);
    uint8_t *datBuf_echo = malloc(sizeof(uint8_t) * BUF_SIZE);
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
                pthread_mutex_lock(&fd_lock);
                fileFDs.append_fd = fopen(AESD_SOCKET_DATA_FILE, "w");
                fwrite(datBuf + offset, sizeof(uint8_t), chunk_len, fileFDs.append_fd);
                fflush(fileFDs.append_fd);
                fclose(fileFDs.append_fd);
                fileFDs.readback_fd = fopen(AESD_SOCKET_DATA_FILE, "r");
                size_t bytes_read;
                while ((bytes_read = fread(datBuf_echo, sizeof(uint8_t), BUF_SIZE, fileFDs.readback_fd)) > 0)
                {
                    ssize_t total_written = 0;
                    while (total_written < (ssize_t)bytes_read)
                    {
                        ssize_t n = write(client_fd, datBuf_echo + total_written, bytes_read - total_written);
                        if (n < 0) { perror("write"); break; }
                        total_written += n;
                    }
                }
                fclose(fileFDs.readback_fd);
                pthread_mutex_unlock(&fd_lock);
                offset += chunk_len;
            }
            else
            {
                pthread_mutex_lock(&fd_lock);
                fileFDs.append_fd = fopen(AESD_SOCKET_DATA_FILE, "w");
                fwrite(datBuf + offset, sizeof(uint8_t), bytes_recv - offset, fileFDs.append_fd);
                fflush(fileFDs.append_fd);
                fclose(fileFDs.append_fd);
                pthread_mutex_unlock(&fd_lock);
                offset = bytes_recv;
            }
        }
    }
    close(client_fd);
    free(datBuf);
    free(datBuf_echo);
    return NULL;
}