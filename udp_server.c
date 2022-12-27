#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <ctype.h>
#include <sys/ioctl.h>
 
#include <net/if.h> // struct ifreq

#include <arpa/inet.h>


#define SERVER_PORT 8989
#define BUFF_LEN 1024

char module_name[100] = "MKS";




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


void handle_udp_msg(int fd)
{
    char buf[BUFF_LEN];  //接收缓冲区，1024字节
	char  ReplyBuffer[100] = "mkswifi:";
    socklen_t len;
    int count;
	char moduleIdBytes[21] = {0};
	char *moduleId;
	unsigned char mac[6] = {0};
	char ip[64];
    struct sockaddr_in clent_addr;  //clent_addr用于记录发送方的地址信息
	
	get_mac(mac);
	sprintf(moduleIdBytes, "HJNLM000%x%x%x%x%x%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	moduleId = my_strupr(moduleIdBytes);
	printf("Get mac: %s\n", moduleId);
	
	

	memset(ip, 0, sizeof(ip));

	get_local_ip(ip);
	printf("Get ip addr:%s\n", ip);
	
    while(1)
    {
        memset(buf, 0, BUFF_LEN);
        len = sizeof(clent_addr);
        count = recvfrom(fd, buf, BUFF_LEN, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        if(count == -1)
        {
            printf("recieve data fail!\n");
            return;
        }
		
		//收到mks wifi的搜索信息
		if(strstr(buf, "mkswifi") != 0)
		{
			memcpy(&ReplyBuffer[strlen("mkswifi:")], module_name, strlen(module_name)); 
			ReplyBuffer[strlen("mkswifi:") + strlen(module_name)] = ',';
			
			memcpy(&ReplyBuffer[strlen("mkswifi:")+ strlen(module_name) + 1], moduleId, strlen(moduleId)); 
			ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + 1] = ',';
			
			//strcpy(&ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + 2], WiFi.localIP().toString().c_str()); 
			//ReplyBuffer[strlen("mkswifi:") + strlen(module_name) + strlen(moduleId) + strlen(WiFi.localIP().toString().c_str()) + 2] = '\n';
			
		}
	
        printf("client:%s\n",buf);  //打印client发过来的信息
        memset(buf, 0, BUFF_LEN);
      //  sprintf(buf, "I have recieved %d bytes data!\n", count);  //回复client
      //  printf("server:%s\n",buf);  //打印自己发送的信息给
        sendto(fd, ReplyBuffer, sizeof(ReplyBuffer), 0, (struct sockaddr*)&clent_addr, len);  //发送信息给client，注意使用了clent_addr结构体指针
		usleep(100);
    }
}


/*
    server:
            socket-->bind-->recvfrom-->sendto-->close
*/

int main(int argc, char* argv[])
{
    int server_fd, ret;
    struct sockaddr_in ser_addr; 

    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(SERVER_PORT);  //端口号，需要网络序转换

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        return -1;
    }

    handle_udp_msg(server_fd);   //处理接收到的数据

    close(server_fd);
    return 0;
}