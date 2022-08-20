#include "protocol.h"
#include "csapp.h"
#include "debug.h"

int proto_send_packet(int fd, MZW_PACKET *pkt, void *data)
{
	//debug("In my send code\n");

	uint16_t size =  pkt->size;

	//convert all multi-byte member variables to network byte ordering
	pkt->size = htons(pkt->size);
	pkt->timestamp_sec = htonl(pkt->timestamp_sec);
	pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);

	ssize_t ret = rio_writen(fd, (void*)pkt, sizeof(MZW_PACKET));
	if (ret < 0) return -1;

	if (size > 0 && data != NULL)
	{
		ret = rio_writen(fd, data, size);
		if (ret < 0) return -1;
	}

	return 0;
}



int proto_recv_packet(int fd, MZW_PACKET *pkt, void **datap)
{

	//debug("In My recieve Code\n");

	ssize_t ret = rio_readn(fd, (void*)pkt, sizeof(MZW_PACKET));
	if (ret < 0)
	{
		if(ret == -1)
			debug("end of file has reached");
		if (ret == -2)
			debug("got an Interrupt");
		return ret;
	}

	//convert all multi-byte member variables to host byte ordering
	pkt->size = ntohs(pkt->size);
	pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
	pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);

	uint16_t pktsize = pkt->size;

	if (pktsize > 0 && datap)
	{
		char* dumm = malloc(sizeof(char)*pktsize);
		//*datap = malloc(sizeof(char)*pktsize);
		ret = rio_readn(fd, dumm, pktsize);
		if (ret < 0)
		{
			free(dumm);

			if(ret == -1)
				debug("end of file has reached");

			if (ret == -2)
				debug("got an Interrupt");

			return ret;
		}

		*datap = dumm;
	}

	return 0;
}