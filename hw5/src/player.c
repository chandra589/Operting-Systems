#include "player.h"
#include "maze.h"
#include "csapp.h"
#include <stdbool.h>
#include "debug.h"


void sigusr_handler(int signum)
{
	//player_check_for_laser_hit();
	debug("In sigusr handler");
}

typedef struct player
{
	pthread_mutex_t mutex_player;
	pthread_mutexattr_t Attr;
	OBJECT avatar;
	char* name;
	int fd;
	int row;
	int col;
	int gaze;
	int score;
	int refcount;
	pthread_t thread_id;
	bool Invalidate;
	int view_depth;
	VIEW *view;
}PLAYER;

typedef struct mapentry
{
	OBJECT avatar;
	PLAYER* player;
	struct mapentry* next;
}MAPENTRY;

typedef struct map
{
	MAPENTRY* head;
	MAPENTRY* tail;
	pthread_mutex_t mutex_map;
}PLAYERMAP;

PLAYERMAP *map = NULL;


void player_init(void)
{
	debug("player module init");
	map = malloc(sizeof(PLAYERMAP));
	map->head = NULL;
	map->tail = NULL;
	pthread_mutex_init(&map->mutex_map, NULL);
}

void player_fini(void)
{
	MAPENTRY* iter = map->head;
	while(iter != NULL)
	{
		MAPENTRY* next = iter->next;
		free(iter->player->name);
		free(iter->player->view);
		//have to destroy player mutex
		pthread_mutexattr_destroy(&iter->player->Attr);
		pthread_mutex_destroy(&iter->player->mutex_player);
		//
		free(iter->player);
		free(iter);

		iter = next;
	}
	pthread_mutex_destroy(&map->mutex_map);
	free(map);
}

PLAYER *player_login(int clientfd, OBJECT avatar, char *name)
{
	debug("in player login func");
	//check if the avatar exists in the map
	debug("locking map mutex");
	pthread_mutex_lock(&map->mutex_map);

	bool avatarexists = false;
	MAPENTRY* iter = map->head;
	while(iter != NULL)
	{
		if (iter->avatar == avatar)
		{
			avatarexists = true;
			break;
		}
		iter = iter->next;
	}

	if (avatarexists)
	{
		//attempt to use an unused avatar.
		debug("avatar exists - finding a new avatar");
		int loopiter = 1;
		OBJECT avatar_check = 'A';
		bool avatar_found = false;
		while(loopiter < 26)
		{
			bool tryavatarexists = false;
			iter = map->head;
			while(iter != NULL)
			{
				if (iter->avatar == avatar_check)
				{
					tryavatarexists = true;
					break;
				}
				iter = iter->next;
			}

			if (!tryavatarexists)
			{
				avatar = avatar_check;
				avatar_found = true;
				break;
			}
			avatar_check+=1;
			loopiter++;
		}

		if (!avatar_found)
		{
			debug("did not find a new avatar");
			pthread_mutex_unlock(&map->mutex_map);
			debug("unlocking map in player login");
			return NULL;
		}
		else
			debug("found a new avatar %c", avatar_check);
	}


	pthread_mutex_unlock(&map->mutex_map);
	debug("unlocking map in player login");


	PLAYER* player = malloc(sizeof(PLAYER));
	player->fd = clientfd;
	player->avatar = avatar;
	char* nameplayer = malloc(sizeof(char)*strlen(name));
	memcpy(nameplayer, name, strlen(name));
	player->name = nameplayer;
	player->refcount = 1;
	player->row = -1;
	player->col = -1;
	player->gaze = EAST;
	player->score = 0;
	player->thread_id = pthread_self();
	player->Invalidate = true;
	player->view_depth = 0;
	player->view = malloc(sizeof(char)*VIEW_DEPTH*VIEW_WIDTH);

	//initializing the player recursive mutex
	pthread_mutexattr_init(&player->Attr);
	pthread_mutexattr_settype(&player->Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&player->mutex_player, &player->Attr);


	//Initializing new map entry
	MAPENTRY* newnode = malloc(sizeof(MAPENTRY));
	newnode->avatar = avatar;
	newnode->player = player;
	newnode->next = NULL;

	//lock mutex on map
	debug("locking map in player login and inserting player in map");
	pthread_mutex_lock(&map->mutex_map);

	//Insert in the map
	if (map->head == NULL)
	{
		map->head = newnode;
	}
	else
	{
		if (map->tail == NULL)
		{
			map->tail = newnode;
			map->head->next = map->tail;
		}
		else
		{
			map->tail->next = newnode;
			map->tail = newnode;
		}
	}

	//unlock mutex on map
	pthread_mutex_unlock(&map->mutex_map);
	debug("unlocking map in player login after inserting player");

	//Install SIGUSR1 handler
	struct sigaction sausr;
    sausr.sa_handler = sigusr_handler;
    sigemptyset(&sausr.sa_mask);
    //sausr.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sausr, NULL) == -1)
    {
        perror("Sigaction Signal error");
    }

	return player;
}

