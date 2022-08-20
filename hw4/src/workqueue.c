#include "workqueue.h"

RECIPEQUEUELIST *workhead = NULL;
RECIPEQUEUELIST *waithead = NULL;
RECIPEQUEUELIST *worktail = NULL;
RECIPEQUEUELIST *waittail = NULL;


void TraverseRecipe(RECIPE *recipe)
{
	if (recipe == NULL) return;
	int* state = (int*)(recipe->state);
	if (*state == IN_QUEUE || *state == IN_WAITLIST) return;
	RECIPE_LINK *subrec = recipe->this_depends_on;
	if (subrec != NULL)
	{
    	if (waithead == NULL)
    	{
    		waithead = malloc(sizeof(RECIPEQUEUELIST));
    		waithead->recipe = recipe;
    		waithead->next = NULL;
    		waittail = waithead;
    	}
    	else
    	{
    		RECIPEQUEUELIST *waitnode = malloc(sizeof(RECIPEQUEUELIST));
    		waitnode->recipe = recipe;
    		waitnode->next = waithead;
    		waithead = waitnode;
    	}
    	*((int*)(recipe->state)) = IN_WAITLIST;
    	while(subrec != NULL)
    	{
        	TraverseRecipe(subrec->recipe);
        	subrec = subrec->next;
    	}
	}
    else
    {

        if (workhead == NULL)
        {
        	workhead = malloc(sizeof(RECIPEQUEUELIST));
        	workhead->recipe = recipe;
        	workhead->next = NULL;
        	worktail = workhead;
        }
        else
        {
        	RECIPEQUEUELIST *worknode = malloc(sizeof(RECIPEQUEUELIST));
        	worknode->recipe = recipe;
        	worknode->next = workhead;
        	workhead = worknode;
        }
        *((int*)(recipe->state)) = IN_QUEUE;
    }
}


void Add_DependentItems_toworkqueue(int pid)
{
	//mark this pid as complete and add dependent items from waitlist
	RECIPEQUEUELIST *currentnode = workhead;
	while(currentnode != NULL)
	{
		if (currentnode->pid == pid)
		{
			//debug("Recipe completed is %s\n", currentnode->recipe->name);
			RECIPE* complete = currentnode->recipe;
			*((int*)(complete->state)) = PROCESSED;
			RECIPE_LINK *dependslink = complete->depend_on_this;
			while(dependslink != NULL)
			{
				RECIPE *tocheck = dependslink->recipe;
				int* statecheck = tocheck->state;
				if (*statecheck != IN_WAITLIST){	//has to be in waitlist, otherwise recipe need not be cooked
					dependslink = dependslink->next;
					continue;
				}
				bool Allsubrecipesfinished = true;
				RECIPE_LINK* recipedependson = tocheck->this_depends_on;
				while(recipedependson != NULL)
				{
					RECIPE *recipeforstatecheck = recipedependson->recipe;
					int* state = recipeforstatecheck->state;
					if (*state != PROCESSED)
					{
						Allsubrecipesfinished = false;
						break;
					}
					recipedependson = recipedependson->next;
				}

				if (Allsubrecipesfinished)
				{
					//debug("New Recipe added is %s\n", tocheck->name);
					//then add to queue
					*((int*)(tocheck->state)) = IN_QUEUE;
					RECIPEQUEUELIST *worknode = malloc(sizeof(RECIPEQUEUELIST));
        			worknode->recipe = tocheck;
        			worknode->next = NULL;
        			worktail->next = worknode;
        			worktail = worktail->next;
				}

				dependslink = dependslink->next;
			}
			break;
		}
		currentnode = currentnode->next;
	}
}


bool CheckAllRecipesareProcessed()
{
	sigset_t mask_child, prev_one;
    sigemptyset(&mask_child);
    sigaddset(&mask_child, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask_child, &prev_one); /* Block SIGCHLD */
	RECIPEQUEUELIST *currentnode = workhead;
	bool AllRecipesProcessed = true;
	while (currentnode != NULL)
	{
		RECIPE *tocheck = currentnode->recipe;
		int* state = tocheck->state;
		if (*state != PROCESSED)
		{
			AllRecipesProcessed = false;
			break;
		}
		currentnode = currentnode->next;
	}
	sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD*/
	if (AllRecipesProcessed) return true;
	else
		return false;
}

RECIPEQUEUELIST* GetRecipetobeprocessed()
{
	sigset_t mask_child, prev_one;
    sigemptyset(&mask_child);
    sigaddset(&mask_child, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask_child, &prev_one); /* Block SIGCHLD */
	RECIPEQUEUELIST *currentnode = workhead;
	while (currentnode != NULL)
	{
		RECIPE *tocheck = currentnode->recipe;
		int* state = tocheck->state;
		if (*state == IN_QUEUE)
		{
			sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD*/
			return currentnode;
		}
		currentnode = currentnode->next;
	}

	sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD*/
	return NULL;
}


void AllocatememoryforState(RECIPE* recipe)
{
	while(recipe != NULL)
	{
		recipe->state = malloc(sizeof(int));
        *((int*)(recipe->state)) = ALLOCATED;
		recipe = recipe->next;
	}
}

void freestatememoryofrecipe(RECIPE* recipe)
{
	while(recipe != NULL)
	{
		free(recipe->state);
		recipe = recipe->next;
	}
}