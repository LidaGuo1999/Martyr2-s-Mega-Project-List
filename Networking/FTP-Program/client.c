#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>


#define USER "USER "
#define PASS "PASS "
#define ACCT "ACCT "
#define CWD "CWD "
#define CDUP "CDUP "
#define RETR "RETR "
#define STOR "STOR "
#define PASV "PASV\r\n"
#define LIST "LIST\r\n"
#define QUIT "QUIT\r\n"

#define reply_220 "220"
#define reply_221 "221"
#define reply_226 "226"
#define reply_230 "230"
#define reply_331 "331"

#define log_SUCCESS 1
#define log_ACTION 2
#define log_RECEIVE 3
#define log_ERROR 5

typedef struct {
    char *ip;
    int port;
} ip_port;

// Global variables
char sub_ip[20];
int sub_port = 0;
int rwFlag = 0; // 0代表非读写命令，1代表从服务器读，2代表向服务器写
FILE *writeToLocal;
FILE *readFromLocal;

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

int check_reply(char *reply, char *target) {
    if (reply[0] == target[0] && reply[1] == target[1] && reply[2] == target[2]) {
        return 1;
    }
    return 0;
}

void splitStringBySpace(char *s, char *rtn[]) {
    int ri = 0, length = strlen(s)-2;
    rtn[ri++] = s;
    for (int i = 0; i < length; i++) {
        if (s[i] == ' ') {
            rtn[ri++] = s+i+1;
            s[i] = '\0';
        }
        else continue;
    }
    return ;
}

void *data_channel(void *ipp) {
    int sockfd;
    struct sockaddr_in data_addr;
    struct hostent *data_host;
    char input_buffer[256], recv_buffer[256];
    char send_buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logger(log_ERROR, "Fail when creating sockfd in data_channel.\n");
        return NULL;
    }

    data_host = gethostbyname(((ip_port *)ipp)->ip);
    if (data_host == NULL) {
        logger(log_ERROR, "No such host in data channel.\n");
        return NULL;
    }

    bzero((char *)&data_addr, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr = *((struct in_addr *)data_host->h_addr);
    data_addr.sin_port = htons(((ip_port *)ipp)->port);

    if (connect(sockfd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        logger(log_ERROR, "connecting in data channel.\n");
    }

    logger(log_SUCCESS, "Connected to FTP server data port.\n");
    bzero((char *)recv_buffer, sizeof(recv_buffer));
    bzero((char *)send_buffer, sizeof(send_buffer));
    while (1) {
        if (rwFlag == 0) {
            if (recv(sockfd, recv_buffer, sizeof(recv_buffer), 0) > 0) {
                //logger(log_RECEIVE, "\n");
                printf("%s", recv_buffer);
            } else break;
        } else if (rwFlag == 1) {
            if (recv(sockfd, recv_buffer, sizeof(recv_buffer), 0) > 0) {
                fwrite(recv_buffer, sizeof(char), strlen(recv_buffer), writeToLocal);
            } else break;
            
        } else if (rwFlag == 2) {
            if (fread(send_buffer, sizeof(char), sizeof(send_buffer), readFromLocal) > 0) {
                send(sockfd, send_buffer, strlen(send_buffer), 0);
                bzero((char *)send_buffer, sizeof(send_buffer));
            } else {
                shutdown(sockfd, SHUT_RDWR);
                break;
            }
        }
        bzero((char *)recv_buffer, sizeof(recv_buffer));
        bzero((char *)send_buffer, sizeof(send_buffer));
    }
    //shutdown(sockfd, SHUT_RDWR);
    
    return NULL;
}

void handle_PORT(char *str, char *h, int *p) {
    // PORT内容格式为(h1,h2,h3,h4,p1,p2)
    int flag = 0;   // 0代表尚未找到左括号，1代表在括号内找ip地址，2代表已经在括号内找端口号，3代表已经找到右括号，该终止了
    int j = 0;
    char *p1 = (char *)malloc(5), *p2 = (char *)malloc(5);
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] != '(') continue;
        else {
            flag = 1;
            int comma_cnt = 0;
            while (++i < strlen(str)) {
                if (str[i] >= '0' && str[i] <= '9') h[j++] = str[i];
                else {
                    h[j++] = '.';
                    comma_cnt++;
                }
                if (comma_cnt >= 4) {
                    i++;
                    h[j-1] = '\0';
                    break;
                }
            }

            flag = 2, j = 0;
            while (str[i] >= '0' && str[i] <= '9') {
                p1[j++] = str[i++];
            }
            i++;
            j = 0;
            while (str[i] >= '0' && str[i] <= '9') {
                p2[j++] = str[i++];
            }

            if (str[i] == ')') flag = 3;
        }
    }

    int intp1 = atoi(p1), intp2 = atoi(p2);
    *p = (intp1 << 8) | intp2;

    return ;
}

