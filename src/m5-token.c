#include "m5-token.h"

M5Token *
m5_token_alloc (void)
{
        M5Token *token = g_slice_new (M5Token);

        token->name = NULL;
        token->content = NULL;
        token->merge_type = M5_MACRO_APPENDED;
        
        return token;
}

void
m5_token_free (M5Token *token)
{
        if (token->name)
                g_string_free (token->name, TRUE);
        if (token->content)
                g_string_free (token->content, TRUE);

        g_slice_free (M5Token, token);
}
