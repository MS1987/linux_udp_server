#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <ctype.h>
#include <sys/ioctl.h>
 
#include <net/if.h> // struct ifreq
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>



#define UDP_SERVER_PORT 8989
#define HTTP_SERVER_PORT 8990
#define BUFF_LEN 1024

#define DEV_NAME_PATH "./dev_info.txt"
#define SAVE_IP_PATH "/home/mks/gcode_files/sda1/"
#define SAVE_IP_FILE "ip.txt"

char module_name[100] = "MKS";



char *rtrim(char *str)
{
    if (str == NULL || *str == '\0')
    {
        return str;
    }
    int len = strlen(str);
    char *p = str + len - 1;
    while (p >= str && isspace(*p))
    {
        *p = '\0'; --p;
    }
    return str;
}


//去除首部空格
char *ltrim(char *str)
{
    if (str == NULL || *str == '\0')
    {
        return str;
    }
    int len = 0;
    char *p = str;
    while (*p != '\0' && isspace(*p))
    {
        ++p; ++len;
    }
    memmove(str, p, strlen(str) - len + 1);
    return str;
}


char *trim(char *str)
{
    str = rtrim(str);
    str = ltrim(str);
    return str;
}


#if 0
int get_local_ip(char *ip)
{

	int fd, intrface, retn = 0;

	struct ifreq buf[INET_ADDRSTRLEN];

	struct ifconf ifc;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{

			ifc.ifc_len = sizeof(buf);

			// caddr_t,linux内核源码里定义的：typedef void *caddr_t；

			ifc.ifc_buf = (caddr_t)buf;

			if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
			{

					intrface = ifc.ifc_len/sizeof(struct ifreq);

					while (intrface-- > 0)
					{

							if (!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface])))
							{

									ip=(inet_ntoa(((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr));

									printf("IP:%s\n", ip);
									printf("IP2:%d\n", (((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr[0]));
									printf("IP3:%d\n", (((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr[1]));
									printf("IP4:%d\n", (((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr[2]));
									printf("IP5:%d\n", (((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr[3]));
									
									if(strstr(ip, "127.0.0.1") == 0)
										break;

							}

					}

			}

		close(fd);

		return 0;

	}

}
#endif

int get_local_ip(char *ip) {

        struct ifaddrs *ifAddrStruct;

        void *tmpAddrPtr=NULL;

        getifaddrs(&ifAddrStruct);

        while (ifAddrStruct != NULL) {

                if (ifAddrStruct->ifa_addr->sa_family==AF_INET) {

                        tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;

                        inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
                        printf("%s IP Address:%s\n", ifAddrStruct->ifa_name, ip);
						
						if(strstr(ifAddrStruct->ifa_name, "eth") || strstr(ifAddrStruct->ifa_name, "wlan"))
							break;

                }  
				
				ifAddrStruct=ifAddrStruct->ifa_next;

        }

        //free ifaddrs

		if(ifAddrStruct == NULL)
			freeifaddrs(ifAddrStruct);

        return 0;

}

 
int get_mac(unsigned char binMAC[6])
{
  int sock;
  struct ifreq ifr;
  unsigned char *puc;
  memset(binMAC, 0, 6);
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("socket");
    return -1;
  }
  strcpy(ifr.ifr_name, "eth0"); 
  if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
  {
    close(sock);
    perror("ioctl");
    return -1;
  }
  puc = ifr.ifr_hwaddr.sa_data;
  close(sock);
  memcpy(binMAC, puc, 6);
  printf("get mac:");
  for(int i = 0; i < 6; i++)
	printf("0x%x,", binMAC[i]);
  return 0;
}

char *my_strupr(char * in_str)
{
	int i;
	int len = strlen(in_str);
	for(i < 0; i < len; i++)
	{
		if((*(in_str + i) >= 'a') && (*(in_str + i) <= 'x'))
			*(in_str + i) = (*(in_str + i)) - 32;
	}	
	return in_str;
}



/*
    server:
            socket-->bind-->recvfrom-->sendto-->close
*/

int udp_server_fd;

/*线程工作函数*/
void *thread_udp_func(void *argv)
{
	
	char buf[BUFF_LEN];  //接收缓冲区，1024字节
	char  ReplyBuffer[100] = "mkswifi:";
	char file_buf[100] = {0};
    socklen_t len;
    int count;
	char moduleIdBytes[21] = {0};
	char *moduleId;
	unsigned char mac[6] = {0};
	char ipaddr[64];
    struct sockaddr_in clent_addr;  //clent_addr用于记录发送方的地址信息
	char *p;
	
	get_mac(mac);
	sprintf(moduleIdBytes, "HJNLM000%x%x%x%x%x%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	moduleId = my_strupr(moduleIdBytes);
	printf("Get mac: %s\n", moduleId);
	
	

	

	
	
    while(1)
    {
		
		
        memset(buf, 0, BUFF_LEN);
        len = sizeof(clent_addr);
        count = recvfrom(udp_server_fd, buf, BUFF_LEN, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        if(count == -1)
        {
            printf("recieve data fail!\n");
            return;
        }
		
		//收到mks wifi的搜索信息
		if(strstr(buf, "mkswifi") != 0)
		{
			int fd=open(DEV_NAME_PATH, 2);
			if(fd>=0)
			{			
				read(fd,file_buf,100);
				
				p = trim(file_buf);
				memset(module_name, 0, sizeof(module_name));
				strcpy(module_name, p);
				close(fd);
			}
			memset(ipaddr, 0, sizeof(ipaddr));
			get_local_ip(ipaddr);
			//printf("Get ip addr:%s\n", ipaddr);	
				
			memcpy(&ReplyBuffer[strlen("mkswifi:")], module_name, strlen(module_name)); 
			ReplyBuffer[strlen("mkswifi:") + strlen(module_name)] = ',';
			
			memcpy(&ReplyBuffer[strlen("mkswifi:")+ strlen(module_name) + 1], moduleId, strlen(moduleId)); 
			ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + 1] = ',';
			
			strcpy(&ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + 2], ipaddr); 
			ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + strlen(ipaddr) + 2] = '\n';
			
		}
	
        printf("client:%s\n",buf);  //打印client发过来的信息
        memset(buf, 0, BUFF_LEN);
      //  sprintf(buf, "I have recieved %d bytes data!\n", count);  //回复client
      //  printf("server:%s\n",buf);  //打印自己发送的信息给
        sendto(udp_server_fd, ReplyBuffer, sizeof(ReplyBuffer), 0, (struct sockaddr*)&clent_addr, len);  //发送信息给client，注意使用了clent_addr结构体指针
		usleep(500);
    }

    close(udp_server_fd);	

    //退出线程
    pthread_exit(NULL);
}


int udp_event()
{
	int ret;
    struct sockaddr_in ser_addr; 
	pthread_t thread_id;

    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(udp_server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(UDP_SERVER_PORT);  //端口号，需要网络序转换

    ret = bind(udp_server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        return -1;
    }
	
	/*创建线程*/
	if(pthread_create(&thread_id,NULL,thread_udp_func,NULL))
	{
		printf("线程创建失败.\n");
		return -1;
	}
	/*设置线程的分离属性*/
	pthread_detach(thread_id);


    
}



void
response_200(int cfd)
{
    // 打印返回200的报文头
    const char* str = "HTTP/1.0 200 OK\r\n"
    "Server: wz simple httpd 1.0\r\n"
    "Content-Type: text/html\r\n"
    "\r\n";

    write(cfd, str, strlen(str));
}

/*
函数功能: 服务器向客户端发送响应数据
*/
int HTTP_ServerSendFile(int client_fd,char *buff,char *type,char *file)
{
        char file_buf[100]={0};
        int cnt;
        char *p;
        int dev_name_len;
    /*1. 打开文件*/
    int fd=open(file,2);
    if(fd<0)return -1;
    /*2. 获取文件大小*/

    /*3. 构建响应头部*/
        cnt=read(fd,file_buf,100);
        printf("cnt:%d\n", cnt);
        p = trim(file_buf);
        dev_name_len = strlen(p);
        printf("dev_name_len:%d\n", dev_name_len);

        printf("File content:%s\n", p);
    sprintf(buff,"HTTP/1.1 200 OK\r\n"
                "Content-type:%s\r\n"
                "Content-Length:%d\r\n"
                "\r\n{\r\n\"result\": {\"dev_name\": %s}\r\n}",type, dev_name_len + strlen("{\r\n\"result\": {\"dev_name\": %}\r\n}"), p);
    /*4. 发送响应头*/
    if(write(client_fd,buff,strlen(buff))!=strlen(buff))
		return -2;
    /*5. 发送消息正文*/

        /*
    while(1)
    {
        cnt=read(fd,buff,1024);
        if(write(client_fd,buff,cnt)!=cnt)return -3;
        if(cnt!=1024)break;
    }*/
        close(fd);
    return 0;
}

int HTTP_ServerSaveToFile(int client_fd,char *buff,char *type,char *file)
{
//      char file_buf[100]={0};
        int cnt;
        char *p;
        int dev_name_len;
        char dev_name[100]={0};

        p = trim(buff);
        printf("buff:%s\n", p);
        char* name_index = strstr(p, "name=");
        printf("name_Index:%s\n", name_index);
        if((int)name_index == 0)
                return -3;
        char * end_index = strstr((char *)name_index, " ");
        printf("end:%s\n", end_index);
       printf("diff:%d\n", (int)end_index - (int)name_index );
        if((int)end_index == 0)
                return -3;

                //printf("nameindex:%s, end_index:%s\n", name_index, end_index);
        strncpy(dev_name, (name_index + 5), (int)end_index - (int)name_index - 5);


        printf("write dev name:%s\n", dev_name);

         //int fd=open(file,2);
		 int fd=open(file, O_CREAT | O_RDWR, 0777 );
          if(fd<0)return -1;

        ftruncate(fd,0);

            /* 重新设置文件偏移量 */
            lseek(fd,0,SEEK_SET);

        write(fd, dev_name, strlen(dev_name));

        response_200(client_fd);

        close(fd);

}


/*线程工作函数*/
void *thread_work_func(void *argv)
{
    int client_fd=*(int*)argv;
    free(argv);

    unsigned int cnt;
    unsigned char buff[1024];
    //读取浏览器发送过来的数据
    cnt=read(client_fd,buff,1024);
    buff[cnt]='\0';
    printf("%s\n",buff);

    if(strstr(buff,"GET /printer/dev_name HTTP/1.1"))
    {
        HTTP_ServerSendFile(client_fd,buff,"application/json",DEV_NAME_PATH);
    }

        else if(strstr(buff,"GET /printer/dev_name?name="))
    {
        HTTP_ServerSaveToFile(client_fd,buff,"text/plain", DEV_NAME_PATH);
//      printf("receive\n");
    }


    close(client_fd);
    //退出线程
    pthread_exit(NULL);
}

void http_event()
{
	int sockfd;
    /*1. 创建socket套接字*/
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /*2. 绑定端口号与IP地址*/
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(HTTP_SERVER_PORT); // 端口号0~65535
    addr.sin_addr.s_addr=INADDR_ANY;    //inet_addr("0.0.0.0"); //IP地址
    if(bind(sockfd,(const struct sockaddr *)&addr,sizeof(struct sockaddr))!=0)
    {
        printf("服务器:端口号绑定失败.\n");
    }
    /*3. 设置监听的数量,表示服务器同一时间最大能够处理的连接数量*/
    listen(sockfd,20);

    /*4. 等待客户端连接*/
    int *client_fd;
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    pthread_t thread_id;
    while(1)
    {
        addrlen=sizeof(struct sockaddr_in);
        client_fd=malloc(sizeof(int));
        *client_fd=accept(sockfd,(struct sockaddr *)&client_addr,&addrlen);
        if(*client_fd<0)
        {
            printf("客户端连接失败.\n");
            return 0;
        }
        printf("连接的客户端IP地址:%s\n",inet_ntoa(client_addr.sin_addr));
        printf("连接的客户端端口号:%d\n",ntohs(client_addr.sin_port));

        /*创建线程*/
        if(pthread_create(&thread_id,NULL,thread_work_func,client_fd))
        {
            printf("线程创建失败.\n");
            break;
        }
        /*设置线程的分离属性*/
        pthread_detach(thread_id);
    }
    /*5. 关闭连接*/
    close(sockfd);
}

void *thread_save_ip_func(void *argv)
{
	
	char ipaddr[64]={0};
	char ipaddr_r[64]={0};
	char file_buf[100]={0};
	char file_path[200]={0};
	char *p;
	int fd;
	
	
    while(1)
    {
		
		if (access(SAVE_IP_PATH, F_OK) == 0)
		{
			printf("save_ip file exist\n");
			strcpy(file_path, SAVE_IP_PATH);
			strcat(file_path, SAVE_IP_FILE);
			fd = open(file_path, 2);
			if(fd>=0)
			{			
				read(fd,file_buf,100);
				
				p = trim(file_buf);
				memset(ipaddr_r, 0, sizeof(ipaddr_r));
				strcpy(ipaddr_r, p);
				close(fd);
			}
			
			memset(ipaddr, 0, sizeof(ipaddr));
			get_local_ip(ipaddr);
			printf("get ipaddr:%s\n", ipaddr);
			printf("read r_ipaddr:%s\n", ipaddr_r);
			if(strcmp(ipaddr, ipaddr_r) != 0)
			{
				printf("ipaddr different\n");
				fd=open(file_path, O_CREAT | O_RDWR, 0777 );
				if(fd<0)
					continue;

				ftruncate(fd,0);

				/* 重新设置文件偏移量 */
				lseek(fd,0,SEEK_SET);
				printf("save to file, %d:%s\n", strlen(ipaddr), ipaddr);
				write(fd, ipaddr, strlen(ipaddr));

				close(fd);
			}
		}
		usleep(5000000);
	}
		
    //退出线程
    pthread_exit(NULL);
}


void save_ip_event()
{	
   
	pthread_t thread_id;

    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(udp_server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

   
	/*创建线程*/
	if(pthread_create(&thread_id,NULL,thread_save_ip_func,NULL))
	{
		printf("线程创建失败.\n");
		return -1;
	}
	/*设置线程的分离属性*/
	pthread_detach(thread_id);
}

int main(int argc, char* argv[])
{
	signal(SIGPIPE,SIG_IGN); //忽略 SIGPIPE 信号--防止服务器异常退出
	
	save_ip_event();
    udp_event();
	http_event();
	
    return 0;
}