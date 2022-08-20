#include "server.h"
#include "csapp.h"
#include "protocol.h"
#include <stdbool.h>
#include "player.h"
#include "debug.h"


int debug_show_maze;
CLIENT_REGISTRY *client_registry;


void *mzw_client_service(void *arg)
{
	int* fd_p = (int*)arg;
	int fd = *fd_p;
	free(fd_p);
	Pthread_detach(Pthread_self());

	bool loginpktreceived = false;
	MZW_PACKET *pkt = malloc(sizeof(MZW_PACKET));
	void** data = malloc(sizeof(char*));
	PLAYER *obj = NULL;
	struct timespec t;

	while(1)
	{
		int ret = proto_recv_packet(fd, pkt, data);
		if (ret == -2) //Interrupt due to SIGUSR1
		{
			//laser hit on player
			debug("calling player player_check_for_laser_hit due to an Interrupt");
			player_check_for_laser_hit(obj);
			show_maze();
			continue;
		}
		else if(ret == -1 || ret == -3)  //EOF or some other error
		{
			break;
		}

		MZW_PACKET_TYPE type = pkt->type;
		if (!loginpktreceived)
		{
			if (type == MZW_LOGIN_PKT)
			{
				debug("login packet received");
				debug("avatar is %c", pkt->param1);
				debug("name of player is %s", (char*)*data);
				obj = player_login(fd, pkt->param1, *data);
				MZW_PACKET *pkt_tosend = malloc(sizeof(MZW_PACKET));
				if (obj)
				{
					//send a READY pkt
					creg_register(client_registry, fd);
					debug("after object creation - sending a ready packet");
					pkt_tosend->type = MZW_READY_PKT;
					pkt_tosend->size = 0;
					clock_gettime(CLOCK_MONOTONIC, &t);
					pkt_tosend->timestamp_sec = t.tv_sec;
					pkt_tosend->timestamp_nsec = t.tv_nsec;
					proto_send_packet(fd, pkt_tosend, NULL);
					player_reset(obj);
					loginpktreceived = true;
					show_maze();
				}
				else
				{
					//send a IN_USE pkt
					pkt_tosend->type = MZW_INUSE_PKT;
					pkt_tosend->size = 0;
					clock_gettime(CLOCK_MONOTONIC, &t);
					pkt_tosend->timestamp_sec = t.tv_sec;
					pkt_tosend->timestamp_nsec = t.tv_nsec;
					proto_send_packet(fd, pkt_tosend, NULL);
					pthread_exit(NULL);
				}
				if (*data != NULL)
					free(*data);
				free(pkt_tosend);
			}
			else
				continue;
		}
		else
		{
			if (type == MZW_LOGIN_PKT)
				continue;
			else
			{
				if (type == MZW_MOVE_PKT)
				{
					debug("move packet received\n");
					int ret = player_move(obj, pkt->param1);
					if (ret < 0)
						debug("maze move failed");
					show_maze();
				}
				else if(type == MZW_TURN_PKT)
				{
					debug("turn packet received\n");
					player_rotate(obj, pkt->param1);
					show_maze();
				}
				else if (type == MZW_FIRE_PKT)
				{
					debug("fire packet received\n");
					player_fire_laser(obj);
					show_maze();
				}
				else if (type == MZW_REFRESH_PKT)
				{
					debug("refresh packet received\n");
					player_invalidate_view(obj);
					player_update_view(obj);
					show_maze();
				}
				else if(type == MZW_SEND_PKT)
				{
					debug("chat packet received\n");
					player_send_chat(obj, *data, pkt->size);
					free(*data);
				}
			}
		}
	}

	player_logout(obj);
	creg_unregister(client_registry, fd);
	free(pkt);
	free(data);
	close(fd);

	return NULL;
}
