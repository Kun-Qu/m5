#ifndef M5_TOKEN_H
#define M5_TOKEN_H

#include <glib.h>

typedef enum {
        M5_MACRO_APPENDED,
        M5_MACRO_PREPENDED
} M5MacroMergeType;

typedef struct _M5MacroPadding M5MacroPadding;
struct _M5MacroPadding {
        GString *left;
        GString *right;
};

typedef struct _M5Token M5Token;
struct _M5Token {
        GString *name;
        GString *content;
        M5MacroMergeType merge_type;
};

M5Token * m5_token_alloc (void);
void m5_token_free (M5Token *token);

void m5_token_macro_signature (M5Token *token);

#endif
