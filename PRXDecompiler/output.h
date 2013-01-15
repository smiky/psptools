/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __OUTPUT_H
#define __OUTPUT_H

#include <stddef.h>
#include "code.h"

#define OUT_PRINT_DFS          1
#define OUT_PRINT_RDFS         2
#define OUT_PRINT_DOMINATOR    4
#define OUT_PRINT_RDOMINATOR   8
#define OUT_PRINT_FRONTIER    16
#define OUT_PRINT_RFRONTIER   32
#define OUT_PRINT_CODE        64
#define OUT_PRINT_STRUCTURES 128
#define OUT_PRINT_EDGE_TYPES 256

#define OPTS_NORESULT           1
#define OPTS_REVERSECOND        2
#define OPTS_RESULT             4
#define OPTS_SECONDRESULT       8

extern int g_verbosity;
extern int g_printoptions;


void ident_line (FILE *out, int size);
void get_base_name (char *filename, char *basename, size_t len);
void print_value (FILE *out, struct value *val, int options);
void print_operation (FILE *out, struct operation *op, int identsize, int options);
void print_subroutine_name (FILE *out, struct subroutine *sub);
void print_subroutine_declaration (FILE *out, struct subroutine *sub);

int print_code (struct code *c, char *filename);
int print_graph (struct code *c, char *prxname);

#endif /* __OUTPUT_H */
