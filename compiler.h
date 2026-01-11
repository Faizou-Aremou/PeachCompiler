#ifndef PEACHCOMPILER_H
#define PEACHCOMPILER_H

#include <stdio.h>
#include <stdbool.h>
#include "helpers/vector.h"
#include <stdlib.h>

// permet de récupérer les détails la ligne d'un token
struct pos
{
  int line;
  int col;
  const char* filename;
};
struct lex_process;
typedef char (*LEX_PROCESS_NEXT_CHAR)(struct lex_process* process);
typedef char (*LEX_PROCESS_PEEK_CHAR)(struct lex_process* process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lex_process* process, char c);
struct lex_process_functions
{
  LEX_PROCESS_NEXT_CHAR next_char;
  LEX_PROCESS_PEEK_CHAR peek_char;
  LEX_PROCESS_PUSH_CHAR push_char;
};

/**
 * Aggregat représentant une procedure de lexer
 */
struct lex_process
{
  struct pos pos;
  struct vector* token_vec;
  struct compile_process* compiler;
  /**
   * ?le nombre de parenthèse déja enregistré?
   * ((50))
   */
  int current_expression_count;
  /*
  ??
  */
  struct buffer* parentheses_buffer;
  struct lex_process_functions* functions;

  // This will be private data that th lexer does not understand
  // but the person using the lexer does understand
  void* private;
};

enum {
  LEXICAL_ANALYSIS_ALL_OK,
  LEXICAL_ANALYSIS_INPUT_ERROR
};
/**
 * Notion d'hierachie entre les types qu'il a été bien de coder
 */
enum
{
  TOKEN_TYPE_IDENTIFIER,
  TOKEN_TYPE_KEYWORD,
  TOKEN_TYPE_OPERATOR,
  TOKEN_TYPE_SYMBOL,
  TOKEN_TYPE_NUMBER,
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_COMMENT,
  TOKEN_TYPE_NEWLINE
};
/**
 * type d'un token
 * REGLE METIER
 *  type de token ne corresponds qu'à un type de valeur bien déterminer
 */
struct token
{
  int type;
  int flags;
  union
  { // tout ce qui est dans union est partagé,
    // donc accessible globalement
    // ? je comprends l'idée d'union mais pourquoi c'est accessible globalement ou alors je me trompe?
    char cval;
    const char* sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void* any;
  };

  // c'est pour signifier qu'il y a un espace entre ce token et le prochain
  // ? concretement à quoi cela sert ? //
  bool whitespace;

  // (5+10+20), si le token en cours de traitement est 5 alors,
  // between bracket sera donc 5+10+20
  const char* between_brackets;
};

enum
{
  COMPILER_FILE_COMPILED_OK,
  COMPILER_FAILED_WITH_ERRORS
};
struct compile_process
{
  // the flags in regards to how this file should be compiled
  int flags;
  struct pos pos;
  struct compile_process_input_file
  {
    FILE* fp;
    const char* abs_path;
  } cfile;
  FILE* ofile;
};
int compile_file(const char* filename, const char* out_filename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

char compile_process_next_char(struct lex_process* lex_process);
char compile_process_peek_char(struct lex_process* lex_process);
void compile_process_push_char(struct lex_process* lex_process, char c);

struct lex_process* lex_process_create(struct compile_process* compiler,
  struct lex_process_functions* functions,
  void* private);
void lex_process_free(struct lex_process* process);
/**
 * void* c'est comme any de typescript
 */
void* lex_process_private(struct lex_process* process);
struct vector* lex_process_tokens(struct lex_process* process);
int lex(struct lex_process* process);
#endif