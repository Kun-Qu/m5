#include <config.h>
#include <glib/gi18n.h>
#include "m5-token.h"
#include "m5-input.h"

static void
output_m5 (gpointer data, gpointer userdata)
{
        M5Token *token = data;
        g_print ("define(`%s',\n", token->name->str);
        g_print ("`%s')\n\n", token->content->str);
}

static void
output_root (gpointer data, gpointer userdata)
{
        M5Token *token = data;
        if (g_str_equal (token->name->str, "*")) {
                g_print ("indir(`*')\n");
        }
}

int
main (int argc, char **argv)
{
        setlocale(LC_CTYPE, "");

        GList *contents = NULL;
        if (argc == 1) {
                contents =  m5_input_split (NULL);
        } else {
                contents =  m5_input_split (argv[1]);
        } 
        GList *macro_set = m5_input_build_macro_set (contents);

        g_print ("divert(-1)\n");
        g_print ("changecom(`尼玛，我是灰常奇葩的注释符！')\n\n");
        g_list_foreach (macro_set, output_m5, NULL);
        g_print ("divert(0)dnl\n");

        /* 输出根结点 */
        g_list_foreach (macro_set, output_root, NULL);
        return 0;
}
