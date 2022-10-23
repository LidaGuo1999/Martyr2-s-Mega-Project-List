#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>

#define log_SUCCESS 1
#define log_ACTION 2
#define log_RECEIVE 3
#define log_ERROR 5

void logger(int flag, char *msg) {
    switch (flag)
    {
    case log_SUCCESS:
        printf("[SUCCESS] ");
        break;

    case log_ACTION:
        printf("[ACTION NEEDED] ");
        break;

    case log_ERROR:
        printf("[ERROR] ");
        break;

    case log_RECEIVE:
        printf("[RECEIVE] ");
        break;
    
    default:
        break;
    }

    printf("%s", msg);
    if (flag == log_ERROR) exit(0);

    return ;
}

int main(int argc, char *argv[]) {
    int sockfd = 0;
    struct sockaddr_in targetAddr;
    struct hostent *target;
    char input_buffer[256] = {0};
    int portStart = 0, portEnd = 0;

    struct pollfd pfds[1];
    pfds[0].events = POLLOUT;

    // 让用户输入主机地址或域名
    printf("Please input IP address you want to scan: ");
    fgets(input_buffer, sizeof(input_buffer), stdin);
    if (strlen(input_buffer) > 1) input_buffer[strlen(input_buffer)-1] = '\0';
    printf("Please clarify port range: (seperate with a space) ");
    scanf("%d %d", &portStart, &portEnd);

    target = gethostbyname(input_buffer);
    if (target == NULL) {
        logger(log_ERROR, "No such host.\n");
    }
    bzero((char *)&targetAddr, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_addr = *((struct in_addr *)target->h_addr);

    for (int p = portStart; p < portEnd; p++) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
        
        if (sockfd < 0) {
            logger(log_ERROR, "Cannot open socket.\n");
        }
        pfds[0].fd = sockfd;

        targetAddr.sin_port = htons(p);
        connect(sockfd, (struct sockaddr*)&targetAddr, sizeof(targetAddr));

        int num_events = poll(pfds, 1, 1000);
        if (num_events > 0) {
            printf("Port %d is open.\n", p);
        }
    }

    return 0;
}