void handle_usercommand(int sockfd, char *send_buffer, char *recv_buffer, char *command) {
    char *rtn[7]; // 最多支持7个参数
    bzero((char *)rtn, sizeof(rtn));
    splitStringBySpace(command, rtn);
    //for (int i = 0; rtn[i] != 0; i++) printf("com: %s\n", rtn[i]);

    char *com_name = rtn[0];

    if (strcmp("list\n", command) == 0) {
        rwFlag = 0;
        strcpy(send_buffer, LIST);
        send(sockfd, send_buffer, strlen(send_buffer), 0);

        pthread_t sub_data;
        ip_port data_ipp;
        data_ipp.ip = sub_ip;
        data_ipp.port = sub_port;

        pthread_create(&sub_data, NULL, (void *)data_channel, (void *)&data_ipp);
        pthread_join(sub_data, NULL);

        bzero((char *)recv_buffer, 256);
        while (recv(sockfd, recv_buffer, 256, 0)) {
            if (check_reply(recv_buffer, reply_226)) break;
            bzero((char *)recv_buffer, 256);
        }
        bzero((char *)recv_buffer, 256);
        
    } else if (strcmp("quit\n", command) == 0) {
        rwFlag = 0;
        strcpy(send_buffer, QUIT);
        send(sockfd, send_buffer, strlen(send_buffer), 0);
        bzero((char *)recv_buffer, 256);
        recv(sockfd, recv_buffer, 256, 0);
        if (check_reply(recv_buffer, reply_221)) {
            logger(log_SUCCESS, "Disconnect from FTP server.\n");
            exit(0);
        }
    } else if (strcmp("retr", com_name) == 0) {
        bzero((char *)send_buffer, 256);
        strcpy(send_buffer, RETR);
        strcat(send_buffer, rtn[1]);
        strcat(send_buffer, "\r\n");
        send(sockfd, send_buffer, strlen(send_buffer), 0);

        writeToLocal = fopen(rtn[2], "w");
        rwFlag = 1;

        pthread_t sub_data;
        ip_port data_ipp;
        data_ipp.ip = sub_ip;
        data_ipp.port = sub_port;

        pthread_create(&sub_data, NULL, (void *)data_channel, (void *)&data_ipp);
        pthread_join(sub_data, NULL);

        bzero((char *)recv_buffer, 256);
        while (recv(sockfd, recv_buffer, 256, 0)) {
            if (check_reply(recv_buffer, reply_226)) break;
            bzero((char *)recv_buffer, 256);
        }
        bzero((char *)recv_buffer, 256);
        fclose(writeToLocal);
    } else if (strcmp("stor", com_name) == 0) {
        bzero((char *)send_buffer, 256);
        strcpy(send_buffer, STOR);
        strcat(send_buffer, rtn[1]);
        strcat(send_buffer, "\r\n");
        send(sockfd, send_buffer, strlen(send_buffer), 0);

        readFromLocal = fopen(rtn[2], "r");
        rwFlag = 2;

        pthread_t sub_data;
        ip_port data_ipp;
        data_ipp.ip = sub_ip;
        data_ipp.port = sub_port;

        pthread_create(&sub_data, NULL, (void *)data_channel, (void *)&data_ipp);
        pthread_join(sub_data, NULL);

        bzero((char *)recv_buffer, 256);
        while (recv(sockfd, recv_buffer, 256, 0)) {
            if (check_reply(recv_buffer, reply_226)) break;
            bzero((char *)recv_buffer, 256);
        }
        bzero((char *)recv_buffer, 256);
        fclose(readFromLocal);
    }
}

int main(int argc, char *argv[]) {
    int mode = 1;   // 1代表被动模式，0代表主动模式
    int sockfd, serv_port;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char input_buffer[256], recv_buffer[256];
    char *send_buffer;
    memset(input_buffer, 0, sizeof(input_buffer));
    memset(recv_buffer, 0, sizeof(recv_buffer));
    if (argc < 2) {
        fprintf(stderr, "usage: %s hostname\n", argv[0]);
        exit(0);
    }

    serv_port = 21; // FTP服务器端命令连接端口号
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logger(log_ERROR, "opening socket.");
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        logger(log_ERROR, "no such host.");
    }
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    //bcopy(*((struct in_addr *)server->h_addr), (char *)serv_addr.sin_addr, server->h_length);
    serv_addr.sin_port = htons(serv_port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        logger(log_ERROR, "connecting.");
    }

    logger(log_SUCCESS, "Connected to FTP server.\n");
    recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
    if (check_reply(recv_buffer, reply_220) == 0) {
        //printf("Receive from FTP server: %s", recv_buffer);
        logger(log_ERROR, "FTP server is not welcoming you.\n");
    }
    
    logger(log_ACTION, "Input your username: ");
    fgets(input_buffer, 256, stdin);
    //strcat(input_buffer, "\r\n");
    send_buffer = (char *) malloc(256);
    strcpy(send_buffer, USER);
    strcat(send_buffer, input_buffer);
    send(sockfd, send_buffer, strlen(send_buffer), 0);
    
    recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
    if (check_reply(recv_buffer, reply_331) > 0) {
        logger(log_ACTION, "Input your password: ");
        system("stty -echo");
        fgets(input_buffer, 256, stdin);
        strcpy(send_buffer, PASS);
        strcat(send_buffer, input_buffer);
        send(sockfd, send_buffer, strlen(send_buffer), 0);
        system("stty echo");
        printf("\n");
    } else {
        logger(log_ERROR, "wrong username.\n");
    }

    recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
    if (check_reply(recv_buffer, reply_230) > 0) {
        logger(log_SUCCESS, "Login successful.\n");
    } else {
        logger(log_ERROR, "wrong password.\n");
    }

    if (mode == 1) {
        // 接收用户的一系列命令
        while (1) {
            bzero((char *)input_buffer, sizeof(input_buffer));
            logger(log_ACTION, "Input your command: ");
            fgets(input_buffer, 256, stdin);
            strcpy(send_buffer, PASV);
            send(sockfd, send_buffer, strlen(send_buffer), 0);
            //logger(log_SUCCESS, "PASV command has been sent.\n");
            bzero((char *)recv_buffer, sizeof(recv_buffer));
            recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
            //printf("%s\n", recv_buffer);
            handle_PORT(recv_buffer, sub_ip, &sub_port);
            
            //logger(log_SUCCESS, "FTP data channel ip and port received.\n");
            
            
            handle_usercommand(sockfd, send_buffer, recv_buffer, input_buffer);

            
        }
        
    }

    

    return 0;
}