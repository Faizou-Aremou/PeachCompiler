/**
 * Contient le metier de l'analyse lexicale en lui même
 */
#include "compiler.h"
#include <string.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>


#define LEX_GETC_IF(buffer, c, exp)     \
     for (c= peekc(); exp ; c = peekc()) \
       {                                \
          buffer_write(buffer, c);      \
          nextc();                      \
       }      

struct token* read_next_token();
static struct lex_process* lex_process; // variable globale
static struct token tmp_token;
static char peekc() {
  return lex_process->functions->peek_char(lex_process);
}

static char nextc() {
  char c = lex_process->functions->next_char(lex_process);
  lex_process->pos.col += 1;
  if (c == '\n') {
    lex_process->pos.line += 1;
    lex_process->pos.col = 1;
  }

  return c;
}

static void pushc(char c) {
  lex_process->functions->push_char(lex_process, c);
}

static struct pos lex_file_position() {
  return lex_process->pos;
}
struct token* token_create(struct token* _token) {
  memcpy(&tmp_token, _token, sizeof(struct token));
  tmp_token.pos = lex_file_position();
  return &tmp_token;
}

static struct token* lexer_last_token() {
  return vector_back_or_null(lex_process->token_vec);
}
static struct  token* handle_whitespace() {
  struct token* last_token = lexer_last_token();

  if (last_token)
  {
    last_token->whitespace = true;
  }

  nextc();
  return read_next_token();

}
const char* read_number_str() {
  const char* num = NULL;
  struct buffer* buffer = buffer_create();
  char c = peekc();
  LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

  buffer_write(buffer, 0x00);
  return buffer_ptr(buffer);
}

unsigned long long read_number() {
  const char* s = read_number_str();
  return atoll(s);
}
struct token* token_make_number_for_value(unsigned long number) {
  return token_create(&(struct token) {
    .type = TOKEN_TYPE_NUMBER,
      .llnum = number
  });
}

struct token* token_make_number() {
  return token_make_number_for_value(read_number());
}

static struct token* token_make_string(char start_delim, char end_delim) {
  struct buffer* buf = buffer_create();

  assert(nextc() == start_delim);
  char c = nextc();
  for (; c != end_delim && c != EOF; c = nextc()) {
    if (c == "\\") {
      // We need to handle an escape character.
      continue;
    }

    buffer_write(buf, c);
  }
  buffer_write(buf, 0x00);
  return token_create(&(struct token) { .type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf) });
}

static bool op_treated_as_one(char op) {
  return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' || op == '?';
}
static bool is_single_operator(char op) {
  return op == '+' ||
    op == '-' ||
    op == '/' ||
    op == '*' ||
    op == '=' ||
    op == '>' ||
    op == '<' ||
    op == '|' ||
    op == '&' ||
    op == '^' ||
    op == '%' ||
    op == '~' ||
    op == '!' ||
    op == '(' ||
    op == '[' ||
    op == ',' ||
    op == '.' ||
    op == '?';
}
static bool op_valid(const char* op) {
  return S_EQ(op, "+") ||
    S_EQ(op, "-") ||
    S_EQ(op, "*") ||
    S_EQ(op, "/") ||
    S_EQ(op, "!") ||
    S_EQ(op, "^") ||
    S_EQ(op, "=") ||
    S_EQ(op, "!=") ||
    S_EQ(op, "+=") ||
    S_EQ(op, "-=") ||
    S_EQ(op, "*=") ||
    S_EQ(op, "/=") ||
    S_EQ(op, ">>") ||
    S_EQ(op, "<<") ||
    S_EQ(op, ">=") ||
    S_EQ(op, "<=") ||
    S_EQ(op, ">") ||
    S_EQ(op, "<") ||
    S_EQ(op, "==") ||
    S_EQ(op, "++") ||
    S_EQ(op, "--") ||
    S_EQ(op, "&&") ||
    S_EQ(op, "|") ||
    S_EQ(op, "->") ||
    S_EQ(op, ".") ||
    S_EQ(op, "...") ||
    S_EQ(op, "~") ||
    S_EQ(op, "||") ||
    S_EQ(op, "(") ||
    S_EQ(op, "[") ||
    S_EQ(op, "%");

}

