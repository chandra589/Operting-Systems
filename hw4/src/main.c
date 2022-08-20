#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <getopt.h>

#include "debug.h"
#include "cookbook.h"
#include "workqueue.h"

extern RECIPEQUEUELIST *workhead;
extern RECIPEQUEUELIST *waithead;
extern RECIPEQUEUELIST *worktail;
extern RECIPEQUEUELIST *waittail;

volatile int active_cooks = 0;
volatile sig_atomic_t allstepsdone = 0;
volatile sig_atomic_t step_successful = 1;
volatile sig_atomic_t cook_successful = 1;
volatile int total_steps = 0;


void child_handler(int sig)
{
    //debug("In_child_handler");
    int olderr = errno;
    int child_status;
    pid_t pid;
    sigset_t mask_all, prev_all;
    sigemptyset(&mask_all);
    sigaddset(&mask_all, SIGCHLD);
    //printf("error is %d\n", errno);
    while ((pid = waitpid(-1, &child_status, WNOHANG)) > 0) { /* Reap child */
        //debug("child pid is %d\n", pid);
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if (WIFEXITED(child_status))
        {
            int exit_status = WEXITSTATUS(child_status);
            if (exit_status != 0) cook_successful = 0;
        }
        else{
            cook_successful = 0;
        }
        Add_DependentItems_toworkqueue(pid); //change the state of this pid recipe in work queue and add recipes which are dependent on this.
        active_cooks--;
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    errno = olderr;
}

void child_handler_steps(int sig)
{
    //debug("In child_handler_steps");
    int child_status;
    int olderr = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    sigfillset(&mask_all);
    while (total_steps > 0 && (pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if (WIFEXITED(child_status))
        {
            int exit_status = WEXITSTATUS(child_status);
            if (exit_status != 0) step_successful = 0;
        }
        else{
            step_successful = 0;
        }
        if(pid < 0)
            perror("wait error");
        total_steps--;
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    if(total_steps == 0)
        allstepsdone = 1;
    errno = olderr;
}

int main(int argc, char *argv[]) {
    COOKBOOK *cbp;
    int err = 0;
    char *cookbook = "rsrc/cookbook.ckb";
    FILE *in;
    int max_cooks = 1; //default


    int ret;
    while((ret = getopt(argc, argv, "f:c:")) != -1)
    {
        switch(ret)
        {
            case 'f':
                cookbook = optarg;
                break;
            case 'c':
                max_cooks = atoi(optarg);
                break;
        }
    }

    char* recipetomake = NULL;
    recipetomake = argv[optind];

    if((in = fopen(cookbook, "r")) == NULL) {
	fprintf(stderr, "Can't open cookbook '%s': %s\n", cookbook, strerror(errno));
	exit(1);
    }
    cbp = parse_cookbook(in, &err);
    if(err) {
	fprintf(stderr, "Error parsing cookbook '%s'\n", cookbook);
	exit(1);
    }

    if (recipetomake == NULL)
        recipetomake = cbp->recipes->name;

    RECIPE *it = cbp->recipes;
    bool recipefound = false;
    while(it != NULL) {
        if (!strcmp(it->name, recipetomake)){
            recipefound = true;
            break;
        }
        it = it->next;
    }

    if (!recipefound) exit(EXIT_FAILURE);

    AllocatememoryforState(cbp->recipes);
    TraverseRecipe(it);

    pid_t pid;
    sigset_t mask_all, mask_child, prev_one, empty;
    sigfillset(&mask_all);
    sigemptyset(&mask_child);
    sigemptyset(&empty);
    sigaddset(&mask_child, SIGCHLD);

    struct sigaction sa;
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        perror("Sigaction Signal error");


    while (!CheckAllRecipesareProcessed())
    {
        RECIPEQUEUELIST* currentnode = GetRecipetobeprocessed();
        RECIPE* recipetobeprocessed = NULL;
        if (currentnode != NULL)
            recipetobeprocessed = currentnode->recipe;

        if (active_cooks < max_cooks && recipetobeprocessed)
        {
            sigprocmask(SIG_BLOCK, &mask_child, &prev_one); /* Block SIGCHLD */
            if ((pid = fork()) == 0)
            {
                //cook associated with a specific recipe
                sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD in the child process*/

                struct sigaction sa1;
                sa1.sa_handler = child_handler_steps;
                sigemptyset(&sa1.sa_mask);
                sa1.sa_flags = SA_RESTART;
                if (sigaction(SIGCHLD, &sa1, NULL) == -1)
                    perror("Sigaction Signal error");

                TASK *task = recipetobeprocessed->tasks;
                while(task != NULL)
                {
                    allstepsdone = 0;
                    STEP *step = task->steps;
                    STEP *itstep = step;
                    int numofsteps = 0;
                    while(itstep != NULL){
                        numofsteps++;
                        itstep = itstep->next;
                    }

                    pid_t step_pids[numofsteps];
                    total_steps = numofsteps;

                    //debug("Task currently processed is %s\n", *(step->words));
                    int pipearray[2*(numofsteps-1)];
                    for(int i = 0; i < numofsteps - 1; i++)
                    {
                        if (pipe(pipearray + i*2) < 0){
                            perror("piping unsuccessful");
                            exit(EXIT_FAILURE);
                        }
                    }

                    for (int i = 0; i < numofsteps; i++)
                    {
                        //pid_t step_pid;
                        if ((step_pids[i] = fork()) == 0)
                        {
                            if (i == 0)
                            {
                                if (task->input_file != NULL)
                                {
                                    int fdin = open(task->input_file, O_RDONLY);
                                    dup2(fdin, 0);
                                }
                            }
                            if (i != 0)
                            {
                                dup2(pipearray[(i-1)*2], 0);
                            }

                            if (i != numofsteps - 1)
                            {
                                dup2(pipearray[(i*2) + 1], 1);
                            }
                            if (i == numofsteps-1)
                            {
                                if (task->output_file != NULL)
                                {
                                    int fdout = open(task->output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                                    dup2(fdout, 1);
                                }
                            }
                            //close all pipes
                            for (int j = 0; j < 2*(numofsteps-1); j++)
                               close(pipearray[j]);

                            char** words = step->words;
                            char* command = *words;
                            char path[] = "./util/";
                            char* cpath = (char*)calloc(strlen(command) + strlen(path), sizeof(char));
                            strcat(cpath, path);
                            strcat(cpath, command);
                            //debug("cpath is %s\n", cpath);
                            int ret = execvp(cpath, words);
                            if (ret == -1)
                                execvp(command, words);
                            exit(EXIT_FAILURE);
                        }

                        step = step->next;
                    }

                    //close all pipes in parent
                    for (int i = 0; i < 2*(numofsteps-1); i++)
                        close(pipearray[i]);

                    while(!allstepsdone)
                    {
                        //debug("in first sigsuspend and number of steps is %d\n", total_steps);
                        sigsuspend(&empty);
                    }

                    if (step_successful == 1)
                        task = task->next;
                    else
                        task = NULL; //should not process anymore tasks as the previous task failed(one of its steps failed)
                }
                if (step_successful == 1)
                    exit(EXIT_SUCCESS);
                else
                    exit(EXIT_FAILURE);
            }
            sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
            currentnode->pid = pid;
            *((int*)(recipetobeprocessed->state)) = IN_PROCESSING;
            active_cooks++; /* Increase number of cooks */
            sigprocmask(SIG_SETMASK, &prev_one, NULL);
        }
        else
        {
            //debug("in last sigsuspend");
            sigsuspend(&empty);
        }

        if (cook_successful == 0)
        {
            freestatememoryofrecipe(cbp->recipes);
            exit(EXIT_FAILURE);
        }
    }

    //unparse_cookbook(cbp, stdout);

    freestatememoryofrecipe(cbp->recipes);
    exit(EXIT_SUCCESS);
}
