#include "maze.h"
#include "csapp.h"
#include "debug.h"
#include <stdbool.h>

char** maze = NULL;
int rowsize = 0;
int colsize = 0;

pthread_mutex_t mutex_maze;

void maze_init(char **template)
{
	debug("In maze init");
	//rowsize = sizeof(*template);
	rowsize = 0;
	for (int i = 0; template[i] != NULL; i++)
		rowsize++;

	colsize = strlen(*template);

	debug("row size and col size is %d and %d", rowsize, colsize);

	maze = malloc(rowsize * sizeof(char *));

    for (int i = 0; i < rowsize; i++)
    {
        maze[i] = malloc(colsize * sizeof(char));
    }

	for (int i = 0; i <rowsize; i++)
	{
	    for (int j = 0; j < colsize; j++)
	        maze[i][j] = template[i][j];
	}

	pthread_mutex_init(&mutex_maze, NULL);
}


void maze_fini()
{
	debug("In maze cleanup");
	for (int i = 0; i < rowsize; i++)
	{
	    free(maze[i]);
	}

	free(maze);

	pthread_mutex_destroy(&mutex_maze);
}


int maze_get_rows()
{
	return rowsize;
}

int maze_get_cols()
{
	return colsize;
}

int maze_set_player(OBJECT avatar, int row, int col)
{
	int ret = -1;
	debug("locking in maze set player");

	pthread_mutex_lock(&mutex_maze);
	if (IS_EMPTY(maze[row][col]))
	{
		maze[row][col] = avatar;
		ret = 0;
	}

	pthread_mutex_unlock(&mutex_maze);

	debug("unlocked in maze set player");
	return ret;
}


int maze_set_player_random(OBJECT avatar, int *rowp, int *colp)
{
	unsigned *seed = malloc(sizeof(unsigned int));
	*seed = time(NULL) ^ pthread_self();

	for(int i = 0; i < 100000; i++)
	{
		int rowpos = (rand_r(seed) % (rowsize));
		int colpos = (rand_r(seed) % (colsize));
		debug("placing at row %d and col %d",  rowpos, colpos);
		int ret = maze_set_player(avatar, rowpos, colpos);
		if (ret == 0)
		{
			debug("placed successfull");
			*rowp = rowpos;
			*colp = colpos;
			free(seed);
			return 0;
		}
		else
			debug("placing failed");
	}

	free(seed);
	debug("player set failed after trying 100000 times");
	return -1;
}


void maze_remove_player(OBJECT avatar, int row, int col)
{
	debug("locking in maze remove player");
	pthread_mutex_lock(&mutex_maze);

	if (maze[row][col] == avatar)
	{
		debug("removing player %c at row %d and col %d", avatar, row, col);
		maze[row][col] = EMPTY;
	}

	pthread_mutex_unlock(&mutex_maze);
	debug("unlocked in maze remove player");
}


int maze_move(int row, int col, int dir)
{
	int updaterow = row;
	int updatecol = col;
	if (dir == NORTH)
		updaterow-=1;
	else if(dir == SOUTH)
		updaterow+=1;
	else if(dir == WEST)
		updatecol-=1;
	else if(dir == EAST)
		updatecol+=1;


	if (updaterow >= rowsize || updatecol >= colsize || updaterow < 0 || updatecol < 0)
		return -1;

	debug("locking in maze move");
	pthread_mutex_lock(&mutex_maze);
	int ret = -1;

	if (IS_EMPTY(maze[updaterow][updatecol]))
	{
		int avatar = maze[row][col];
		maze[row][col] = EMPTY;
		maze[updaterow][updatecol] = avatar;
		ret = 0;
	}

	pthread_mutex_unlock(&mutex_maze);
	debug("unlocked in maze move");
	return ret;
}


OBJECT maze_find_target(int row, int col, DIRECTION dir)
{
	OBJECT ret = EMPTY;
	int updaterow = row;
	int updatecol = col;

	debug("locking in maze find target");
	pthread_mutex_lock(&mutex_maze);

	while(1)
	{
		if (dir == NORTH)
			updaterow-=1;
		else if(dir == SOUTH)
			updaterow+=1;
		else if(dir == WEST)
			updatecol-=1;
		else if(dir == EAST)
			updatecol+=1;

		if (updaterow >= rowsize || updatecol >= colsize || updaterow < 0 || updatecol < 0)
			break;

		if (IS_AVATAR(maze[updaterow][updatecol]))
		{
			ret = maze[updaterow][updatecol];
			break;
		}
		else if(!IS_EMPTY(maze[updaterow][updatecol]))
		{
			break;
		}
	}

	pthread_mutex_unlock(&mutex_maze);
	debug("unlocked in maze find target");
	return ret;
}


