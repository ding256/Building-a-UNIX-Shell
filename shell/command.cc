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
#include <cstdlib>
#include <cstring>

#include <iostream>

#include "command.hh"
#include "shell.hh"
#include "unistd.h"
#include "sys/wait.h"
#include <sys/stat.h>
#include <fcntl.h>

extern char **environ;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile && _outFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;

    _append = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
      if (isatty(0)) {
        Shell::prompt();
      }
      return;
    }

    // Print contents of Command data structure
    if (isatty(0)) {
      print();
    }

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
    int tmpin = dup(0);
    int tmpout = dup(1);
    int tmperr = dup(2);
    int fdin;
    if (_inFile) {
      fdin = open(_inFile->c_str(), O_RDONLY);
    }
    else {
      fdin = dup(tmpin);
    }
    int ret;
    int fdout;
    int fderr;
    unsigned int q;

    for (unsigned int i = 0; i < _simpleCommands.size(); i++) {
      if (isatty(0)) {
        printf(" %-3d:", i);
        for (unsigned int j = 0; j < _simpleCommands[i]->_arguments.size(); j++) {
          printf(" %-3d, %s:", j, _simpleCommands[i]->_arguments[j]->c_str());
        }
      }

      dup2(fdin, 0);
      close(fdin);
      if (i == _simpleCommands.size() - 1) {
        if (_outFile) {
          if (_append) {
            fdout = open(_outFile->c_str(), O_RDWR|O_CREAT|O_APPEND, 0666);
          }
          else {
            fdout = open(_outFile->c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
          }
        }
        else {
          fdout = dup(tmpout);
        }
      }
      else {
        int fdpipe[2];
        pipe(fdpipe);
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }
      if (_errFile && !_outFile) {
        fderr = open(_errFile->c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
      }
      else if (_errFile) {
        fderr = fdout;
      }
      else {
        fderr = dup(tmperr);
      }
      dup2(fdout, 1);
      dup2(fderr, 2);
      close(fdout);
      if (_errFile && !_outFile) {
        close(fderr);
      }

      int last_len = _simpleCommands[i]->_arguments.size();
      char *last_content = strdup(_simpleCommands[i]->_arguments[last_len - 1]->c_str());
      setenv("_", last_content, 1);

      /* Set environment variable */
      if (!strcmp((char *)_simpleCommands[i]->_arguments[0]->c_str(), "setenv")) {
        setenv((char *)_simpleCommands[i]->_arguments[1]->c_str(), (char *)_simpleCommands[i]->_arguments[2]->c_str(), 1);
        clear();
        if (isatty(0)) {
          printf("\n");
          Shell::prompt();
        }
        return;
      }

      /* Unset environment variable */
      if (!strcmp((char *)_simpleCommands[i]->_arguments[0]->c_str(), "unsetenv")) {
        unsetenv((char *)_simpleCommands[i]->_arguments[1]->c_str());
        clear();
        if (isatty(0)) {
          printf("\n");
          Shell::prompt();
        }
        return;

      }

      /* Change directory */
      if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "cd")) {
        if (_simpleCommands[i]->_arguments.size() == 1) {
          chdir(getenv("HOME"));
        }
        else {
          char *path = (char *)_simpleCommands[i]->_arguments[1]->c_str();
          int c_ret = chdir(path);
          if (c_ret != 0) {
            fprintf(stderr, "cd: can't cd to %s\n", path);
          }
        }
        clear();
        if (isatty(0)) {
          Shell::prompt();
        }
        return;
      }

      ret = fork();
      if (ret == 0) {
        char **args =  (char **)malloc((_simpleCommands[i]->_arguments.size() + 1) * sizeof(char *));
        for (q = 0; q < _simpleCommands[i]->_arguments.size(); q++) {
          args[q] = (char *) _simpleCommands[i]->_arguments[q]->c_str();
        }
        args[q] = NULL;

        /* Print environment variable */
        if (strcmp(args[0], "printenv") == 0) {
          int count = 0;
          while (environ[count]) {
            printf("%s\n", environ[count]);
            count++;
          }
          exit(1);
        }

        execvp(args[0], args);
        if (isatty(0)) {
          perror("execvp");
        }
        exit(1);
      }
      else if (ret < 0) {
        perror("fork");
        return;
      }
    }
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    dup2(tmperr, 2);
    close(tmpin);
    close(tmpout);
    close(tmperr);

    if (!_background) {
      int stat;
      waitpid(ret, &stat, 0);
      std::string highlight = std::to_string(WEXITSTATUS(stat));
      setenv("?", highlight.c_str(), 1);
    }
    else {
      std::string highlight = std::to_string(ret);
      setenv("!", highlight.c_str(), 1);
      Shell::pid_list.push_back(ret);
    }
    if (isatty(0)) {
      printf("\n");
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt
    if (isatty(0)) {
      Shell::prompt();
    }
}

SimpleCommand * Command::_currentSimpleCommand;
