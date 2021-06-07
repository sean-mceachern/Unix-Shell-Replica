// 
// tsh - A tiny shell program with job control
// 
// Sean McEachern - semc2265
//

using namespace std;

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char **argv) 
{
    int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
    dup2(1, 2);

  /* Parse the command line */
    char c;
    while ((c = getopt(argc, argv, "hvp")) != EOF) 
    {
        switch (c) 
        {
            case 'h':             // print help message
                usage();
                break;
            case 'v':             // emit additional diagnostic info
                verbose = 1;
                break;
            case 'p':             // don't print a prompt
                emit_prompt = 0;  // handy for automatic testing
                break;
            default:
                usage();
        }
    }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
    Signal(SIGINT,  sigint_handler);   // ctrl-c
    Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
    Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
    Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
    initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
    for(;;) 
    {
    //
    // Read command line
    //
        if(emit_prompt) 
        {
            printf("%s", prompt);
            fflush(stdout);
        }

        char cmdline[MAXLINE];

        if((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) 
        {
            app_error("fgets error");
        }
    //
    // End of file? (did user type ctrl-d?)
    //
        if(feof(stdin)) 
        {
            fflush(stdout);
            exit(0);
        }

    //
    // Evaluate command line
    //
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
    char *argv[MAXARGS];
  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
    int bg = parseline(cmdline, argv); 
    
    if(argv[0] == NULL) return;
    
    pid_t pid;
    
    /* to be used for sending the desired signal blocking/unblocking */
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);

  
    /* If first arg is a builtind command, run it and return true */
    /* sigprocmask is used to block and unblock selected signals */ 
    if(!builtin_cmd(argv)) 
    {
        sigprocmask(SIG_BLOCK, &mask, NULL);

        if(bg) /* background jobs */
        {
            if((pid = fork()) == 0) /* child, runs user job */
            {
                
                sigprocmask(SIG_UNBLOCK, &mask, NULL); /* allows BG child to run process */
                setpgid(0, 0);
                
                
                /* invalid command line input */
                if(execv(argv[0], argv) < 0)/* execv = loads and runs a new program in the child process. only returns if there is an error (-1). */
                {
                    printf("%s: Command not found\n", argv[0]);
                    exit(0);
                } 
            }  
            addjob(jobs, pid, BG, cmdline);
            struct job_t* job = getjobpid(jobs, pid);
            printf("[%d] (%d) %s",job->jid,job->pid,job->cmdline);
        }

        else /* fg jobs */
        {
            if((pid = fork()) == 0) /* child, runs user job */
            {
                sigprocmask(SIG_UNBLOCK, &mask, NULL); /* allows FG child to run process */
                setpgid(0, 0);
                
                
                /* invalid command line input */
                if(execv(argv[0], argv) < 0)/* execv = loads and runs a new program in the child process. only returns if there is an error (-1). */
                {
                    printf("%s: Command not found\n", argv[0]);
                    exit(0);
                }
                return;
            }
            
            /* parent add jobs to fg */
            addjob(jobs, pid, FG, cmdline);
            sigprocmask(SIG_UNBLOCK, &mask, NULL); /* allows parent process to run job. */ 
            waitfg(pid);
        }
        
    }
    return;
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//

/* compare argument[0] with desired commnands*/
int builtin_cmd(char **argv) 
{
    string cmd(argv[0]);

    if(cmd == "quit")
    {
        exit(0);
    }
    if(cmd == "fg")
    {
        do_bgfg(argv);
        return 1;
    }
    if(cmd == "bg")
    {
        do_bgfg(argv);
        return 1;
    }
    if(cmd == "jobs")
    {
        listjobs(jobs);
        return 1;
    }
    return 0;

}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
    struct job_t *jobp = NULL;

      /* Ignore command if no argument */
    if (argv[1] == NULL) 
    {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }
    
  /* Parse the required PID or %JID arg */
    if (isdigit(argv[1][0])) 
    {
        pid_t pid = atoi(argv[1]);
        if (!(jobp = getjobpid(jobs, pid))) 
        {
            printf("(%d): No such process\n", pid);
            return;
        }
    }
    
    else if (argv[1][0] == '%') 
    {
        int jid = atoi(&argv[1][1]);
        if (!(jobp = getjobjid(jobs, jid))) 
        {
            printf("%s: No such job\n", argv[1]);
            return;
        }
    }	    
    else
    {
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
    
    
    /* executes the desired FG or BG commandline. If FG, the terminal waits until process terminates. if BG, print out job info.  */
    string cmd(argv[0]);
    if(strcmp(argv[0], "bg") == 0) 
    {
        jobp->state = BG; /* Change (FG -> BG) or (ST ->  BG) */
        kill(-jobp->pid, SIGCONT); /* parent sends continue signal to desired job. negative sign send signal to every process in the group */
        printf("[%d] (%d) %s", jobp->jid, jobp->pid, jobp->cmdline); /* prints the BG jobs */
    } 
    if((strcmp(argv[0], "fg")) == 0)
    {
        jobp->state = FG; /* Change (BG -> FG) or (ST -> FG) */
        kill(-jobp->pid, SIGCONT);/* parent sends continue signal to desired job. negative sign send signal to every process in the group */
        waitfg(jobp->pid); /* Terminal waits for fg job to finish */
    }
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//

/* passed a PID, if it is a foreground job sleep until job terminates.  */
void waitfg(pid_t pid)
{
    while(pid == fgpid(jobs))
    {
        sleep(0);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//



/* If something happens to the child this is where the notification goes. */
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;    
    
    
    
    /* checking the status of a reaped child process */ 
    /* suspends execution of a process until child process changes from wait until terminated (or stopped) */
    /* then the program modifies the behavior of the deletion process depending on the exit status of the terminated child */
    
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) 
    /* WNOHANG allows proccess to continue while waiting for the child to terminate */
    /* WUNTRACED also terminates for children in the "stop" status. while the default would normally only do this for the "terminated" status */
    
    {
        if(WIFEXITED(status)) /* exit status of the child that terminated normally */
        { 
            deletejob(jobs, pid); /* reaps */
		}
        
		if(WIFSIGNALED(status)) /* exit status of the child that terminated because the signal was not caught */
        {
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status)); 
                /* WTERMSIG; if WIFSIGNALED is true this returns the number of the signal that terminated the child */
            
            deletejob(jobs,pid); /* reaps */ 
        }
        
        if(WIFSTOPPED(status)) /* checks if the child was "stopped" */
        {
            getjobpid(jobs, pid)->state = ST; /* Change job status to ST (stopped) */
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
                /* returns the signal number of the signal that caused the child to stop */
        }   
    }
    
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//
void sigint_handler(int sig) 
{        
    pid_t pid = fgpid(jobs);
    
    /* sends the SIGINT (interrupt signal) to all running processes */ 
    if(pid > 0)
    {
        kill(-pid, SIGINT);
    }
    
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//



// int sig = ID of the signal. refer to slides. sigtstp = signal 20.
// want to stop the fg process not tsh
    // to do this figure out what the fg process is.
    // look at fgpid() in the jobs.cc file.
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs);  
    
    /* sends the SIGTSTP (terminal stop signal) to all processes */
    if (pid != 0) 
    { 
        kill(-pid, SIGTSTP); /* this will stop the signal from terminal */
    }  
    return;
}

/*********************
 * End signal handlers
 *********************/