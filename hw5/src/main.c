#include <stdlib.h>

#include "client_registry.h"
#include "maze.h"
#include "player.h"
#include "debug.h"
#include "server.h"
#include "csapp.h"
#include <getopt.h>

static void terminate(int status);

static char *default_maze[] = {
  "******************************",
  "***** %%%%%%%%% &&&&&&&&&&& **",
  "***** %%%%%%%%%        $$$$  *",
  "*           $$$$$$ $$$$$$$$$ *",
  "*##########                  *",
  "*########## @@@@@@@@@@@@@@@@@*",
  "*           @@@@@@@@@@@@@@@@@*",
  "******************************",
  NULL
};


void sigup_handler(int sig)
{
    terminate(EXIT_SUCCESS);
}

void sigpipe_handler(int sig)
{
    //nothing to be done
}



CLIENT_REGISTRY *client_registry;

void *test_client(void* vargs)
{
    int* rec = (int*)vargs;
    creg_register(client_registry, *rec);
    sleep(5);
    creg_unregister(client_registry, *rec);
    return NULL;
}

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry,
    // maze, and player modules.
    client_registry = creg_init();
    player_init();
    debug_show_maze = 1;  // Show the maze after each packet.

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function mzw_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    int ret;
    char* portnum = 0;
    char* temp_file = NULL;
    while((ret = getopt(argc, argv, "p:t:")) != -1)
    {
        switch(ret)
        {
            case 'p':
                portnum = optarg;
                break;
            case 't':
                temp_file = optarg;
                break;
        }
    }

    if (temp_file != NULL)
    {
        FILE *f = fopen(temp_file, "r");
        if (f == NULL)
            terminate(EXIT_FAILURE);

        size_t len = 0;
        ssize_t nread;
        char* line = NULL;
        int numofrows = 0;
        while ((nread = getline(&line, &len, f)) != -1) {
               numofrows++;
           }
        fclose(f);
        f = fopen(temp_file, "r");
        char* inputmaze[numofrows+1];
        int i = 0;
        while ((nread = getline(&line, &len, f)) != -1) {
               char* newptr = malloc(sizeof(char)*(strlen(line) - 1));
               memcpy(newptr, line, strlen(line) - 1);
               inputmaze[i] = newptr;
               i++;
           }
        inputmaze[numofrows] = NULL;

        maze_init(inputmaze);
        for (i = 0; i < numofrows; i++)
        {
            free(inputmaze[i]);
        }
        free(line);
    }
    else
        maze_init(default_maze);

    if (portnum == NULL) terminate(EXIT_FAILURE);

    struct sigaction sa;
    sa.sa_handler = sigup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGHUP, &sa, NULL) == -1)
        perror("Sigaction Signal error");

    struct sigaction sapipe;
    sapipe.sa_handler = sigpipe_handler;
    sigemptyset(&sapipe.sa_mask);
    sapipe.sa_flags = SA_RESTART;
    if (sigaction(SIGPIPE, &sapipe, NULL) == -1)
        perror("Sigaction Signal error");

    int server_socket;

    server_socket = open_listenfd(portnum);
    if (server_socket < 0) terminate(EXIT_FAILURE);


    //testcode for checking client registry
    /*
    int n = 6;
    pthread_t ti;
    while(n-- > 1)
    {
        int* send = malloc(sizeof(int));
        *send = n*2;
        Pthread_create(&ti, NULL, test_client, send);
    }

    creg_wait_for_empty(client_registry);
    exit(1);
    //
    */


    int* clientfd;
    pthread_t tid;
    while(1)
    {
        clientfd = malloc(sizeof(int));
        *clientfd = Accept(server_socket, NULL, NULL);
        if (*clientfd < 0)
        {
            free(clientfd);
            //terminate(EXIT_FAILURE);
        }
        Pthread_create(&tid, NULL, mzw_client_service, clientfd);

    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the MazeWar server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    player_fini();
    maze_fini();

    debug("MazeWar server terminating");
    exit(status);
}
