/*

 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <unistd.h>

#include "shell.hh"
#include <signal.h>
#include "sys/wait.h"

#ifndef YY_BUF_SIZE
#ifdef __ia64__
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif
#endif

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

int yyparse(void);
void yyrestart(FILE *fp);
//void yypush_buffer_state(YY_BUFFER_STATE buf);
//void yypop_buffer_state();
//YY_BUFFER_STATE yy_create_buffer(FILE *fp, int size);

void Shell::prompt() {
  if (isatty(0)) {
    char *p = getenv("PROMPT");
    if (p) {
      printf("%s", p);
      fflush(stdout);
    }
    else {
      printf("myshell>");
      fflush(stdout);
    }
  }
}

void disp(int sig) {
  sig = sig;
  Shell::_currentCommand.clear();
  if (isatty(0)) {
    printf("\n");
    Shell::prompt();
  }
}

void killZombie(int sig) {
  sig = sig;
  //while(waitpid(-1, 0, WNOHANG) > 0);
  pid_t pid = waitpid(-1, NULL, WNOHANG);
  for (unsigned i = 0; i < Shell::pid_list.size(); i++) {
    if (pid == Shell::pid_list[i]) {
      if (isatty(0)) {
        printf("[%d] exited\n", pid);
        Shell::pid_list.erase(Shell::pid_list.begin() + i);
        break;
      }
    }
  }
}

/*void source(void) {
  std::string s = ".shellrc";
  FILE * in = fopen(s.c_str(), "r");

  if (!in) {
    return;
  }

  yypush_buffer_state(yy_create_buffer(in, YY_BUF_SIZE));
  Shell::_srcCmd = true;
  yyparse();
  yypop_buffer_state();
  fclose(in);
  Shell::_srcCmd = false;
}*/

int main(int argc, char** argv) {
  //source();
  struct sigaction ctrlC;
  ctrlC.sa_handler = disp;
  sigemptyset(&ctrlC.sa_mask);
  ctrlC.sa_flags = SA_RESTART;
  struct sigaction zombie;
  zombie.sa_handler = killZombie;
  sigemptyset(&zombie.sa_mask);
  zombie.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &ctrlC, NULL)){
    perror("sigaction");
    exit(-1);
  }
  else if (sigaction(SIGCHLD, &zombie, NULL)) {
    perror("sigaction");
    exit(-1);
  }

  char rpath[280];
  argc = argc;
  realpath(argv[0], rpath);
  setenv("SHELL", rpath, 1);
  std::string d = std::to_string(getpid());
  setenv("$", d.c_str(), 1);

  Shell::_srcCmd = false;
  if (isatty(0)) {
    Shell::prompt();
  }

  yyrestart(stdin);
  yyparse();
}

Command Shell::_currentCommand;
bool Shell::_srcCmd;
std::vector<int> Shell::pid_list;
