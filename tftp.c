/* TFTP Server 
   Author: Meenakshi Narayanan
*/

#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<time.h>
#include<netdb.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>

#define RRQ 1
#define DATA 3
#define ACK 4
#define ERROR 5



int client_addr_size=sizeof(struct sockaddr);


struct argument
	{
	int sd;
	FILE * fp;
	struct sockaddr_in client_addr;

	};
	
void client(void* arguments)
{	char file_data[512],buf[512],*send_data,*error;
	time_t start;
	fd_set read_fds;
	int finish=0,num_bytes,diff,opcode,block=1,block_nw,ack_no,err_no,len,sd;
	struct argument* arg=arguments;	
	struct sockaddr_in client_addr;
	socklen_t client_size=sizeof client_addr;
	sd=arg->sd;
	client_addr=arg->client_addr;
	FD_SET(sd,&read_fds);
	
	printf("Hello! Inside the new thread\n");

	num_bytes=fread(file_data,1,512,arg->fp);
	
	printf("\ndata read is %s\n",file_data);
	printf("\nNumber of bytes read is %d\n",num_bytes);
	if(num_bytes<512)
		finish=1;
	
	//send packet
	send_data=(char*)malloc(2+2+num_bytes);
	opcode=DATA;
	opcode=htons(opcode);
	block_nw=htons(block);
	
	memcpy(send_data,&opcode,2);
	memcpy(send_data+2,&block_nw,2);
	memcpy(send_data+4,file_data,num_bytes);
	
	printf("\ndata to be sent is %s\n",send_data);
	if(sendto(sd,send_data, 4+num_bytes,0,(struct sockaddr*)&client_addr, client_size)==-1)
		printf("\n send failed\n");
	start=time(NULL);
	block++;
	
	while(1)
	{	end=time(NULL);
		
		diff=(start-end)*1000;

		printf("\n The packet was sent %d milliseconds back\n",diff);
		if(diff>100)
		{	//retransmit
			printf("\nsending same packet again\n");

			opcode=DATA;
			block--;
			opcode=htons(opcode);
			block_nw=htons(block);

			realloc(send_data,2+2+num_bytes);
			memcpy(send_data,&opcode,2);
			memcpy(send_data+2,&block_nw,2);
			memcpy(send_data+4,file_data,num_bytes);

			if(sendto(sd,send_data, 4+num_bytes,0,(struct sockaddr*)&client_addr, client_size)==-1)
				printf("\nsend failed\n");			
			
			start=time(NULL);
			block++;
		}
			
		else if(diff>5)
		{	fclose(arg->fp);
			printf("\nClosing connection\n");
			close(sd);
			pthread_exit(NULL);
		}
		
		printf("\nLook for acknowledgements\n");
		select(sd+1,&read_fds,NULL,NULL,NULL);
		if(FD_ISSET(sd,&read_fds))
		{	printf("\nThere's an incoming message\n");
			len=recvfrom(sd,buf,512,0,(struct sockaddr*)&client_addr,&client_addr_size);	
			
			//check if client has closed connection
			if(len==0)
			{	fclose(arg->fp);
				close(sd);
				pthread_exit(NULL);
			}
			
			//check opcode
			memcpy(&opcode,buf,2);
			opcode=ntohs(opcode);
			printf("\nopcode is %d\n",opcode);

			if(opcode==ACK)
			{	
				memcpy(&ack_no,buf+2,2);
				ack_no=ntohs(ack_no);
				printf("\nack for block %d and block number %d\n",ack_no,block);

				if(ack_no==block-1)	
				{	//check if entire file has been successfully sent
					if(finish==1)
					{	printf("\nClosing connection\n");
						close(sd);
						printf("\nServiced clientn");
						fclose(arg->fp);
						pthread_exit(NULL);
					}
					else
					{	num_bytes=fread(file_data,1,512,arg->fp);
						if(num_bytes<512)
							finish=1;	
						realloc(send_data,2+2+num_bytes);
						opcode=DATA;
						opcode=htons(opcode);
						block_nw=htons(block);

						memcpy(send_data,&opcode,2);
						memcpy(send_data+2,&block_nw,2);
						memcpy(send_data+4,file_data,num_bytes);
		
						sendto(sd,send_data, 4+num_bytes,0,(struct sockaddr*)&client_addr, client_size);
						start=time(NULL);
						block++;
						
					}
				}
				else
				{	//send error packet
					opcode=ERROR;
					err_no=0;
					error="Duplicate acknowledgement";
					opcode=htons(opcode);
					err_no=htons(err_no);
					realloc(send_data,2+2+strlen(error));
					memcpy(send_data,&opcode,2);
					memcpy(send_data+2,&err_no,2);
					memcpy(send_data+4,error,strlen(error));
					
				sendto(sd,send_data, 4+strlen(error),0,(struct sockaddr*)&client_addr,client_size);
				}
					
			}			
		}
	}

}	