void player_logout(PLAYER *player)
{
	debug("player logout called on avatar %c", player->avatar);

	maze_remove_player(player->avatar, player->row, player->col);

	MZW_PACKET * scorepkt = malloc(sizeof(MZW_PACKET));
	scorepkt->type = MZW_SCORE_PKT;
	scorepkt->param1 = player->avatar;
	scorepkt->param2 = -1;
	scorepkt->size = 0;

	debug("locking map in player logout for removing this player");
	pthread_mutex_lock(&map->mutex_map);


	MAPENTRY* iter = map->head;
	while(iter != NULL)
	{
		//if (iter->avatar != player->avatar)
		player_send_packet(iter->player, scorepkt, NULL);
		iter = iter->next;
	}

	debug("unlocking map in player logout for removing this player");
	pthread_mutex_unlock(&map->mutex_map);
	free(scorepkt);

	//call unref()
	char dum[] = "logging out user";
	char* why = malloc(sizeof(char)*strlen(dum));
	memcpy(why, dum, strlen(dum));
	player_unref(player, why);
	free(why);

	debug("locking map in player logout to remove player from map");
	pthread_mutex_lock(&map->mutex_map);

	//remove from map
	MAPENTRY* prev = map->head;
	MAPENTRY* curr = map->head;
	while(curr != NULL){
		if (curr->avatar == player->avatar)
		{
			debug("removing player %c", player->avatar);
			//destroy mutex attr and mutex
			pthread_mutexattr_destroy(&curr->player->Attr);
			pthread_mutex_destroy(&curr->player->mutex_player);

			if (curr == map->head)
			{
				map->head = map->head->next;
				free(prev->player->name);
				free(prev->player->view);
				free(prev->player);
				free(prev);
			}
			else if (curr == map->tail)
			{
				prev->next = curr->next;
				free(curr->player->name);
				free(curr->player->view);
				free(curr->player);
				free(curr);
				map->tail = prev;
			}
			else
			{
				prev->next = curr->next;
				free(curr->player->name);
				free(curr->player->view);
				free(curr->player);
				free(curr);
			}
			break;
		}
		prev = curr;
		curr = curr->next;
	}


	iter = map->head;
	while(iter != NULL)
	{
		debug("updating view of client %c", iter->avatar);
		player_update_view(iter->player);
		iter = iter->next;
	}

	pthread_mutex_unlock(&map->mutex_map);
	debug("unlocking map in player logout after removing player from map");
}


