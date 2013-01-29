#include <glib.h>
#include <string.h>
#include "m5-input.h"
#include "m5-token.h"

typedef enum {
        M5_NON_MACRO,
        M5_SLOPE,
        M5_ROAD,
        M5_PREPEND_OPERATOR,
        M5_AT,
        M5_BEGIN_MACRO,
        M5_IN_MACRO,
        M5_END_MACRO,
        M5_BEGIN_ESCAPE,
        M5_IN_ESCAPE,
        M5_END_ESCAPE
} M5InputStatus;

static M5InputStatus
m5_parser_status (GString *cache,
                  M5InputStatus current_status,
                  M5InputStatus history_status)
{
        M5InputStatus new_status;

        glong offset = g_utf8_strlen (cache->str, -1) - 1;
        gchar *last_char = g_utf8_offset_to_pointer (cache->str, offset);

        switch (current_status) {
        case M5_NON_MACRO:
                if (g_str_equal (last_char, "\\")) {
                        new_status = M5_SLOPE;
                } else {
                        new_status = M5_NON_MACRO;
                 }
                break;
        case M5_SLOPE:
                if (history_status == M5_IN_MACRO) {
                        new_status = M5_IN_MACRO;
                } else if (g_str_equal (last_char, "_")) {
                        new_status = M5_ROAD;
                } else if (g_str_equal (last_char, "@")) {
                        new_status = M5_AT;
                } else {
                        new_status = M5_NON_MACRO;
                }
                break;
        case M5_ROAD:
                if (g_str_equal (last_char, "{")) {
                        if (history_status == M5_IN_MACRO) {
                                new_status = M5_IN_MACRO;
                        } else {
                                new_status = M5_BEGIN_MACRO;
                        }
                } else if (g_str_equal (last_char, "^")) {
                        new_status = M5_PREPEND_OPERATOR;
                } else {
                        new_status = history_status;
                }
                break;
        case M5_AT:
                if (g_str_equal (last_char, "{")) {
                        new_status = M5_BEGIN_ESCAPE;
                } else if (g_str_equal (last_char, ">")) {
                        new_status = M5_END_MACRO;
                } else {
                        new_status = history_status;
                }
                break;
        case M5_PREPEND_OPERATOR:
                if (g_str_equal (last_char, "{")) {
                        new_status = M5_BEGIN_MACRO;
                } else {
                        new_status = M5_NON_MACRO;
                }
                break;
        case M5_BEGIN_MACRO:
                new_status = M5_IN_MACRO;
                break;
        case M5_IN_MACRO:
                if (g_str_equal (last_char, "\\")) {
                        new_status = M5_SLOPE;
                } else if (g_str_equal (last_char, "@")) {
                        new_status = M5_AT;
                } else {
                        new_status = M5_IN_MACRO;
                }
                break;
        case M5_END_MACRO:
                new_status = M5_NON_MACRO;
                break;
        case M5_BEGIN_ESCAPE:
                new_status = M5_IN_ESCAPE;
                break;
        case M5_IN_ESCAPE:
                if (g_str_equal (last_char, "}")) {
                        gchar *t = g_utf8_offset_to_pointer (cache->str, offset - 2);
                        if (g_str_equal (t, "\\@}")) {
                                new_status = M5_END_ESCAPE;
                        } else {
                                new_status = M5_IN_ESCAPE;
                        }
                } else {
                        new_status = M5_IN_ESCAPE;
                }
                break;
        case M5_END_ESCAPE:
                if (history_status == M5_NON_MACRO) {
                        new_status = M5_NON_MACRO;
                } else if (history_status == M5_IN_MACRO) {
                        new_status = M5_IN_MACRO;
                } else {
                        g_error ("ESCAPE status error!\n");
                }
                break;
        default:
                break;
        }
        
        return new_status;
}

static M5Content *
m5_input_take_non_macro_text (GString *cache, gchar *head)
{
        M5Content *content = NULL;
        glong cache_len = g_utf8_strlen (cache->str, -1);
        glong head_len = g_utf8_strlen (head, -1);
        if (cache_len > head_len) {
                gchar *substr = g_utf8_substring (cache->str, cache_len - head_len, cache_len);
                if (g_str_equal (substr, head)) {
                        gchar *text = g_utf8_substring (cache->str,
                                                        0,
                                                        cache_len - head_len);
                        content = g_slice_new (M5Content);
                        content->type = M5_NON_MACRO_TEXT;
                        content->data = g_string_new (substr);
                        g_free (text);

                        g_string_erase (cache,
                                        0,
                                        strlen (cache->str) - strlen (head));
                }
                g_free (substr);
        }
        
        return content;
}

