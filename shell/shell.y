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
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>
#include <unistd.h>
#include <signal.h>
#include "sys/wait.h"
#include <regex.h>
#include <dirent.h>
#include <algorithm>
#include <sys/types.h>
#include <cstring>

#define MAXFILENAME 1024
#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN NEWLINE PIPE GREAT LESS GREATAMPERSAND GREATGREAT GREATGREATAMPERSAND AMPERSAND TWOGREAT EXIT AMBIGUOUS

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"

void yyerror(const char * s);
int yylex();
bool error;
void expandIf(std::string *arg);
void expand(char *prefix, char *suffix);
bool cmp(char *n, char *m);

static std::vector<char *> sortArg = std::vector<char *>();
static bool wildCard;

%}

%%
goal:
  command_list
  ;

command_list:
  command_line
  | command_list command_line
  ;

command_line:
  pipe_list io_modifier_list background_optional NEWLINE {
    if (!error) {
      if (isatty(0)) {
        printf("   Yacc: Execute command\n");
      }
      Shell::_currentCommand.execute();
    }
    error = false;
  }
  | NEWLINE {
    Shell::_currentCommand.execute();
  }
  | error NEWLINE { yyerrok; }
  ;

cmd_and_args:
  EXIT {
    printf("Good bye!!\n");
    exit(1);
  }
  | AMBIGUOUS {
    printf("Ambiguous output redirect.\n");
    yyerrok;
    error = true;
    if (isatty(0)) {
      Shell::prompt();
    }
  }
  | WORD {
    if (isatty(0)) {
      printf("   Yacc: insert command \"%s\"\n", $1->c_str());
    }
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  arg_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

arg_list:
  arg_list WORD {
    if (isatty(0)) {
      printf("   Yacc: insert argument \"%s\"\n", $2->c_str());
    }
    wildCard = false;
    char *p = (char *)"";
    expand(p, (char *)($2->c_str()));
    std::sort(sortArg.begin(), sortArg.end(), cmp);
    for (auto i: sortArg) {
      std::string *insert = new std::string(i);
      Command::_currentSimpleCommand->insertArgument( insert );
    }
    sortArg.clear();
  }
  |
  ;

pipe_list:
  cmd_and_args
  | pipe_list PIPE cmd_and_args
  ;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  |
  ;

io_modifier:
  GREATGREAT WORD {
    if (isatty(0)) {
      printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._append = true;
  }
  | GREAT WORD {
    if (isatty(0)) {
      printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREATAMPERSAND WORD {
    if (isatty(0)) {
      printf("   Yacc: insert output \"%s\"\n", $2->c_str());
      printf("   Yacc: insert error \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._append = true;
  }
  | GREATAMPERSAND WORD {
    if (isatty(0)) {
      printf("   Yacc: insert output \"%s\"\n", $2->c_str());
      printf("   Yacc: insert error \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | LESS WORD {
    if (isatty(0)) {
      printf("   Yacc: insert input \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._inFile = $2;
  }
  | TWOGREAT WORD {
    if (isatty(0)) {
      printf("   Yacc: insert input \"%s\"\n", $2->c_str());
    }
    Shell::_currentCommand._errFile = $2;
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier
  |
  ;

%%

bool cmp(char *n, char *m) { return strcmp(n, m) < 0; }

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

void expandIf(std::string *arg) {
  char * arg_c = (char *)arg->c_str();
  char * a;
  std::string path;
  if (strchr(arg_c,'?')==NULL & strchr(arg_c,'*')==NULL) {
    Command::_currentSimpleCommand->insertArgument(arg);
    return;
  }
  DIR * dir;
  if (arg_c[0] == '/') {
    std::size_t found = arg->find('/');
    while (arg->find('/',found+1) != -1)
      found = arg->find('/', found+1);

    path = arg->substr(0, found+1);
    a = (char *)arg->substr(found+1, -1).c_str();
    dir = opendir(path.c_str());
  }
  else {
    dir = opendir(".");
    a = arg_c;
  }
  if (dir == NULL) {
    perror("opendir");
    return;
  }
  char * reg = (char*)malloc(2*strlen(arg_c)+10);
  char * r = reg;
  *r = '^'; r++;
  while (*a) {
    if (*a == '*') {*r='.'; r++; *r='*'; r++;}
    else if (*a == '?') {*r='.'; r++;}
    else if (*a == '.') {*r='\\'; r++; *r='.'; r++;}
    else {*r=*a; r++;}
    a++;
  }
  *r='$'; r++; *r=0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  if (expbuf != 0) {
    perror("regcomp");
    return;
  }

  std::vector<char *> sortArgu = std::vector<char *>();
  struct dirent * ent;
  while ( (ent=readdir(dir)) != NULL) {
    if (regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
      if (reg[1] == '.') {
        if (ent->d_name[0] != '.') {
          std::string name(ent->d_name);
          name = path + name;
          sortArgu.push_back(strdup((char *)name.c_str()));
        }
      } else {
        std::string name(ent->d_name);
        name = path + name;
        sortArgu.push_back(strdup((char *)name.c_str()));
      }
    }
  }

  closedir(dir);
  regfree(&re);

  std::sort(sortArgu.begin(), sortArgu.end(), cmp);

  for (auto a: sortArgu) {
    std::string * argToInsert = new std::string(a);
    Command::_currentSimpleCommand->insertArgument(argToInsert);
  }

  sortArgu.clear();
}

void expand(char *prefix, char *suffix) {
  if (suffix[0] == 0) {
    sortArg.push_back(strdup(prefix));
    return;
  }
  char Prefix[MAXFILENAME + 1];
  if (prefix[0] == 0) {
    if (suffix[0] == '/') {suffix += 1; sprintf(Prefix, "%s/", prefix);}
    else strcpy(Prefix, prefix);
  }
  else
    sprintf(Prefix, "%s/", prefix);

  char * s = strchr(suffix, '/');
  char component[MAXFILENAME];
  if (s != NULL) {
    strncpy(component, suffix, s-suffix);
    component[s-suffix] = 0;
    suffix = s + 1;
  }
  else {
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  char newPrefix[MAXFILENAME + 1];
  if (strchr(component,'?')==NULL & strchr(component,'*')==NULL) {
    if (Prefix[0] == 0) strcpy(newPrefix, component);
    else sprintf(newPrefix, "%s/%s", prefix, component);
    expand(newPrefix, suffix);
    return;
  }

  char * reg = (char*)malloc(2*strlen(component)+10);
  char * r = reg;
  *r = '^'; r++;
  int i = 0;
  while (component[i]) {
    if (component[i] == '*') {*r='.'; r++; *r='*'; r++;}
    else if (component[i] == '?') {*r='.'; r++;}
    else if (component[i] == '.') {*r='\\'; r++; *r='.'; r++;}
    else {*r=component[i]; r++;}
    i++;
  }
  *r='$'; r++; *r=0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);

  char * dir;
  if (Prefix[0] == 0) dir = (char*)"."; else dir = Prefix;
  DIR * d = opendir(dir);
  if (d == NULL) {
    return;
  }
  struct dirent * ent;
  bool find = false;
  while ((ent = readdir(d)) != NULL) {
    if(regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
      find = true;
      if (Prefix[0] == 0) strcpy(newPrefix, ent->d_name);
      else sprintf(newPrefix, "%s/%s", prefix, ent->d_name);

      if (reg[1] == '.') {
        if (ent->d_name[0] != '.') expand(newPrefix, suffix);
      } else
        expand(newPrefix, suffix);
    }
  }
  if (!find) {
    if (Prefix[0] == 0) strcpy(newPrefix, component);
    else sprintf(newPrefix, "%s/%s", prefix, component);
    expand(newPrefix, suffix);
  }
  closedir(d);
  regfree(&re);
  free(reg);
}

#if 0
main()
{
  yyparse();
}
#endif