void player_reset(PLAYER *player)
{
	debug("Inside player reset");
	debug("locking player object");
	pthread_mutex_lock(&player->mutex_player);

	//remove player if not setting for the first time.
	if (player->row != -1 && player->col == -1)
		maze_remove_player(player->avatar, player->row, player->col);

	int* row = malloc(sizeof(int));
	int* col = malloc(sizeof(int));
	int ret = maze_set_player_random(player->avatar, row, col);
	if (ret < 0)
	{
		debug("shutting down player as we could not find a new place in maze");
		shutdown(player->fd, SHUT_RD);
		debug("unlocking player mutex and returing");
		pthread_mutex_unlock(&player->mutex_player);
		return;
	}

	player->row = *row;
	player->col = *col;
	player->gaze = EAST;
	free(row); free(col);

	debug("unlocking player mutex after updating new row and col");
	pthread_mutex_unlock(&player->mutex_player);

	debug("locking map mutex for sending score update and view update in player reset");
	pthread_mutex_lock(&map->mutex_map);

	//updating player views
	debug("sending view pkts for all clients");
	MAPENTRY* iter = map->head;
	while(iter != NULL)
	{
		debug("updating view of player %c", iter->avatar);
		player_update_view(iter->player);
		iter = iter->next;
	}
	debug("sent updated views on all clients");

	//updating scores of all clients
	debug("sending score pkts to all clients");

	iter = map->head;
	while(iter != NULL)
	{
		MAPENTRY *iter1 = map->head;
		while(iter1 != NULL)
		{
			PLAYER* other_player = player_get(iter1->player->avatar);

			MZW_PACKET* iter1pkt = malloc(sizeof(MZW_PACKET));
			iter1pkt->type = MZW_SCORE_PKT;
			iter1pkt->param1 = other_player->avatar;
			iter1pkt->param2 = other_player->score;
			iter1pkt->size = strlen(other_player->name);
			debug("sending score pkt from player %c to player %c", iter1->avatar, iter->avatar);
			player_send_packet(iter->player, iter1pkt, other_player->name);

			//calling unref on other player
			char dum[] = "sent score pkt update to other player and decrease ref count";
			char* why = malloc(sizeof(char)*strlen(dum));
			memcpy(why, dum, strlen(dum));
			player_unref(other_player, why);
			free(why);
			free(iter1pkt);
			iter1 = iter1->next;
		}
		iter = iter->next;
	}

	pthread_mutex_unlock(&map->mutex_map);
	debug("unlocking map mutex after sending score update and view update");

}

PLAYER *player_get(unsigned char avatar)
{
	debug("In player get function");
	//debug("locking map mutex");
	//pthread_mutex_lock(&map->mutex_map);

	MAPENTRY* iter = map->head;
	PLAYER* player = NULL;
	while(iter != NULL)
	{
		if (iter->player->avatar == avatar)
		{
			char dum[] = "Increase ref count as a result of call to player_get func";
			char* why = malloc(sizeof(char)*strlen(dum));
			memcpy(why, dum, strlen(dum));
			player_ref(iter->player, why);
			free(why);
			player = iter->player;
			break;
		}
		iter = iter->next;
	}

	//pthread_mutex_unlock(&map->mutex_map);
	//debug("unlocking map mutex");

	return player;
}


PLAYER *player_ref(PLAYER *player, char *why)
{
	debug("locking player mutex in player_ref func");
	pthread_mutex_lock(&player->mutex_player);

	player->refcount+=1;
	debug("%s", why);
	debug("ref count of avatar %c changed from %d to %d", player->avatar, (player->refcount) - 1, player->refcount);

	pthread_mutex_unlock(&player->mutex_player);
	debug("unlocking player mutex in player_ref func");
	return player;
}


void player_unref(PLAYER *player, char *why)
{
	debug("locking player mutex in player_unref func");
	pthread_mutex_lock(&player->mutex_player);

	player->refcount-=1;
	debug("%s", why);
	debug("ref count of avatar %c changed from %d to %d", player->avatar, (player->refcount) + 1, player->refcount);

	pthread_mutex_unlock(&player->mutex_player);
	debug("unlocking player mutex in player_unref func");
}


int player_send_packet(PLAYER *player, MZW_PACKET *pkt, void *data)
{
	//lock player mutex - this should be a recursive mutex
	//debug("locking player mutex in player_send_pkt func");
	pthread_mutex_lock(&player->mutex_player);

	//debug("player fd is %d and player avatar is %c", player->fd, player->avatar);
	//debug("param is %c", pkt->param1);


	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	pkt->timestamp_sec = t.tv_sec;
	pkt->timestamp_nsec = t.tv_nsec;

	proto_send_packet(player->fd, pkt, data);

	//unlock player mutex
	pthread_mutex_unlock(&player->mutex_player);
	//debug("unlocking player mutex in player_send_pkt func");

	return 0;
}

