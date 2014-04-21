/*
   udp4-r2.c: bind to wildcard IP address
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAXSIZE 100

/* -----------------------------------------------------------------
    paddr: print the IP address in a standard decimal dotted format
   ----------------------------------------------------------------- */
void paddr(unsigned char *a)
{
   printf("%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
}

typedef struct reliableMsg {
	int rseqno;
	char rdata[100];
} reliableMsg;

int reliable_recvfrom(int seqno, int sock, void *data, int len, int flags, struct sockaddr *src_addr, int src_addr_len)
{
	reliableMsg msg;
	char ACK[MAXSIZE];
	int success = 0;
	fd_set write_fds;
	struct timeval timeout;
	//wait for first message
	printf("waiting for 1st msg\n");
	recvfrom(sock, &msg, sizeof(msg), 0 /* flags */, src_addr, &src_addr_len);
	printf("received 1st message\n");
	if(msg.rseqno == seqno){ // correct message, copy data
		success = 1;
		data = msg.rdata;
		printf("received seq %u 1st attempt: %s\n", msg.rseqno, data);
	}
	else {	//prepare write_fds and timeout value for select()
	FD_ZERO(&write_fds);
	FD_SET(sock, &write_fds);
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;	
	}
	while(!success){
		if(select(FD_SETSIZE, NULL, &write_fds, NULL, &timeout) == 0){
			//timeout! send ACK seqno-1
		  memcpy(ACK, (char*)&seqno, sizeof(int));
		  sendto(sock, ACK, strlen(ACK), 0 /* flags */, src_addr, src_addr_len);
			printf("timeout. sent old ACK %u\n", ACK);
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
		}
		else {
			//receive message, check seqno
			recvfrom(sock, &msg, sizeof(msg), 0 /* flags */, src_addr, &src_addr_len);
			printf("addtl attempt, received (%s) with ACK=%u\n", msg.rdata, msg.rseqno);
			if(msg.rseqno == seqno){
				//seq number matches, copy data and break out of loop
				success = 1;
				data = msg.rdata;
				break;
			}
			else {
				//wrong ACK, do nothing with msg, send ACK seqno-1, prepare to receive again
				FD_ZERO(&write_fds);
				FD_SET(sock, &write_fds);
				timeout.tv_sec = 5;
				timeout.tv_usec = 0;
			}
		}
	}
	//send ack
	memcpy(ACK, (char*)&seqno, sizeof(int));
	sendto(sock, ACK, strlen(ACK), 0 /* flags */, src_addr, src_addr_len); 
	printf("sent current ACK %u\n", ACK);
	return strlen(msg.rdata);
}

main(int argc, char *argv[])
{
   int s;			   /* s = socket */
   struct sockaddr_in in_addr;	   /* Structure used for bind() */
   struct sockaddr_in sock_addr;   /* Output structure from getsockname */
   struct sockaddr_in src_addr;    /* Used to receive (addr,port) of sender */
   int src_addr_len;		   /* Length of src_addr */
   int len;			   /* Length of result from getsockname */
   char line[MAXSIZE];
	int seqno;

   if (argc == 1)
    { printf("Usage: %s port\n", argv[0]);
      exit(1);
    }

   /* ---
      Create a socket
      --- */
   s = socket(AF_INET, SOCK_DGRAM, 0);

   /* ---
      Set up socket end-point info for binding
      --- */
   memset((char *)&in_addr, 0, sizeof(in_addr));
   in_addr.sin_family = AF_INET;                   /* Protocol family */
   in_addr.sin_addr.s_addr = htonl(INADDR_ANY);    /* Wildcard IP address */
   in_addr.sin_port = htons(atoi(argv[1]));	   /* Use any UDP port */

   /* ---
      Here goes the binding...
      --- */
   if (bind(s, (struct sockaddr *)&in_addr, sizeof(in_addr)) == -1)
    { perror("bind: ");
      printf("- Note: this program can be run on any machine...\n");
      printf("        but you have to pick an unsed port#\n");
      exit(1);
    }
   else
    { printf("OK: bind SUCCESS\n");
    }

   /* --------------------------------------------------------
      **** Print socket "name" (which is IP-address + Port#)
      -------------------------------------------------------- */
   len = sizeof(sock_addr);
   getsockname(s, (struct sockaddr *) &sock_addr, &len);
   printf("Socket is bind to:\n");
   printf("  addr = %u\n", ntohl(sock_addr.sin_addr.s_addr) );
   printf("  port = %u\n", ntohs(sock_addr.sin_port) );
	
	seqno = 0;
   while(1)
    { src_addr_len = sizeof(src_addr);
      len = reliable_recvfrom(seqno, s, line, MAXSIZE, 0 /* flags */, (struct sockaddr *) &src_addr, src_addr_len);

      printf("Msg from (");
      paddr( (void*) &src_addr.sin_addr);
      printf("/%u): `%s' (%u bytes)\n", src_addr.sin_port, line, len);
		seqno++;
    }
}