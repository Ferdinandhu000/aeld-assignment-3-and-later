#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define CHUNK_SIZE 1024

static volatile sig_atomic_t caught_signal = 0;

static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        caught_signal = 1;
    }
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') {
            daemon_mode = true;
        }
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error setting up SIGINT handler: %m");
        closelog();
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error setting up SIGTERM handler: %m");
        closelog();
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));
        closelog();
        return -1;
    }

    int server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (server_fd == -1) {
        syslog(LOG_ERR, "socket creation failed: %m");
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed: %m");
        close(server_fd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
        syslog(LOG_ERR, "bind failed: %m");
        close(server_fd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    freeaddrinfo(servinfo);

    if (listen(server_fd, 10) != 0) {
        syslog(LOG_ERR, "listen failed: %m");
        close(server_fd);
        closelog();
        return -1;
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "fork failed: %m");
            close(server_fd);
            closelog();
            return -1;
        }
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0) {
            syslog(LOG_ERR, "setsid failed: %m");
            close(server_fd);
            closelog();
            return -1;
        }

        if (chdir("/") < 0) {
            syslog(LOG_ERR, "chdir failed: %m");
        }

        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null != -1) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            if (dev_null > 2) {
                close(dev_null);
            }
        }
    }

    int client_fd = -1;

    while (!caught_signal) {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (errno == EINTR || caught_signal) {
                break;
            }
            syslog(LOG_ERR, "accept failed: %m");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        struct sockaddr_in *p = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &(p->sin_addr), client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char recv_chunk[CHUNK_SIZE];
        char *packet_buf = NULL;
        size_t packet_len = 0;

        while (!caught_signal) {
            ssize_t bytes_recv = recv(client_fd, recv_chunk, sizeof(recv_chunk), 0);
            if (bytes_recv > 0) {
                char *temp = realloc(packet_buf, packet_len + bytes_recv);
                if (!temp) {
                    syslog(LOG_ERR, "Memory allocation error for packet buffer");
                    free(packet_buf);
                    packet_buf = NULL;
                    packet_len = 0;
                    break;
                }
                packet_buf = temp;
                memcpy(packet_buf + packet_len, recv_chunk, bytes_recv);
                packet_len += bytes_recv;

                char *newline_ptr;
                while ((newline_ptr = memchr(packet_buf, '\n', packet_len)) != NULL) {
                    size_t single_packet_len = (size_t)(newline_ptr - packet_buf + 1);

                    int data_fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (data_fd == -1) {
                        syslog(LOG_ERR, "open data file failed: %m");
                    } else {
                        size_t written_total = 0;
                        while (written_total < single_packet_len) {
                            ssize_t w = write(data_fd, packet_buf + written_total, single_packet_len - written_total);
                            if (w < 0) {
                                if (errno == EINTR) {
                                    continue;
                                }
                                syslog(LOG_ERR, "write to data file failed: %m");
                                break;
                            }
                            written_total += (size_t)w;
                        }
                        close(data_fd);
                    }

                    data_fd = open(DATA_FILE, O_RDONLY);
                    if (data_fd != -1) {
                        char send_chunk[CHUNK_SIZE];
                        ssize_t bytes_read;
                        while ((bytes_read = read(data_fd, send_chunk, sizeof(send_chunk))) > 0) {
                            size_t sent_total = 0;
                            while (sent_total < (size_t)bytes_read) {
                                ssize_t s = send(client_fd, send_chunk + sent_total, (size_t)bytes_read - sent_total, MSG_NOSIGNAL);
                                if (s < 0) {
                                    if (errno == EINTR) {
                                        continue;
                                    }
                                    syslog(LOG_ERR, "send to client failed: %m");
                                    break;
                                }
                                sent_total += (size_t)s;
                            }
                            if (sent_total < (size_t)bytes_read) {
                                break;
                            }
                        }
                        close(data_fd);
                    }

                    size_t remaining = packet_len - single_packet_len;
                    memmove(packet_buf, packet_buf + single_packet_len, remaining);
                    packet_len = remaining;
                }
            } else if (bytes_recv == 0) {
                break;
            } else {
                if (errno == EINTR) {
                    if (caught_signal) {
                        break;
                    }
                    continue;
                }
                syslog(LOG_ERR, "recv error: %m");
                break;
            }
        }

        if (packet_buf != NULL) {
            free(packet_buf);
            packet_buf = NULL;
            packet_len = 0;
        }

        close(client_fd);
        client_fd = -1;
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    unlink(DATA_FILE);
    closelog();

    return 0;
}