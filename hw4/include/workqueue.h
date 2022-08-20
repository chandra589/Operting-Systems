#ifndef W_QUEUE_H
#define W_QUEUE_H

#include <stdbool.h>
#include <stdlib.h>
#include "cookbook.h"
#include "debug.h"
#include <signal.h>

typedef struct recipequeuelist{
    RECIPE *recipe;
    struct recipequeuelist *next;
    int pid;
}RECIPEQUEUELIST;

#define ALLOCATED 0
#define IN_QUEUE 1
#define IN_WAITLIST 2
#define IN_PROCESSING 3
#define PROCESSED 4

void TraverseRecipe(RECIPE *recipe);
void Add_DependentItems_toworkqueue(pid_t pid);
bool CheckAllRecipesareProcessed();
RECIPEQUEUELIST* GetRecipetobeprocessed();
void AllocatememoryforState(RECIPE* recipe);
void freestatememoryofrecipe(RECIPE* recipe);


#endif