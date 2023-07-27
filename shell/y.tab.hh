/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_Y_TAB_HH_INCLUDED
# define YY_YY_Y_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 31 "shell.y"

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

#line 65 "y.tab.hh"

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    WORD = 258,
    NOTOKEN = 259,
    NEWLINE = 260,
    PIPE = 261,
    GREAT = 262,
    LESS = 263,
    GREATAMPERSAND = 264,
    GREATGREAT = 265,
    GREATGREATAMPERSAND = 266,
    AMPERSAND = 267,
    TWOGREAT = 268,
    EXIT = 269,
    AMBIGUOUS = 270
  };
#endif
/* Tokens.  */
#define WORD 258
#define NOTOKEN 259
#define NEWLINE 260
#define PIPE 261
#define GREAT 262
#define LESS 263
#define GREATAMPERSAND 264
#define GREATGREAT 265
#define GREATGREATAMPERSAND 266
#define AMPERSAND 267
#define TWOGREAT 268
#define EXIT 269
#define AMBIGUOUS 270

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 49 "shell.y"

  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;

#line 112 "y.tab.hh"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_HH_INCLUDED  */
