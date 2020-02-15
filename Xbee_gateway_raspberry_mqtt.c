#include<wiringPi.h>
#include<time.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<math.h>
#include<wiringSerial.h>
#include<mosquitto.h>


int serial_port=0;
char rx_pkt[300];
int cnt,status;
char address[][8]={{0x00,0x13,0xA2,0X00,0X41,0X04,0XAB,0X9B},
				{0x00,0x13,0xA2,0X00,0X41,0XAE,0XEE,0XCE},
				{0x00,0x13,0xA2,0X00,0X41,0XA4,0X4C,0X3B},				     {0x00,0x13,0xA2,0X00,0X41,0X89,0X8F,0X46}};
unsigned char r_addr[8],route_id[200];
char data[100];


int uart_init(void)
{
	if ((serial_port = serialOpen ("/dev/ttyAMA0", 9600)) < 0)	/* open serial port */
  	{
		perror("serialopen");
    		return 0;
  	}
}

void tx_data(unsigned char* data,unsigned char *Coor_Addr,unsigned int pkt_len) 
{
    unsigned int sum=0,check_sum=0;
    unsigned char xbee_pkt[200];
    
    memset(xbee_pkt,0,sizeof(xbee_pkt));
	
    xbee_pkt[0] = 0x7E;
    xbee_pkt[1] = 0x00;
    xbee_pkt[2] = 0x0E+pkt_len;  
    xbee_pkt[3] = 0x01;
    xbee_pkt[4] = 0x10;
    xbee_pkt[5] = Coor_Addr[0];
    xbee_pkt[6] = Coor_Addr[1];
    xbee_pkt[7] = Coor_Addr[2];
    xbee_pkt[8] = Coor_Addr[3];
    xbee_pkt[9] = Coor_Addr[4];
    xbee_pkt[10] = Coor_Addr[5];
    xbee_pkt[11] = Coor_Addr[6];
    xbee_pkt[12] = Coor_Addr[7];
    xbee_pkt[13] = 0xFF;
    xbee_pkt[14] = 0XFE;
    xbee_pkt[15] = OPTIONS;//options;
		xbee_pkt[16] = 0x00;//no.of addresses
		xbee_pkt[17] = 0x00;//adresses
    
		pkt_len = 18;
    for(i=3;i<pkt_len;i++)
    {
        sum = sum + xbee_pkt[i];
    }
    check_sum = (0xFF-(sum&0x00FF));
    xbee_pkt[pkt_len]=check_sum;
		pkt_len++;
    xbee_pkt[pkt_len]='\0';

	for(i=0;i<pkt_len;i++)
		serialPutchar(serial_port,data[i]);
}

int rx_data(void)
{
	int i=0;
	cnt =0;
	memset(rx_pkt,0,sizeof(rx_pkt));
	static char rx_pkt[20];
	while(serialDataAvail(serial_port)>0)
	{
			rx_pkt[i]=serialGetchar(serial_port);
			i++;
			cnt++;//will be used in checksum
	}
	if(i)
	{
		rx_pkt[i]='\0';
		return 1;
	}
	return 0;
}

int strcomp(char *src,char *src1)
{
	int i,j;
	for(i=0,j=0;i<8;i++,j++)
	{
		if(src1[i]==src[j])
			continue;
		else break;
	}
	if(i<0)
		return 1;
	else return 0;
}


int parse(void)
{
		int len,i,j,c_sum=0;

		memset(route_id,0,sizeof(route_id));
		memset(r_addr,0,sizeof(r_addr));
		memset(data,0,sizeof(data));	
			
//checking for delimiter
	if(rx_pkt[0]==0x7E)
	{
		//checking for receive packet frame
		if(rx_pkt[3]==0x90)
		{
			//extracting original data length from frame
				len=(rx_pkt[1]+rx_pkt[2])-0x0C;

			//checksum validation
				for(k=3;k<cnt;k++)
					c_sum+=rx_pkt[k];
				c_sum=c_sum&0x00ff;
				if(c_sum==0x00ff)
				{
					status = 1;
					printf("checksum matched\n");
				}
				else printf("checksum not matched\n");

			//extracting 64bit address to identify device
				for(i=4,j=0;i<8;i++,j++)
				{
					r_addr[j]=rx_pkt[i];
				}
				r_addr[j]='\0';

			//checking for device match
				if(strcomp(r_addr,r1[0])==0)
				{
					strcpy(route_id,"R1:");
				}
				else if(strcomp(r_addr,r1[1])==0)
				{
					strcpy(route_id,"R2:");
				}
				else if(strcomp(r_addr,r1[2])==0)
				{
					strcpy(route_id,"R3:");
				}
				else if(strcomp(r_addr,r1[3])==0)
				{
					strcpy(route_id,"R4:");
				}
			//if checksum matched collecting data
				if(status==1)
				{
					for(i=0,j=15;i<len;i++,j++)
						data[i]=rx_pkt[j];
					data[i]='/0';
					printf("data:%s\n",data);
					cnt=0;
					return 1;
				}
				else return 0;
		}
		printf("Not a received frame\n");
		return 0;
	}
		printf("Delimiter not received\n");
		return 0;
}


//Time Collection
char *timedate(void)
{
	static char temp[50];
	memset(temp,0,50); 
        time_t current_time = time(NULL);
        struct tm *tm = localtime(&current_time);
        strftime(temp, sizeof(temp), "%c", tm);
	return temp;
}

//Publish to server
void publish(char *data,int pkt_len,char *topic,struct mosquitto *mosq)
{
		mosquitto_publish(mosq,NULL,"topic",pkt_len,data,0,true);
		printf("\n------------------------------------------------\n");
		printf("%s published\n",data);
}


int main()
{
	printf( "Gateway Between XBEE and MQTT\n" );
  	int pkt_len ;
	char *recv,*arr=NULL,*time;
	
	char *host = "soldier.cloudmqtt.com";
	char *username = "cbocdpsu";
	char *passwd = "3_UFu7oaad-8";
//	char *host = "test.mosquitto.org";
//	char *host = "broker.hivemq.com";
//	char *host = "192.168.1.10";
	int port = 14035;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	char *topic = "RTU";

	if(uart_init()==0)
		return 0;


	if ( wiringPiSetup() == -1 )
		exit( 1 );
		
		mosquitto_lib_init();
	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}
		
	mosquitto_username_pw_set(mosq,username,passwd);

	if(mosquitto_connect_async(mosq, host, port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
		return 1;
		}
	mosquitto_loop_start(mosq);

	while ( 1 )
	{
		if(rx_data()>0)
		{
			Printf("\n----received data extracting-----\n");
			if(parse()>0)
			{
				time=timedate();
				strcpy(route_id,data);
				strcat(route_id," ");
				strcat(route_id,time);
				pkt_len=strlen(route_id);
				publish(route_id,pkt_len,topic,mosq);
			("\n------------------------------------\n");
			}
			delay(1000);//for 1 sec delay
		}
	}
	mosquitto_loop_stop(mosq,true);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
 
	return(0);
}
