#include "csapp.h"
#include <thread>
#include "sbuf.h"
#include <sys/time.h>

const int sbuf_size = 1;
sbuf_t sbuf[sbuf_size];
void doit(int fd);
void serve_static(int fd, char *filename, int filesize);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_filetype(char *filename,char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
int parse_uri(char *uri, char *filename, char *cgiargs);
int nrequest = 0;
sem_t mu_request;

using namespace std;
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];

    //初始化 rio 结构
    Rio_readinitb(&rio, fd);
    //读取http请求行
    ssize_t readn = Rio_readlineb(&rio, buf, MAXLINE);
    //格式化存入 把该行拆分
    if(readn == 0){
        return;//客户端关闭连接
    }
    //printf("readn=%d\nrequest line: %s\n",readn,buf);
    sscanf(buf, "%s %s %s",method, uri, version);
    //只能处理GET请求，如果不是GET请求的话返回错误
    if(strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not Implemented","Tiny does not implement thid method");
        return ;
    }

    //读取并忽略请求报头
    read_requesthdrs(&rio);

//    memset(filename,0,sizeof(filename));

    //解析 URI
    is_static = parse_uri(uri, filename, cgiargs);
    
    //文件不存在
    if(stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
        return ;
    }

    if(is_static) {     //服务静态内容
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
            return ;
        }
        serve_static(fd, filename, sbuf.st_size);
    } else {    //服务动态内容
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
            return ;
        }
        serve_dynamic(fd, filename,cgiargs);
    }
}


/*
 * 读取http 请求报头，TINY不使用请求报头的任何信息，读取之后忽略掉
 */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    //printf("%s",buf);
    //空文本行终止请求报头，碰到 空行 就结束 空行后面是内容实体
    while(strcmp(buf, "\r\n")){
        Rio_readlineb(rp, buf, MAXLINE);
      //  printf("%s",buf);
    }
    return ;
}


/*
 * 解析URI 为 filename 和 CGI 参数
 * 如果是动态内容返回0；静态内容返回 1
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    if(!strstr(uri, "cgi-bin")){  //默认可执行文件都放在cgi-bin下，这里表示没有找到
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);

        //如果是以 / 结束，则加上默认文件名 home.html
        if(uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        //printf("filename:%s\n",filename);
        return 1;   //static
    } else {   //动态内容
        char *ptr = strchr(uri,'?');
        if(ptr) {   //有参数
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {    //无参数
            strcpy(cgiargs, "");
        }

        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }

}
void handle(){
    printf("111111");
}

/*
 * 功能：发送一个HTTP响应，主体包含一个本地文件的内容
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, body[MAXBUF], filetype[MAXLINE];
    //printf("server static\n");
    /* 发送 响应行 和 响应报头 */
    get_filetype(filename, filetype);

    sprintf(body, "HTTP/1.0 200 OK\r\n");
    sprintf(body, "%sServer: Tiny Web Server\r\n",body);
    sprintf(body, "%sConnection:close\r\n",body);
    sprintf(body, "%sContent-length: %d\r\n",body, filesize);
    sprintf(body, "%sContent-type: %s\r\n\r\n",body, filetype);
    Rio_writen(fd, body, strlen(body));
    //printf("Response headers: \n%s",body);

    /* 发送响应主体 即请求文件的内容 */
    /* 只读方式发开filename文件，得到描述符*/
    srcfd = Open(filename, O_RDONLY, 0);
    /* 将srcfd 的前 filesize 个字节映射到一个从地址 srcp 开始的只读虚拟存储器区域
     * 返回被映射区的指针 */
    srcp = (char *)Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    /* 此后通过指针 srcp 操作，不需要这个描述符，所以关掉 */
    Close(srcfd);
   //printf("%d before filesize %d\n",this_thread::get_id(),filesize);
    int res = write(fd,srcp,filesize);
   // Rio_writen(fd, srcp, filesize);
  // printf("%d after res %d\n",this_thread::get_id(),res);
    /* 释放映射的虚拟存储器区域 */
    Munmap(srcp, filesize);
}


/*
 * 功能：从文件名得出文件的类型
 */
void get_filetype(char *filename,char *filetype)
{
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if(strstr(filename, ".ipg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}


/*
 * 功能：运行客户端请求的CGI程序
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE];
    char *emptylist[] = { NULL };
    
    /* 发送响应行 和 响应报头 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));


    /* 剩下的内容由CGI程序负责发送 */
    if(Fork() == 0) { //子进程
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, __environ);
    }
    Wait(NULL);
}


/*
 * 检查一些明显的错误，报告给客户端
 */
void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* 构建HTTP response 响应主体 */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor="" ffffff"">\r\n",body);
    sprintf(body, "%s%s: %s\r\n",body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n",body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n",body);

    /* 打印HTTP响应报文 */
    sprintf(buf, "HTTP/1.0 %s %s",errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n",(int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
void threads(int id){
    int co = 0;
    while(1){
        int connfd = sbuf_remove(&(sbuf[id]));
        doit(connfd);
        // P(&mu_request);
        // ++nrequest;
        // co = nrequest;
        // V(&mu_request);
        Close(connfd);
        // if((co%10000) == 0){
        //     printf("thread:%d handle %d requests\n",this_thread::get_id(),co);
        // }
    }
}
void orgin(){
   int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    signal(SIGPIPE, SIG_IGN); 
    listenfd = Open_listenfd(1024,0);
    int co = 0;
    char buf[MAXLINE];
    strcpy(buf,"hello");
    
    int id = 0;
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        doit(connfd);
        Close(connfd);
        //printf("close:%d\n",connfd);
    }
    //printf("handle %d reqeusts\n",co);
}
void version1(){
    int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    fd_set ready,read;
    struct timeval timeout;
    int fd_max,fd_num;
    // if(argc != 2){
    //     fprintf(stderr, "Usage: %s <port>\n",argv[0]);
    //     exit(1);
    // }
    
    //port = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN); 
    listenfd = Open_listenfd(1024,1);
    FD_ZERO(&read);
    FD_SET(listenfd,&read);
    //printf("11111\n");
    int co = 0;
    char buf[MAXLINE];
    strcpy(buf,"hello");
    int concurrent_count = thread::hardware_concurrency();
    printf("hadrware_concurrent:%d\n",concurrent_count);
    concurrent_count = 25;
    Sem_init(&mu_request,0,1);
    for(int i = 0;i<sbuf_size;i++){
        sbuf_init(&(sbuf[i]),100);
    }
    for(int i = 0;i < concurrent_count;i++){
        thread t(threads,i%sbuf_size);
        t.detach();
    }
    fd_max = listenfd;
    int id = 0;
    while(1){
        ready = read;
        timeout.tv_sec=0;
        timeout.tv_usec=5000;
        if((fd_num = select(fd_max+1,&ready,0,0,&timeout)) == -1)
            break;
        if(fd_num == 0)
            continue;
        
        if(FD_ISSET(listenfd,&ready)){
           clientlen = sizeof(clientaddr);
            while(1){
                ++co;
                connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);
                if(connfd < 0){
                    if(errno == EWOULDBLOCK) break;
                    else {
                        printf("connect error!\n");
                        return;
                        }
                    }
                    sbuf_insert(&(sbuf[connfd%sbuf_size]),connfd);
                }
        }
    
    }
    //printf("handle %d reqeusts\n",co);
}
int count[sbuf_size];
int h[25];
int main(int argc, char **argv)
{
    version1();
}
