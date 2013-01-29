#include "m5-token.h"

M5Token *
m5_token_alloc (void)
{
        M5Token *token = g_slice_new (M5Token);

        token->name = NULL;
        token->content = NULL;
        token->signature = g_string_new ("M5_");
        token->merge_type = M5_MACRO_APPENDED;
        token->padding = NULL;
        
        return token;
}

void
m5_token_free (M5Token *token)
{
        if (token->name)
                g_string_free (token->name, TRUE);
        if (token->signature)
                g_string_free (token->signature, TRUE);
        if (token->content)
                g_string_free (token->content, TRUE);
        if (token->padding) {
                g_string_free (token->padding->left, TRUE);
                g_string_free (token->padding->right, TRUE);
                g_slice_free (M5MacroPadding, token->padding);
        }

        g_slice_free (M5Token, token);
}

void
m5_token_macro_signature (M5Token *token)
{
        GString *all = g_string_new (token->name->str);
        all = g_string_append (all, token->content->str);
        
        gchar *sig = g_compute_checksum_for_string (G_CHECKSUM_SHA256,
                                                    all->str,
                                                    -1);
        g_string_append (token->signature, sig);
}