static void read_op_flush_back_keep_first(struct buffer* buffer) {
  const char* data = buffer_ptr(buffer);
  int len = buffer->len;
  for (int i = len - 1; i >= 1; i--) {
    if (data[i] == 0x00) { continue; }
    pushc(data[i]); // Remet le caractère dans le flux d'entrée
  }
}
const char* read_op() {
  bool single_operator = true;
  char op = nextc(); // Lit le premier caractère
  struct buffer* buf = buffer_create();
  buffer_write(buf, op);

  if (!op_treated_as_one(op)) {
    char next_c = peekc();
    if (is_single_operator(next_c)) {
      buffer_write(buf, next_c);
      nextc(); // Consomme le deuxième caractère
      single_operator = false;
    }
  }

  buffer_write(buf, 0x00);
  char* ptr = buffer_ptr(buf);

  if (!single_operator && !op_valid(ptr)) {
    read_op_flush_back_keep_first(buf);
    ptr[1] = 0x00; // On ne garde que le premier caractère
  }
  else if (!op_valid(ptr)) {
    compiler_error(lex_process->compiler, "The operator %s is not valid\n", ptr);
  }
  return ptr;
}
static void lex_new_expression() {
  // On incrémente le compteur d'expressions (parenthèses ouvertes)
  lex_process->current_expression_count++;

  // Si c'est la toute première parenthèse de la chaîne (ex: le début d'un calcul)
  if (lex_process->current_expression_count == 1) {
    // On initialise le buffer qui stockera le contenu entre parenthèses
    lex_process->parentheses_buffer = buffer_create();
  }
}
static void lex_finish_expression() {
  lex_process->current_expression_count--;

  // Sécurité : Vérifier si l'utilisateur ferme une parenthèse jamais ouverte
  if (lex_process->current_expression_count < 0) {
    compiler_error(lex_process->compiler, "You closed an expression that you never opened");
  }
}
bool lex_is_in_expression() {
  return lex_process->current_expression_count > 0;
}
static struct token* token_make_operator_or_string() {
  char op = peekc();

  // Cas spécial pour #include <file.h>
  if (op == '<') {
    struct token* last_token = lexer_last_token();
    if (token_is_keyword(last_token, "include")) {
      return token_make_string('<', '>');
    }
  }

  struct token* token = token_create(&(struct token) { .type = TOKEN_TYPE_OPERATOR, .sval = read_op() });

  if (op == '(') {
    lex_new_expression();
  }

  return token;
}
static struct token* token_make_symbol() {
  char c = nextc(); // On consomme le caractère du flux

  // Si le symbole est une parenthèse fermante, on décrémente le compteur d'expression
  if (c == ')') {
    lex_finish_expression();
  }

  struct token* token = token_create(&(struct token) {
    .type = TOKEN_TYPE_SYMBOL,
      .cval = c
  });

  return token;
}
static struct token* token_make_identifier_or_keyword() {
  struct buffer* buffer = buffer_create();
  char c = 0;
  LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');


  buffer_write(buffer, 0x00); // Toujours terminer par un caractère nul

  // Plus tard, nous vérifierons ici si le contenu du buffer est un mot-clé (ex: "int")
  // Pour l'instant, tout est un identifiant.
  return token_create(&(struct token) {
    .type = TOKEN_TYPE_IDENTIFIER,
      .sval = buffer_ptr(buffer)
  });
}

static struct token* read_special_token() {
  char c = peekc();

  // Un identifiant doit commencer par une lettre ou un underscore
  if (isalpha(c) || c == '_') {
    return token_make_identifier_or_keyword();
  }

  return NULL;
}

struct token* read_next_token()
{
  struct token* token = NULL;
  char c = peekc();
  switch (c)
  {
  NUMERIC_CASE:
    token = token_make_number();
    break;
    // Dans lexer.c
  OPERATOR_CASE_EXCLUDING_DIVISION:
    token = token_make_operator_or_string();
    break;
  SYMBOL_CASE:
    token = token_make_symbol();
    break;
  case '"':
    token = token_make_string('"', '"');
    break;
    // we don't care about whitespace
  case ' ':
  case '\t':
    token = handle_whitespace();
    break;
  case EOF:
    // we have finished lexical analysis on the file
    break;

  default:
    token = read_special_token();
    if (!token) {
      compiler_error(lex_process->compiler, "Unexpected token\n");
    }

  }
  return token;
}
int lex(struct lex_process* process) {
  process->current_expression_count = 0;
  process->parentheses_buffer = NULL;
  lex_process = process;
  process->pos.filename = process->compiler->cfile.abs_path;

  struct token* token = read_next_token();
  while (token) {
    vector_push(process->token_vec, token);
    token = read_next_token();
  }
  return LEXICAL_ANALYSIS_ALL_OK;
}