#ifndef PEACHCOMPILER_H
#define PEACHCOMPILER_H

#include <stdio.h>
#include <stdbool.h>

// permet de récupérer les détails la ligne d'un token
struct pos
{
  int line;
  int col;
  const char *filename;
};

/**
 * Notion d'hierachie entre les type qu'il a été bien de coder
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
    const char *sval;
    unsigned int inum;
    unsigned long lnum;
    unsigned long long llnum;
    void *any;
  };

  // c'est pour signifier qu'il y a un espace entre ce token et le prochain
  // ? concretement à quoi cela sert ? //
  bool whitespace;

  // (5+10+20), si le token en cours de traitement est 5 alors,
  // between bracket sera donc 5+10+20
  const char *between_brackets;
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
  struct compile_process_input_file
  {
    FILE *fp;
    const char *abs_path;
  } cfile;
  FILE *ofile;
};
int compile_file(const char *filename, const char *out_filename, int flags);
struct compile_process *compile_process_create(const char *filename, const char *filename_out, int flags);
#endif