int main(int argc, char* argv[])
{	int sd,err,len,i=0,opcode,err_no,sd1,*a,port,status;
	fd_set read_fds;
	struct sockaddr_in client_addr;
	socklen_t client_size=sizeof client_addr;
	char buf[516],*temp,*send_data,*error,filename[56],str_port[5];
	struct addrinfo hints, *res;
	pthread_t t;
	FILE *fp;
	struct argument arg;
	
	memset(&hints, 0, sizeof hints);
	//Already known connection details that helps in lookup
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_family=AF_INET;
	hints.ai_flags=AI_PASSIVE;
	
	//look up
	if((status=getaddrinfo(argv[1],"8500",&hints, &res))!=0)
	
	{	printf("\nstatus %d\n",status);
		fprintf(stderr,"\n Error in service lookup\n");
		
		return 1;
	}
	
	//socket creation
	if((sd=socket(res->ai_family, res->ai_socktype,res->ai_protocol))==0)
	{	printf(" cha\n");
		fprintf(stderr,"\nError creating socket\n");
		return 1;
	}
	
	if((err=bind(sd,res->ai_addr,res->ai_addrlen))==-1)
	{	fprintf(stderr,"\nError binding socket\n");
		return 1;
	}
	
	printf("\nAfter socket set up\n");
	FD_SET(sd,&read_fds);
	strcpy(str_port,argv[2]);
	
	while(1)
	{	select(sd+1,&read_fds,NULL,NULL,NULL);
		if(FD_ISSET(sd,&read_fds))
		{	printf("\n\n**************************************There's a request in main**************************!\n");
			len=recvfrom(sd,buf,512,0,(struct sockaddr*)&client_addr,&client_addr_size);
			if(len==-1)
				fprintf(stderr,"\n error receiving info\n");
			else
			{
				memcpy(&opcode,buf,2);
	
				//alternate option
				//a=(int*)buf;
				//opcode=*a;
	
				opcode=ntohs(opcode);
				printf("\n opcode %d", opcode);
				if(opcode==RRQ)
				{	//get filename
					strcpy(filename,&buf[2]);
					printf("\nfilename %s\n",filename);

					//get new port and if look up fails, try at most 10 times
					printf("\nLookup\n");

					port=atoi(str_port);
					port++;
					//itoa(port,str_port,10);
					sprintf(str_port,"%d",port);

					memset(&hints, 0, sizeof hints);
					//Already known connection details that helps in lookup
					hints.ai_socktype=SOCK_DGRAM;
					hints.ai_family=AF_INET;
					hints.ai_flags=AI_PASSIVE;

					if(getaddrinfo(argv[1],NULL,&hints,&res)!=0)
					{	fprintf(stderr,"Can't find empty port\n");
						i=1;
					}
						
					if(i)
						continue;

					printf("\nsocket creation\n");
					//create a new socket and bind it to new port
					sd1=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
					bind(sd1,res->ai_addr,res->ai_addrlen);
				
					printf("\nopen file\n");
					fp=fopen(filename,"r");
					

					if (fp==NULL)
					{	//send error packet
						printf("\nfile not found!\n");
						opcode=ERROR;
						err_no=1;
						opcode=htons(opcode);
						err_no=htons(err_no);
						error="File not found";
						send_data=malloc(2+2+strlen(error));
						memcpy(send_data,&opcode,2);
						memcpy(send_data+2,&err_no,2);
						memcpy(send_data+4,error,strlen(error));
					
					sendto(sd,send_data, 4+strlen(error),0,(struct sockaddr*)&client_addr,client_size);
						continue;
					}
					//arguments to send to function that is to be executed by thread
					printf("\nfile opened successfully\n");
					
					arg.sd=sd1;
					arg.fp=fp;
					arg.client_addr=client_addr;
			
					//start executing new thread by passing new socket descriptor, file descriptor
					pthread_create(&t,NULL,client,(void*)&arg);
				}

				
			}	
		}
				
	}
	pthread_join(t,NULL);
	return 1;

}	
				

										


//figure out sockaddr
//check if incoming data is from that client?
			
//port