int player_get_location(PLAYER *player, int *rowp, int *colp, int *dirp)
{
	//lock player mutex
	debug("locking player mutex in player_get_loc func");
	pthread_mutex_lock(&player->mutex_player);

	*rowp = player->row;
	*colp = player->col;
	*dirp = player->gaze;

	//unlock player mutex
	pthread_mutex_unlock(&player->mutex_player);
	debug("unlocking player mutex in player_get_loc func");

	return 0;
}

int player_move(PLAYER *player, int dir)
{

	debug("player move called on avatar %c", player->avatar);
	DIRECTION dirtomove = player->gaze;
	if (dir == -1)
		dirtomove = REVERSE(dirtomove);

	int ret = maze_move(player->row, player->col, dirtomove);

	if (ret < 0)
	{
		//maze move failed
		//send an alert packet to client
		MZW_PACKET* alpkt = malloc(sizeof(MZW_PACKET));
		alpkt->type = MZW_ALERT_PKT;
		alpkt->size = 0;
		player_send_packet(player, alpkt, NULL);
		free(alpkt);
	}
	else if(ret == 0)
	{
		//update player state
		debug("locking player mutex in player_move");
		pthread_mutex_lock(&player->mutex_player);

		int newrow = player->row;
		int newcol = player->col;

		if (player->gaze == EAST)
		{
			if(dir == 1)
				newcol+=1;
			else
				newcol-=1;
		}
		else if(player->gaze == WEST)
		{
			if (dir == 1)
				newcol-=1;
			else
				newcol+=1;
		}
		else if(player->gaze == SOUTH)
		{
			if(dir == 1)
				newrow+=1;
			else
				newrow-=1;
		}
		else if(player->gaze == NORTH)
		{
			if (dir == 1)
				newrow-=1;
			else
				newrow+=1;
		}
		player->row = newrow;
		player->col = newcol;

		pthread_mutex_unlock(&player->mutex_player);
		debug("unlocking player mutex in player move");

		debug("player move success - updating all logged in client views");
		debug("locking map mutex");
		pthread_mutex_lock(&map->mutex_map);

		MAPENTRY* iter = map->head;
		while(iter != NULL)
		{
			debug("updating view of client %c", iter->avatar);
			player_update_view(iter->player);
			iter = iter->next;
		}

		pthread_mutex_unlock(&map->mutex_map);
		debug("unlocking map mutex");
	}

	return ret;

}


void player_rotate(PLAYER *player, int dir)
{
	debug("locking player mutex in player_rotate");
	pthread_mutex_lock(&player->mutex_player);

	if (dir == 1)
		player->gaze = TURN_LEFT(player->gaze);
	else if(dir == -1)
		player->gaze = TURN_RIGHT(player->gaze);

	pthread_mutex_unlock(&player->mutex_player);
	debug("unlocking player mutex in player rotate");

	player_invalidate_view(player);
	player_update_view(player);
}


void player_fire_laser(PLAYER *player)
{
	debug("locking player mutex in player_fire_laser");
	pthread_mutex_lock(&player->mutex_player);

	OBJECT ret = maze_find_target(player->row, player->col, player->gaze);
	if (IS_AVATAR(ret))
	{
		debug("hit successfull");
		//score of this player is increased
		player->score +=1;

		MZW_PACKET* scorepkt = malloc(sizeof(MZW_PACKET));
		scorepkt->type = MZW_SCORE_PKT;
		scorepkt->size = strlen(player->name);
		scorepkt->param1 = player->avatar;
		scorepkt->param2 = player->score;

		//some avatar is hit - send signal to this thread and send new score of this player to all clients
		debug("locking map mutex");
		pthread_mutex_lock(&map->mutex_map);

		MAPENTRY* iter = map->head;
		while(iter != NULL)
		{
			if (ret == iter->player->avatar)
			{
				debug("sending kill singal to player %c", ret);
				pthread_kill(iter->player->thread_id, SIGUSR1);
				//break;
			}
			iter = iter->next;
		}


		debug("sending updated score to all clients");
		iter = map->head;
		while(iter != NULL)
		{
			MAPENTRY *iter1 = map->head;
			while(iter1 != NULL)
			{
				PLAYER* other_player = player_get(iter1->player->avatar);

				MZW_PACKET* iter1pkt = malloc(sizeof(MZW_PACKET));
				iter1pkt->type = MZW_SCORE_PKT;
				iter1pkt->param1 = other_player->avatar;
				iter1pkt->param2 = other_player->score;
				iter1pkt->size = strlen(other_player->name);
				debug("sending score pkt from player %c to player %c", iter1->avatar, iter->avatar);
				player_send_packet(iter->player, iter1pkt, other_player->name);

				//calling unref on other player
				char dum[] = "sent score pkt update to other player and decrease ref count";
				char* why = malloc(sizeof(char)*strlen(dum));
				memcpy(why, dum, strlen(dum));
				player_unref(other_player, why);
				free(why);
				free(iter1pkt);
				iter1 = iter1->next;
			}
			iter = iter->next;
		}

		pthread_mutex_unlock(&map->mutex_map);
		debug("unlocking map mutex");

		free(scorepkt);
	}

	debug("unlocking player mutex in player_fire_laser");
	pthread_mutex_unlock(&player->mutex_player);
}


