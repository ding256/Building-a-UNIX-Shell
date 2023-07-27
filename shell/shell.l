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

/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%x src

%{

#include <cstring>
#include "y.tab.hh"
#include "shell.hh"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

extern "C" char *read_line();

int mygetc(FILE *f) {
  static char *p;
  char ch;
  if (!isatty(0)) {
    return getc(f);
  }

  if (p == NULL || *p == 0) {
    char *s = read_line();
    p = s;
  }

  ch = *p;
  p++;
  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static  void yyunput (int c, char *buf_ptr);
void myunputc(int c) {
  unput(c);
}

%}

%option noyywrap

%%

"exit" {
  return EXIT;
}

.+>.+>.+ {
  return AMBIGUOUS;
}

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

"|" {
  return PIPE;
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

">&" {
  return GREATAMPERSAND;
}
"&>" {
  return GREATAMPERSAND;
}

">>" {
  return GREATGREAT;
}

">>&" {
  return GREATGREATAMPERSAND;
}
"&>>" {
  return GREATGREATAMPERSAND;
}

"&" {
  return AMPERSAND;
}

"2>" {
  return TWOGREAT;
}

source {
  BEGIN(src);
}
<src>[ \t]*
<src>[^ \t\n]+ {
  FILE *fp = fopen(yytext, "r+");
  if (!fp) {
    perror("fopen");
    exit(1);
  }
  yypush_buffer_state(yy_create_buffer(fp, YY_BUF_SIZE));
  BEGIN(INITIAL);
}
<<EOF>> {
  yypop_buffer_state();
  if (!YY_CURRENT_BUFFER) yyterminate();
}

~[^ \t\n|<&>]* {
  std::string directory = std::string(yytext);
  if (directory.size() == 1) {
    yylval.cpp_string = new std::string(getenv("HOME"));
    return WORD;
  }
  else {
    if (directory[1] == '/') {
      directory = getenv("HOME") + directory.substr(1, std::string::npos);
    }
    else {
      directory = std::string("/homes/") + directory.substr(1);
    }
    yylval.cpp_string = new std::string(directory);
  }
  return WORD;
}

\$\([^\n\$]*\)|'[^\n']*' {
  std::string str = std::string(yytext);
  str = str.substr(2, str.size() - 3);
  str += "\nexit\n";

  int tmpin = dup(0);
  int tmpout = dup(1);
  int pipin[2];
  int pipout[2];

  pipe(pipin);
  pipe(pipout);
  dup2(pipin[0], 0);
  close(pipin[0]);
  dup2(pipout[1], 1);
  close(pipout[1]);

  write(pipin[1], str.c_str(), str.size());
  close(pipin[1]);

  int ret = fork();
  if (ret == 0) {
    char **args = new char*[2];
    args[0] = (char *)"/proc/self/exe";
    args[1] = NULL;
    execvp(args[0], args);
    perror("execvp(subshell");
    exit(1);
  } else if (ret < 0) {
    perror("fork");
    exit(1);
  } else {
    waitpid(ret, NULL, 0);
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    close(tmpin);
    close(tmpout);

    int size = 1024;
    char * buf = new char[size];
    int word = read(pipout[0], buf, size);
    close(pipout[0]);

    for (int i = word - 12; i >= 0; --i) {
      if (buf[i] == '\n') buf[i] = '\t';
      myunputc(buf[i]);
    }
    delete buf;
  }
}

\"[^\"\n]*\" {
  std::string cpp_string = std::string(yytext);
  std::string substr = cpp_string.substr(1, cpp_string.length() - 2);
  yylval.cpp_string = new std::string(substr);
  return WORD;
}

[^ \\\t\n]*(\\[^nt])((\\[^nt])|[^ \\\t\n])* {
  std::string str = std::string(yytext);
  std::size_t found = str.find('\\');
  while (found != -1) {
    str = str.substr(0, found) + str.substr(found + 1, std::string::npos);
    found = str.find('\\', found + 1);
  }
  yylval.cpp_string = new std::string(str);
  return WORD;
}

([^ \t\n]*$\{[^ \}\n]*\}[^ \t\n]*)+ {
  std::string str = std::string(yytext);
  std::string env_var;
  std::size_t find = str.find('$');
  std::size_t find2;
  while (find != -1) {
    find2 = str.find('}', find + 1);
    env_var = getenv(str.substr(find + 2, find2 - find - 2).c_str());
    str = str.substr(0, find) + env_var + str.substr(find2 + 1, std::string::npos);
    find = str.find('$');
  }
  yylval.cpp_string = new std::string(str);
  return WORD;
}

[^ \|\>\t\n]+ {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