int maze_get_view(VIEW *view, int row, int col, DIRECTION gaze, int depth)
{
	int updaterow = row;
	int updatecol = col;

	int depthallowed = 0;

	while(1)
	{
		if (gaze == NORTH)
			updaterow-=1;
		else if(gaze == SOUTH)
			updaterow+=1;
		else if(gaze == WEST)
			updatecol-=1;
		else if(gaze == EAST)
			updatecol+=1;

		if (updaterow >= rowsize || updatecol >= colsize || updaterow < 0 || updatecol < 0)
			break;
	}

	debug("locking in maze get view");
	pthread_mutex_lock(&mutex_maze);

	if (updaterow == row)
	{
		//gaze towards east or west
		if (gaze == EAST)
		{
			//updatecol will be colsize
			int maxdepth = updatecol - col;

			if (depth >= maxdepth)
				depthallowed = maxdepth;
			else
				depthallowed = depth;

			for (int i = col; i < depthallowed+col; i++)
			{
				//debug("in view filling loop - %c and %c and %c", maze[row-1][i], maze[row][i], maze[row+1][i]);
				(*view)[i-col][0] = maze[row-1][i];
				(*view)[i-col][1] = maze[row][i];
				(*view)[i-col][2] = maze[row+1][i];
			}

			debug("direction of gaze is %d and view depth is %d", gaze, depthallowed);
			show_view(view, depthallowed);
		}
		else if (gaze == WEST)
		{
			//updatecol will be -1
			int maxdepth = col - updatecol;

			if (depth >= maxdepth)
				depthallowed = maxdepth;
			else
				depthallowed = depth;

			for (int i = col; i > col - depthallowed; i--)
			{
				(*view)[col-i][0] = maze[row+1][i];
				(*view)[col-i][1] = maze[row][i];
				(*view)[col-i][2] = maze[row-1][i];
			}

			debug("direction of gaze is %d and view depth is %d", gaze, depthallowed);
			show_view(view, depthallowed);
		}
	}
	else if (updatecol == col)
	{
		//gaze towards south or north
		if (gaze == SOUTH)
		{
			//updaterow will be rowsize
			int maxdepth = updaterow - row;

			if (depth >= maxdepth)
				depthallowed = maxdepth;
			else
				depthallowed = depth;

			for (int i = row; i < depthallowed + row; i++)
			{
				(*view)[i-row][0] = maze[i][col+1];
				(*view)[i-row][1] = maze[i][col];
				(*view)[i-row][2] = maze[i][col-1];
			}

			debug("direction of gaze is %d and view depth is %d", gaze, depthallowed);
			show_view(view, depthallowed);
		}
		else if (gaze == NORTH)
		{
			//updaterow will be -1
			int maxdepth = row - updaterow;

			if (depth >= maxdepth)
				depthallowed = maxdepth;
			else
				depthallowed = depth;

			for (int i = row; i > row - depthallowed; i--)
			{
				(*view)[row-i][0] = maze[i][col-1];
				(*view)[row-i][1] = maze[i][col];
				(*view)[row-i][2] = maze[i][col+1];
			}

			debug("direction of gaze is %d and view depth is %d", gaze, depthallowed);
			show_view(view, depthallowed);
		}
	}

	pthread_mutex_unlock(&mutex_maze);
	debug("unlocked in maze get view");

	return depthallowed;

}

void show_view(VIEW *view, int depth)
{
	debug("In show view");

	for(int i = 0; i < depth; i++)
	{
		printf("%c", (*view)[i][0]);
	}
	printf("\n");
	for(int i = 0; i < depth; i++)
	{
		printf("%c", (*view)[i][1]);
	}
	printf("\n");
	for(int i = 0; i < depth; i++)
	{
		printf("%c", (*view)[i][2]);
	}
	printf("\n");

}

void show_maze()
{
	debug("locking in show maze");

	pthread_mutex_lock(&mutex_maze);

	for (int i = 0; i <rowsize; i++)
	{
	    for (int j = 0; j < colsize; j++)
	    {
	        printf("%c", maze[i][j]);
	    }
    	printf("\n");
	}

	pthread_mutex_unlock(&mutex_maze);

	debug("unlocked in show maze");
}