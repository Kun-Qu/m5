#include <config.h>
#include <glib/gi18n.h>
#include "m5-token.h"
#include "m5-input.h"

static void
output_m5 (gpointer data, gpointer userdata)
{
        M5Token *token = data;
        g_print ("dnl \\_{%s}\n", token->name->str);
        g_print ("define(%s,\n", token->signature->str);

        if (token->padding) {
                g_print ("@[");
                gchar **s = g_strsplit (token->content->str, "\n", 0);
                for (gint i = 0; s[i] != NULL; i++) {
                        if (i == 0) {
                                g_print ("%s%s\n", s[i], token->padding->right->str);
                        } else if (s[i+1] != NULL) {
                                g_print ("%s%s%s\n",
                                         token->padding->left->str,
                                         s[i],
                                         token->padding->right->str);
                        } else {
                                g_print ("%s%s", token->padding->left->str, s[i]);
                        }
                }
                g_print ("]@)\n\n");
                g_strfreev (s);
        } else {
                g_print ("@[%s]@)\n\n", token->content->str);
        }
}

static void
output_root (gpointer data, gpointer userdata)
{
        M5Token *token = data;
        if (g_str_equal (token->name->str, "*")) {
                 g_print ("%s\n", token->signature->str);
        }
}

int
main (int argc, char **argv)
{
        setlocale(LC_CTYPE, "");

        if (argc != 2) {
                g_error ("you should specify m5 file name!\n");
        }
        
        GList *contents =  m5_input_split (argv[1]);
        GList *macro_set = m5_input_build_macro_set (contents);

        g_print ("divert(-1)\nchangequote(@[,]@)dnl\n\n");
        g_list_foreach (macro_set, output_m5, NULL);
        g_print ("divert(0)dnl\n");

        /* 输出根结点 */
        g_list_foreach (macro_set, output_root, NULL);
        return 0;
}
