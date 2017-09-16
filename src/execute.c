/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"
#include <stdio.h>
#include "quash.h"
#include "deque.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)

IMPLEMENT_DEQUE_STRUCT(PIDDeque, pid_t);
IMPLEMENT_DEQUE(PIDDeque, pid_t);

//Create jobs struct
typedef struct Job {
    int job_id;
    char* cmd;
    PIDDeque pid_list;
} Job;

IMPLEMENT_DEQUE_STRUCT(JobDeque, Job);
IMPLEMENT_DEQUE(JobDeque, Job);

//Declare queue of jobs
JobDeque jobs;

//setup execution environment
typedef struct ExecEnv
{
	int num;
	int pfd[2][2];
	Job job;
} ExecEnv;

static Job __new_job() {
	return (Job) {
		0, get_command_string(), new_PIDDeque(1),
	};
}

static void __destroy_job(Job j) {
	if(j.cmd != NULL)
		free(j.cmd);
	destroy_PIDDeque(&j.pid_list);
}

static void __init_exec_env(ExecEnv* e) {
	assert(e != NULL);
	e->job = __new_job();
}

static void __destroy_exec_env(ExecEnv* e)
{
	assert(e != NULL);
	__destroy_job(e->job);
}

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {

    char* cwd = NULL;
    cwd = getcwd(NULL, 0);
    if(cwd == NULL)
    {
        perror("Get working dir. failed!\n");
    }

    *should_free = true;

    return cwd;
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // TODO: Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly

  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status() {
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.
  size_t qlen = length_JobDeque(&jobs);

  for(size_t i = 0; i < qlen; i++)
  {
    Job current = pop_front_JobDeque(&jobs);
    size_t pidQLen = length_PIDDeque(&current.pid_list);

    for(size_t j = 0; j < pidQLen; j++)
    {
      pid_t current_proc = pop_front_PIDDeque(&current.pid_list);
      int status;
      pid_t return_proc = waitpid(current_proc, &status, WNOHANG);

      if (return_proc == -1) {
        perror("Error in process");
      }  else if (return_proc == 0) {
        push_back_PIDDeque(&current.pid_list, current_proc);
      } else if (return_proc == current_proc) {
        print_job_bg_complete(&current.job_id, current_proc, &current.cmd);
      }
    }

    if(!length_PIDDeque(&current.pid_list) == 0)
    {
      push_back_JobDeque(&jobs, current);
    }
   }

  // TODO: Once jobs are implemented, uncomment and fill the following line

}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
	char* exec = cmd.args[0];
	char** args = cmd.args;

	execvp(exec, args);

  perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd) {
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** str = cmd.args;

  while((*str != NULL) && (*(str + 1) != NULL))
  {
		printf("%s ", *str);
		++str;
  }
  if(*str != NULL)
  {
		printf("%s\n", *str);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  const char* dir = cmd.dir;

  // Check if the directory is valid
  if (dir == NULL) {
		perror("ERROR: Failed to resolve path");
		return;
  }

  // TODO: Change directory
  chdir(dir);

  // TODO: Update the PWD environment variable to be the new current working
  // directory and optionally update OLD_PWD environment variable to be the old
  // working directory.
  setenv("OLD_PWD", lookup_env("PWD"), 1);
  setenv("PWD", dir, 1);
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
  int signal = cmd.sig;
  int job_id = cmd.job;

  size_t qlen = length_JobDeque(&jobs);

  for(size_t i = 0; i < qlen; i++)
  {
    Job current = pop_front_JobDeque(&jobs);
    size_t pidQLen = length_PIDDeque(&current.pid_list);
    if(current.job_id == job_id)
    {
      for(size_t j = 0; j < pidQLen; j++)
      {
        pid_t tempID = pop_front_PIDDeque(&current.pid_list);
        kill(tempID, signal);
        push_back_PIDDeque(&current.pid_list, tempID);
      }
    }

    push_back_JobDeque(&jobs, current);
  }
}

// Prints the current working directory to stdout
void run_pwd() {
  // TODO: Print the current working directory
  bool should_free = false;
  char* cwd = get_current_directory(&should_free);
  fprintf(stdout, "%s\n", cwd);

  // Flush the buffer before returning
  fflush(stdout);
  if(should_free)
    free(cwd);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {
  // TODO: Print background jobs
  IMPLEMENT_ME();

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder, ExecEnv* exec_env) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                             // is true

  // TODO: Remove warning silencers
  (void) p_in;  // Silence unused variable warning
  (void) p_out; // Silence unused variable warning
  (void) r_in;  // Silence unused variable warning
  (void) r_out; // Silence unused variable warning
  (void) r_app; // Silence unused variable warning

  // TODO: Setup pipes, redirects, and new process
  //IMPLEMENT_ME();

  //parent_run_command(holder.cmd);
  // This should be done in the parent branch of
  // a fork

  pid_t pid;
  //int p1[2];
  pid = fork();

  if(pid == 0)
  {
    //run child command

    child_run_command(holder.cmd);

    exit(EXIT_SUCCESS);
  }
  else if(pid > 0)
  {
    //populate pid queue, then run parent command
    parent_run_command(holder.cmd);
  }
  else
    exit(EXIT_FAILURE);

}

// Run a list of commands
void run_script(CommandHolder* holders) {

    static bool first = true;

    //holders is just a list of commands
    //ends run_script if there are no scripts to run
    if (holders == NULL)
        return;

    if(first)
    {
        jobs = new_destructable_JobDeque(2, __destroy_job);
        first = false;
    }

    check_jobs_bg_status();

    if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC) {
        end_main_loop();
        return;
    }

    CommandType type; //e.g. export, cd, kill, generic
    ExecEnv env;

    __init_exec_env(&env);

    // Run all commands in the `holder` array
    for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
        create_process(holders[i], &env);

    if (!(holders[0].flags & BACKGROUND)) {
        while(!is_empty_PIDDeque(&env.job.pid_list)) {
            int status = 0;

            if(waitpid(pop_front_PIDDeque(&env.job.pid_list), &status, 0) == -1)
                exit(EXIT_FAILURE);
        }

        __destroy_exec_env(&env);
    }
    else
    {
        if(is_empty_PIDDeque(&jobs))
            env.job.job_id = 1;
        else
            env.job.job_id = peek_back_JobDeque(&jobs).job_id + 1;

        push_back_JobDeque(&jobs, env.job);
        print_job_bg_start(env.job.job_id, peek_front_PIDDeque(&env.job.pid_list),
                           env.job.cmd);
    }
}
