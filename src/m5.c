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

        if (argc != 2) {
                g_error ("you should specify m5 file name!\n");
        }
        
        GList *contents =  m5_input_split (argv[1]);
        GList *macro_set = m5_input_build_macro_set (contents);

        g_print ("divert(-1)\n");
        g_print ("changecom(`尼玛，我是灰常奇葩的注释符！')\n"
                 "define(`m5_index_nl', `index(`$1', `\n"
                 "')')\n"
                 "\n"
                 "define(`m5_first_line',\n"
                 "       `pushdef(`pos', `m5_index_nl(`$2')')'`$1'`substr(`$2', 0, pos)'`$3\n"
                 "popdef(`pos')')\n"
                 "\n"
                 "define(`m5_tail_text', `substr(`$1', eval(1 + m5_index_nl(`$1')), len(`$1'))')\n"
                 "\n"
                 "define(`m5_add_padding',\n"
                 "`m5_first_line(`$1', `$2', `$3')dnl\n"
                 "pushdef(`tail_text', `m5_tail_text(`$2')')dnl\n"
                 "ifelse(m5_index_nl(tail_text),\n"
                 "       -1,\n"
                 "       `$1'tail_text`$3',\n"
                 "       `m5_add_padding(`$1', tail_text, `$3')')`'popdef(`tail_text')'dnl\n"
                 ")\n");
        g_list_foreach (macro_set, output_m5, NULL);
        g_print ("divert(0)dnl\n");

        /* 输出根结点 */
        g_list_foreach (macro_set, output_root, NULL);
        return 0;
}
