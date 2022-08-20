#include "client_registry.h"
#include "csapp.h"
#include "debug.h"
#include <stdbool.h>

typedef struct fdlist
{
	int fd;
	struct fdlist* next;
}FD_LIST;

typedef struct client_registry
{
	int numofclients;
	sem_t sem;
	pthread_mutex_t mutex;
	FD_LIST* head;
	FD_LIST* tail;
}CLIENT_REGISTRY;


CLIENT_REGISTRY *creg_init()
{
	debug("IN registry initialization");
	CLIENT_REGISTRY *cr = malloc(sizeof(CLIENT_REGISTRY));
	if (!cr) return NULL;
	cr->numofclients = 0;
	int ret1 = sem_init(&cr->sem, 0, 0);
	int ret2 = pthread_mutex_init(&cr->mutex, NULL);
	if (ret1 < 0 || ret2 != 0) {
		free(cr);
		return NULL;
	}
	cr->head = NULL;
	cr->tail = NULL;
	return cr;
}

void creg_fini(CLIENT_REGISTRY *cr)
{
	debug("In registry clear");
	if (!cr) return;
	sem_destroy(&cr->sem);
	pthread_mutex_destroy(&cr->mutex);
	free(cr);
}

void creg_register(CLIENT_REGISTRY *cr, int fd)
{
	debug("In register function\n");
	if (!cr) return;
	if (pthread_mutex_lock(&cr->mutex) < 0) return;
	cr->numofclients++;
	debug("Increased clients by one, current clients is %d\n", cr->numofclients);
	debug("client id is %d\n", fd);
	if (cr->head == NULL)
	{
		//first client
		cr->head = malloc(sizeof(FD_LIST));
		cr->head->fd = fd;
		cr->head->next = cr->tail;
	}
	else
	{
		if (cr->tail == NULL)
		{
			//second client
			cr->tail = malloc(sizeof(FD_LIST));
			cr->tail->fd = fd;
			cr->tail->next = NULL;
			cr->head->next = cr->tail;
		}
		else
		{
			FD_LIST* newnode = malloc(sizeof(FD_LIST));
			newnode->fd = fd;
			newnode->next = NULL;
			cr->tail->next = newnode;
			cr->tail = newnode;
		}

	}

	if (pthread_mutex_unlock(&cr->mutex) < 0) return;
}


void creg_unregister(CLIENT_REGISTRY *cr, int fd)
{
	debug("In unregister function");
	if (!cr) return;
	if (pthread_mutex_lock(&cr->mutex) < 0) return;
	FD_LIST* prev = cr->head;
	FD_LIST* curr = cr->head;
	bool Isfdpresent = false;
	while(curr != NULL){
		if (curr->fd == fd)
		{
			Isfdpresent = true;
			if (curr == cr->head)
			{
				cr->head = cr->head->next;
				free(prev);
			}
			else if (curr == cr->tail)
			{
				prev->next = curr->next;
				free(curr);
				cr->tail = prev;
			}
			else
			{
				prev->next = curr->next;
				free(curr);
			}
			break;
		}
		prev = curr;
		curr = curr->next;
	}

	if (Isfdpresent)
	{
		cr->numofclients--;
		debug("decreased clients by one, current clients is %d", cr->numofclients);
		debug("client id which got freed is %d", fd);
		if (cr->numofclients == 0)
		{
			debug("all clients are unregistered, unlocking now\n");
			V(&cr->sem);
		}
	}

	if (pthread_mutex_unlock(&cr->mutex) < 0) return;
}



void creg_wait_for_empty(CLIENT_REGISTRY *cr)
{
	if (!cr) return;
	if (cr->numofclients == 0) return;
	debug("Waiting\n");
	P(&cr->sem);
	debug("after releasing the lock\n");
}

void creg_shutdown_all(CLIENT_REGISTRY *cr)
{
	if (!cr) return;
	debug("In shutdown all");
	pthread_mutex_lock(&cr->mutex);

	FD_LIST* curr = cr->head;
	while(curr != NULL)
	{
		shutdown(curr->fd, SHUT_RD);
		curr = curr->next;
	}

	pthread_mutex_unlock(&cr->mutex);
}