GList *
m5_input_split (gchar *filename)
{
        GIOChannel *channel = g_io_channel_new_file (filename, "r", NULL);
        GIOStatus status;

        GList *contents = NULL;
        
        GString *cache = g_string_new (NULL);
        gunichar unicode_char;
        M5Content *content = NULL;

        M5InputStatus m, h;
        m = h = M5_NON_MACRO;
        do {
                status = g_io_channel_read_unichar (channel,
                                                    &unicode_char,
                                                    NULL);
                if (status == G_IO_STATUS_NORMAL) {
                        g_string_append_unichar (cache, unicode_char);
                        m = m5_parser_status (cache, m, h);
                        if (m == M5_IN_MACRO || m == M5_NON_MACRO) {
                                h = m;
                        }
                        /* 分离宏文本与非宏文本 */
                        if (m == M5_BEGIN_MACRO) {
                                content = m5_input_take_non_macro_text (cache, "\\_{");
                                if (!content) {
                                        content = m5_input_take_non_macro_text (cache, "\\_^{");
                                }
                                if (content)
                                        contents = g_list_prepend (contents, content);
                        } else if (m == M5_END_MACRO) {/* 分离宏文本 */
                                content = g_slice_new (M5Content);
                                content->type = M5_MACRO_TOKEN;
                                content->data = g_string_new (cache->str);
                                contents = g_list_prepend (contents, content);  
                                g_string_free (cache, TRUE);
                                cache = g_string_new (NULL);
                        }
                }
        } while (status == G_IO_STATUS_NORMAL);

        /* 可能会有非宏文本残余在 cache 中，将它直接转化为非宏文本结点 */
        if (0 < g_utf8_strlen (cache->str, -1)) {
                content = g_slice_new (M5Content);
                content->type = M5_NON_MACRO_TEXT;
                content->data = cache;
                contents = g_list_prepend (contents, content);
        }
        
        g_io_channel_unref (channel);
        
        contents = g_list_reverse (contents);
        
        return contents;
}

static void
build_macro_token (gpointer data, gpointer user_data)
{
        M5Content *content = data;
        gint index[4] = {0};
        M5Token *token;
        GString *text = content->data;

        if (content->type == M5_NON_MACRO_TEXT) {
                g_string_overwrite (text,  0, text->str);
        } else {
                token = m5_token_alloc ();
                GRegex *macro_re = g_regex_new ("(\\Q\\_\\E[\\Q^\\E]?\\{.*\\})"
                                                " *\\+= *[\r\n]*(.*)"
                                                "[\r\n]@>$",
                                                G_REGEX_DOTALL, 0, NULL);
                gchar **splited_text = g_regex_split (macro_re, text->str, 0);
                g_regex_unref (macro_re);
                for (gint i = 0, j = 0; splited_text[i] != NULL; i++) {
                        if (strlen (splited_text[i]) > 0) {
                                index[j++] = i;
                                if (j > 2) {
                                        g_error ("building macro token error!\n");
                                }
                        }
                }
                /* 剥除宏名的外衣 */
                GRegex *macro_name_re = g_regex_new ("\\Q_\\E([\\Q^\\E]*)\\{\\s*(.*)\\s*\\}",
                                                     G_REGEX_DOTALL, 0, NULL);
                gchar **splited_macro_name = g_regex_split (macro_name_re, splited_text[index[0]], 0);
                g_regex_unref (macro_name_re);
                for (gint i = 0; splited_macro_name[i] != NULL; i++) {
                        if (g_str_equal (splited_macro_name[i], "^")) {
                                token->merge_type = M5_MACRO_PREPENDED;
                        } else if (strlen (splited_macro_name[i]) > 0) {
                                token->name = g_string_new (splited_macro_name[i]);
                        }
                }
                g_strfreev (splited_macro_name);
                
                /* 宏的内容 */
                splited_text[index[1]] = g_strchomp (splited_text[index[1]]);
                token->content = g_string_new (splited_text[index[1]]);

                /* 释放用来产生 M5Token 数据的文本 */
                g_string_free (content->data, TRUE);

                /* 将 M5Token 数据插入至列表中 */
                content->data = token;
                
                g_strfreev (splited_text);
        }
}

static GList *
m5_input_merge_same_macro (GList *contents)
{
        GList *macro_set = NULL;

        /* 构造宏集 */
        for (GList *it_i = g_list_first (contents);
             it_i != NULL;
             it_i = g_list_next (it_i)) {
                M5Content *content_i = it_i->data;
                if (content_i->type == M5_MACRO_TOKEN) {
                        gboolean exist = FALSE;
                         for (GList *it_j = g_list_first (macro_set);
                              it_j != NULL;
                              it_j = g_list_next (it_j)) {
                                 M5Token *ti = content_i->data;
                                 M5Token *tj = it_j->data;
                                 if (g_str_equal (ti->name->str, tj->name->str)) {
                                         exist = TRUE;
                                         break;
                                 }
                         }
                         if (!exist) {
                                 macro_set = g_list_append (macro_set, content_i->data);
                         }
                }
        }

        /* 合并 */
        for (GList *it_i = g_list_first (contents);
             it_i != NULL;
             it_i = g_list_next (it_i)) {
                M5Content *content_i = it_i->data;
                if (content_i->type == M5_MACRO_TOKEN) {
                         for (GList *it_j = g_list_first (macro_set);
                              it_j != NULL;
                              it_j = g_list_next (it_j)) {
                                 M5Token *ti = content_i->data;
                                 M5Token *tj = it_j->data;
                                 if (g_str_equal (ti->name->str, tj->name->str)) {
                                         if (ti->content == tj->content)
                                                 continue;
                                         if (ti->merge_type == M5_MACRO_APPENDED) {
                                                 g_string_append (tj->content, "\n");
                                                 g_string_append (tj->content, ti->content->str);
                                         } else {
                                                 g_string_prepend (tj->content, "\n");
                                                 g_string_prepend (tj->content, ti->content->str);
                                         }
                                 }
                         }
                }
        }
        
        return macro_set;
}