void player_check_for_laser_hit(PLAYER *player)
{
	debug("In player_check_for_laser_hit called for avatar %c", player->avatar);

	maze_remove_player(player->avatar, player->row, player->col);

	show_maze();

	//updating view of all clients(except this client) such that the player has been removed
	debug("locking map mutex in check for laser hit");
	pthread_mutex_lock(&map->mutex_map);

	MAPENTRY* iter = map->head;
	while(iter != NULL)
	{
		//update the view of all other clients the client which took a hit
		if (iter->player->avatar != player->avatar)
			player_update_view(iter->player);

		iter = iter->next;
	}

	pthread_mutex_unlock(&map->mutex_map);
	debug("unlocking map mutex in check for laser hit");
	//

	//alert packet to this player, showing that he took a hit
	debug("sending alert packet to player %c", player->avatar);
	MZW_PACKET* alpkt = malloc(sizeof(MZW_PACKET));
	alpkt->type = MZW_ALERT_PKT;
	alpkt->size = 0;
	player_send_packet(player, alpkt, NULL);
	free(alpkt);

	//sleep for 3 sec
	debug("sleep for purgatory 3 sec before resetting %c", player->avatar);
	sleep(3);

	//resetting player to a new location
	player_reset(player);
}


void player_invalidate_view(PLAYER *player)
{
	player->Invalidate = true;
}

void player_update_view(PLAYER *player)
{
	//sending a clear pkt first
	MZW_PACKET* viewpkt = malloc(sizeof(MZW_PACKET));
	viewpkt->type = MZW_CLEAR_PKT;
	viewpkt->size = 0;
	player_send_packet(player, viewpkt, NULL);

	debug("locking player mutex in update view");
	pthread_mutex_lock(&player->mutex_player);

	if (player->Invalidate)
	{
		//send all the view packets
		int depthallowed = maze_get_view(player->view, player->row, player->col, player->gaze, VIEW_DEPTH);
		debug("depth allowed is %d", depthallowed);
		player->view_depth = depthallowed;
		VIEW* view = player->view;
		viewpkt->type = MZW_SHOW_PKT;
		viewpkt->size = 0;

		for(int i = 0; i < depthallowed; i++)
		{
			viewpkt->param1 = (*view)[i][0];
			viewpkt->param2 = 0;
			viewpkt->param3 = i;
			player_send_packet(player, viewpkt, NULL);

			viewpkt->param1 = (*view)[i][1];
			viewpkt->param2 = 1;
			viewpkt->param3 = i;
			player_send_packet(player, viewpkt, NULL);

			viewpkt->param1 = (*view)[i][2];
			viewpkt->param2 = 2;
			viewpkt->param3 = i;
			player_send_packet(player, viewpkt, NULL);
		}
	}
	else
	{
		//send only the changed view packets

	}

	free(viewpkt);
	pthread_mutex_unlock(&player->mutex_player);
	debug("unlocking player mutex in update view");
}


void player_send_chat(PLAYER *player, char *msg, size_t len)
{
	//to do
}