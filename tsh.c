/* 
 * tsh - A tiny shell program with job control
 * 
 * Sandra Shtabnaya (Meta Genie)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "util.h"
#include "jobs.h"


/* Global variables */
int verbose = 0;            /* if true, print additional output */

extern char **environ;      /* defined in libc */
static char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
static struct job_t jobs[MAXJOBS]; /* The job list */

/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);


/* Here are helper routines that we've provided for you */
void usage(void);
void sigquit_handler(int sig);
pid_t Fork(void);
void add_job(struct job_t* job);
void delete_job(struct job_t* job);


/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}


/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char* args[MAXARGS];
    int bg = parseline(cmdline, args); // returns true if background job requested.

    // if command is empty line
    if (args[0] == NULL) {
        return;
    }

    // if command is not built in
    if (builtin_cmd(args) == 0) {
        sigset_t mask;
        pid_t pid = Fork();

        // parent process
        if (pid > 0) {

            sigemptyset(&mask); // creates blocked set
            sigaddset(&mask, SIGCHLD); // adds SIGCHILD to set
            sigprocmask(SIG_BLOCK, &mask, NULL); // blocks SIGCHILD signals

            // if background job requested
            if (bg) {
                addjob(jobs, pid, BG, cmdline);
                struct job_t* job = getjobpid(jobs, pid);
                printf("[%d] (%d) %s", job->jid, job->pid , job->cmdline);
            }
            else {
                addjob(jobs, pid, FG, cmdline);
                struct job_t* job = getjobpid(jobs, pid);

                // unblocks SIGCHLD signals, so child can only be reaped after addjob
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
                waitfg(pid); // waits for foreground job to finish

                // if foreground job has not been stopped
                if(job && job->state != ST) {
                    kill(-pid, SIGTSTP);
                    deletejob(jobs, pid); 
                }
            }
        }

        // child process
        else {
            setpgid(0, 0); // puts child in new process group

            // children inherit blocked set. Unblocks SIGCHLD signals
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            if (execve(args[0], args, environ) < 0) {
                printf("%s: Command not found.\n", args[0]);
                exit(0);
            }
        }
    }

    return;
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 * Return 1 if a builtin command was executed; return 0
 * if the argument passed in is *not* a builtin command.
 */
int builtin_cmd(char **argv) 
{
    char* command = argv[0]; // argv[0] contains command

    if (strcmp(command, "quit") == 0) {
        exit(0);
    }
    else if (strcmp(command, "jobs") == 0) {
        listjobs(jobs); // print out a list of all background jobs
        return 1;
    }
    else if (strcmp(command, "bg") == 0) {
        do_bgfg(argv); // runs a given job in the background
        return 1;
    }
    else if (strcmp(command, "fg") == 0) {
        do_bgfg(argv); // runs a given job in the foreground
        return 1;
    }
    else if (strcmp(command, "&") == 0) {
        return 1; // ignores just &
    }

    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    struct job_t* job = NULL;

    // if arguments exist
    if (argv[1] != NULL) {
    
        // if command gives job id
        if (argv[1][0] == '%') {
            int jid = argv[1][1] - '0'; // gets job id, converted to int
            job = getjobjid(jobs, jid); // converts jid to int value

            if (job == NULL) {
                if (strcmp(argv[0], "bg") == 0) {
                    printf("bg ");
                }
                else {
                    printf("fg ");
                }
                printf("%%(%d): No such job\n", jid);
                return;
            }
        }

        // command gives process id
        else {
            
            // if passed pid is not a number
            if (isalpha(argv[1][0])) {
                if (strcmp(argv[0], "bg") == 0) {
                    printf("bg: ");
                }
                else {
                    printf("fg: ");
                }
                printf("argument must be a PID or %%jobid\n");
                return;
            }

            int pid = atoi(argv[1]); // gets process id
            job = getjobpid(jobs, pid); // converts pid to int value

            if (job == NULL) {
                printf("(%d): No such process\n", atoi(argv[1]));
                return;
            }
        }
    }


    if (strcmp(argv[0], "fg") == 0) {
        if (job) {
            pid_t pid = job->pid;

            if (job->state == ST) {
                kill(-pid, SIGCONT); // continue background process, if stopped
            }

            tcsetpgrp(0, pid); // makes process foreground
            job->state = FG; // changes state of process
            waitfg(pid);

            if (job->state != ST) {
                deletejob(jobs, job->pid); // delete foreground process when done
            }
        }
        else {
            printf("fg command requires PID or %%jobid argument\n");
        }
    }
    else if (strcmp(argv[0], "bg") == 0) {
        if (job) {
            pid_t pid = job->pid;
            kill(pid, SIGCONT);
            job->state = BG;
            printf("[%d] (%d) %s", job->jid, pid, job->cmdline);
        }
        else {
            printf("bg command requires PID or %%jobid argument\n");
        }
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    struct job_t* job = getjobpid(jobs, pid); // gets foreground job

    // sleep until job is not foreground process
    while (job->state == FG) {
        sleep(1);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;

    // wait for children to stop or terminate (WUNTRACED) or reap any zombie children (WNOHANG)
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {

        // child terminated normally
        if (WIFEXITED(status)) {
            deletejob(jobs, pid);
        }

        // child exited due to uncaught signal
        else if (WIFSIGNALED(status)) {
            int jid = getjobpid(jobs, pid)->jid;
            deletejob(jobs, pid);
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(status));
        } 

        // child exited due to SIGTSTP. Don't remove from job list
        else if (WIFSTOPPED(status)) {
            struct job_t* job = getjobpid(jobs, pid);
            job->state = ST; // change state to stopped
            printf("Job [%d] (%d) stopped by signal: %d\n", job->jid, pid, WSTOPSIG(status));
        } 
    }
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t pid = fgpid(jobs); // gets PID of current foreground job

    // if a foreground job exists
    if (pid != 0) {
        kill(-pid, sig); // signals all processes in fg group
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs); // gets PID of current foreground job

    // if a foreground job exists
    if (pid != 0) {
        kill(-pid, sig); // signals all processes in fg group
    }
    return;
}

/*********************
 * End signal handlers
 *********************/



/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

pid_t Fork(void) {
  pid_t pid;

  if ((pid = fork()) < 0)
    unix_error("Fork error");

  return pid;
}


