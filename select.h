#ifndef XS_SELECT_H
#define XS_SELECT_H

#include "parser.h"

/*
 * Pragmatic CSS selector engine (FEAT-004).
 * Supports: type (p), universal (*), class (.c), id (#i), compound
 * (p.c#i), grouping (a, b), and descendant combinator (nav a).
 * Specificity is computed per W3C (ids, classes, types).
 */

#define SEL_MAX_SIMPLE   8   /* simple selectors per compound */
#define SEL_MAX_COMPOUND 6   /* compounds in a descendant chain */

typedef enum { SIMPLE_TYPE, SIMPLE_UNIVERSAL, SIMPLE_CLASS, SIMPLE_ID } SimpleKind;

typedef struct {
    SimpleKind kind;
    char       name[64];   /* tag/class/id name (lowercased) */
} SimpleSel;

typedef struct {
    SimpleSel simples[SEL_MAX_SIMPLE];
    int       n_simples;
} CompoundSel;

/* A complex selector is a chain of compounds joined by descendant combinators.
   compounds[n-1] is the subject (rightmost); earlier ones are ancestors. */
typedef struct {
    CompoundSel compounds[SEL_MAX_COMPOUND];
    int         n_compounds;
    int         specificity;   /* packed: ids*10000 + classes*100 + types */
    int         valid;
} ComplexSel;

/* A parsed selector list (comma-separated). */
typedef struct {
    ComplexSel* items;
    int         count;
} SelectorList;

/* Parse a selector string (may contain commas). Caller frees with
   selector_list_free. Never returns NULL for valid input; invalid individual
   selectors are marked !valid and skipped during matching. */
SelectorList* selector_list_parse(const char* text);
void          selector_list_free(SelectorList* list);

/* Does `node` match the complex selector? */
int  selector_matches(const ComplexSel* sel, const DOMNode* node);

/* Compute specificity for a single complex selector (also stored in .specificity). */
int  selector_specificity(const ComplexSel* sel);

#endif /* XS_SELECT_H */
