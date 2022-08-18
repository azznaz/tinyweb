#include "csapp.h"

int main(){
    int clientfd;
    clientfd = Open_clientfd("127.0.0.1",1024);
    char buf[MAXLINE];
    strcpy(buf,"hello server.");
   // Rio_writen(clientfd,buf,strlen(buf));
    printf("%d\n",clientfd);
    close(clientfd);
}