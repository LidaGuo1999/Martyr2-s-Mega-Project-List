#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define LI 0
#define VN 4
#define MODE 3
#define STRATUM 0
#define POLL 6
#define PRECISION ((signed int)-18)
#define GAP70 2208988800ull   //1900年到1970年的时间差

#define log_SUCCESS 1
#define log_ACTION 2
#define log_RECEIVE 3
#define log_ERROR 5

#define uint32 unsigned int
#define uint64 unsigned long long

typedef struct {
    uint32 meta;
    uint32 rootDelay;
    uint32 rootDispersion;
    uint32 refID;

    uint64 refTs;
    uint64 oriTs;
    uint64 ReceiveTs;
    uint32 TransTs_s;
    uint32 TransTs_ms;
} ntp_packet;

time_t atomicTime;

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

void sendRequest(int sockfd) {
    // 向服务器发送请求时间的request
    ntp_packet sendPack, receivePack;
    memset(&sendPack, 0, sizeof(sendPack));
    memset(&receivePack, 0, sizeof(receivePack));

    sendPack.meta = htonl((LI << 30) | (VN << 27) | (MODE << 24));

    send(sockfd, (char *)&sendPack, sizeof(sendPack), 0);
    recv(sockfd, (char *)&receivePack, sizeof(receivePack), 0);
    receivePack.TransTs_s = ntohl(receivePack.TransTs_s);
    receivePack.TransTs_ms = ntohl(receivePack.TransTs_ms);

    atomicTime = (time_t)(receivePack.TransTs_s - GAP70);
    logger(log_RECEIVE, "\n");
    printf("NTP time: %s\n", ctime((const time_t *)&atomicTime));

    return ;
}

int main(int argc, char *argv[]) {
    char input_buffer[256] = {0};
    char *server_name = "ntp.aliyun.com";
    int sockfd = 0, ntp_port = 123;
    struct sockaddr_in servAddr;
    struct hostent *serverHost;

    printf("Welcome to use Lida NTP Service.\n");
    printf("Please input NTP server address: (Press Enter to user default servers) ");
    fgets(input_buffer, sizeof(input_buffer), stdin);
    if (strlen(input_buffer) > 1) {
        input_buffer[strlen(input_buffer)-1] = '\0';
    } else {
        strcpy(input_buffer, server_name);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        logger(log_ERROR, "Cannot open a new socket file discription.\n");
    }

    serverHost = gethostbyname(input_buffer);
    if (serverHost == NULL) {
        logger(log_ERROR, "No such host.\n");
    }

    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr = *((struct in_addr *)serverHost->h_addr);
    servAddr.sin_port = htons(ntp_port);

    if (connect(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        logger(log_ERROR, "Can't connect to host.\n");
    }

    sendRequest(sockfd);
}