static GString *
m5_get_head_spaces (gchar *text)
{
        GString *spaces = g_string_new (NULL);
        gchar *substr = NULL;
        gint i = 0;
        while (1) {
                substr = g_utf8_substring (text, i++, i+1);
                if (g_str_equal (substr, " ")
                    || g_str_equal (substr, "\t")) {
                        g_string_append (spaces, substr);
                        g_free (substr);
                } else {
                        break;
                }
        }
        return spaces;
}

static GString *
m5_get_tail_spaces (gchar *text)
{
        gint i = g_utf8_strlen (text, -1) - 1;
        GString *spaces = g_string_new (NULL);
        gchar *substr;              
        while (1) {
                substr = g_utf8_substring (text, i--, i+1);
                if (g_str_equal (substr, " ")
                    || g_str_equal (substr, "\t")) {
                        g_string_append (spaces, substr);
                        g_free (substr);
                } else {
                        break;
                }
        }
        return spaces;
}

static gboolean
m5_input_is_left_leading_parenthesis (gchar *text)
{
        gboolean r = TRUE;
        gchar *substr = NULL;
        gint i = 0;
        while (1) {
                substr = g_utf8_substring (text, i++, i+1);
                if (g_str_equal (substr, " ")
                    || g_str_equal (substr, "\t")) {
                        continue;
                } else if (g_str_equal (substr, "(")) {
                        break;
                } else {
                        r = FALSE;
                        break;
                }
                g_free (substr);
        }
        
        return r;
}

static GString *
m5_input_search_and_replace (M5Token *t, M5Token *r)
{
        GString *text = t->content;

        gchar *a = r->name->str;
        gchar *b = r->signature->str;
        
        gchar **splited_text = g_strsplit (text->str, a, 0);
        GString *new_text = g_string_new (NULL);

        gint i;
        for (i = 0; splited_text[i] != NULL; i++);
        if (i == 1) {
                return NULL;
        } else if (i > 2) {
                g_error ("one macro can not be used twice in the same macro!\n");
        } else {
                g_string_append (new_text, splited_text[0]);
                g_string_append (new_text, b);

                if (r->padding) {
                        g_error ("One macro should not be used multi-times!\n");
                }
                r->padding = g_slice_new (M5MacroPadding);
                r->padding->left = m5_get_tail_spaces (splited_text[0]);

                /* 有可能会是带参数的宏，m5 允许宏名与左括号之间有空格，
                   而 m4 不允许，所以要删除左括号前的空格 */
                if (m5_input_is_left_leading_parenthesis (splited_text[1])) {
                        splited_text[1] =  g_strchug (splited_text[1]);
                        r->padding->right = g_string_new (NULL);
                } else {
                        r->padding->right = m5_get_head_spaces (splited_text[1]);
                }
                g_string_append (new_text, splited_text[1]);
        }
        g_strfreev (splited_text);
        
        return new_text;
}

static void
generate_signature (gpointer data, gpointer userdata)
{
        M5Token *t = data;
        m5_token_macro_signature (t);
}

GList *
m5_input_build_macro_set (GList *contents)
{
        g_list_foreach (contents, build_macro_token, NULL);
        
        GList *macro_set = m5_input_merge_same_macro (contents);
        g_list_foreach (macro_set, generate_signature, NULL);
        
        GString *new_macro_content = NULL;
        M5Token *t1, *t2;
        /* 宏内容中的宏名替换 */
        for (GList *it_i = g_list_first (macro_set);
             it_i != NULL;
             it_i = g_list_next (it_i)) {
                t1 = it_i->data;
                if (g_str_equal (t1->name->str, "*"))
                        continue;
                for (GList *it_j = g_list_first (macro_set);
                     it_j != NULL;
                     it_j = g_list_next (it_j)) {
                        t2 = it_j->data;
                        new_macro_content = m5_input_search_and_replace (t2, t1);
                        if (new_macro_content) {
                                g_string_free (t2->content, TRUE);
                                t2->content = new_macro_content;
                        }
                }
        }
                
        return macro_set;
}
