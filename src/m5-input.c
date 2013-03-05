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

gchar *
g_utf8_substring (const gchar *str,
                  glong        start_pos,
                  glong        end_pos)
{
  gchar *start, *end, *out;

  start = g_utf8_offset_to_pointer (str, start_pos);
  end = g_utf8_offset_to_pointer (start, end_pos - start_pos);

  out = g_malloc (end - start + 1);
  memcpy (out, start, end - start);
  out[end - start] = 0;

  return out;
}


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
                        gchar *t = g_utf8_offset_to_pointer (cache->str, offset - 1);
                        if (g_str_equal (t, "@}")) {
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
        GIOChannel *channel = NULL;
        if (filename)
                channel = g_io_channel_new_file (filename, "r", NULL);
        else 
                channel = g_io_channel_unix_new (0);
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
                        } else if (m == M5_BEGIN_ESCAPE || m == M5_END_ESCAPE) {/* 遭遇逃逸字串 */
                                /* 删除 cache 末尾的 @{ 或 @}字符 */
                                glong length = g_utf8_strlen (cache->str, -1);
                                gchar *escape_chars = g_utf8_substring (cache->str, length - 2, length);
                                gsize offset = strlen (escape_chars);
                                g_string_erase (cache, cache->len - offset, offset);
                                g_free (escape_chars);
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
                                                "[\r\n]*@>$",
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

static void
m5_string_chug (GString *text)
{
        gchar *substr = NULL;
        while (1) {
                substr = g_utf8_substring (text->str, 0, 1);
                if (g_str_equal (substr, " ")
                    || g_str_equal (substr, "\t")) {
                        g_string_erase (text, 0, strlen (substr));
                        g_free (substr);
                } else {
                        break;
                }
        }
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

static void
m5_string_chomp (GString *text)
{
        GString *spaces = g_string_new (NULL);
        gchar *substr = NULL;
        gint i = 0;
        while (1) {
                i = g_utf8_strlen (text->str, -1) - 1;
                substr = g_utf8_substring (text->str, i, i+1);
                if (g_str_equal (substr, " ")
                    || g_str_equal (substr, "\t")) {
                        g_string_erase (text, text->len - 1, strlen (substr));
                        g_free (substr);
                } else {
                        break;
                }
        }
}

static gboolean
m5_is_left_leading_parenthesis (gchar *text)
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
m5_take_arg_list (GString *text) 
{
        GString *arg_list = g_string_new (NULL);
        gchar *substr = NULL;
        gint i = 0;
        gint arg_list_end = 0;
        
        /* 计算去除 text 的前导空格与制表符的 text 长度，
         之所以要去除前导空白字符，是为了保证 text 的第一个字符是 "("*/
        m5_string_chug (text);
        glong len = g_utf8_strlen (text->str, -1);
        
        while (1) {
                substr = g_utf8_substring (text->str, i++, i+1);
                g_string_append (arg_list, substr);
                if (g_str_equal (substr, "(")) {
                        arg_list_end ++;
                } else if (g_str_equal (substr, ")")) {
                        arg_list_end --;
                }
                g_free (substr);
                if (arg_list_end == 0) {
                        break;
                }
                if (i == len) {
                        g_error ("There is no terminator of macro argument list!\n");
                }
        }

        return arg_list;
}

static GString *
m5_input_fix_macro_definition (M5Token *t, M5Token *r)
{       
        GString *new_definition = g_string_new (NULL);
        gchar **splited_text = g_strsplit (t->content->str, r->name->str, 0);
        GList *text_list = NULL;
        
        for (gint i = 0; splited_text[i] != NULL; i++) {
                text_list = g_list_prepend (text_list, g_string_new (splited_text[i]));
        }
        g_strfreev (splited_text);
        
        if (g_list_length (text_list) <= 1) {
                return NULL;
        }

        /* 向 text_list 结点之间插入 r 中的宏名，得到 new_text_list */
        GList *new_text_list = NULL;
        GList *last_node = g_list_last (text_list);
        for (GList *it = g_list_first (text_list);
             it != NULL;
             it = g_list_next (it)) {
                new_text_list = g_list_prepend (new_text_list, it->data);
                if (it != last_node) {
                        new_text_list = g_list_prepend (new_text_list,
                                                        g_string_new (r->name->str));
                }
        }
        g_list_free (text_list);

        /* 产生 padding 与 indir 宏封装*/
        GString *text = NULL;
        M5MacroPadding *padding = NULL;
        
        for (GList *it = g_list_first (new_text_list);
             it != NULL;
             it = g_list_next (g_list_next (it))) {
                if (it != g_list_first (new_text_list)) {
                        /* 左 padding */
                        text = (g_list_previous(g_list_previous(it)))->data;
                        padding = g_slice_new (M5MacroPadding);
                        padding->left = m5_get_tail_spaces (text->str);
                        
                        /* 右 padding */
                        text = it->data;
                        GString *macro_name = (g_list_previous (it))->data;
                        GString *m5_add_padding = g_string_new (NULL);
                        if (m5_is_left_leading_parenthesis (text->str)) {
                                /* 处理带参数的宏 */
                                 GString *arg_list = m5_take_arg_list (text);
                                if (arg_list->len == 0)
                                        g_error ("Parsing argument list of macro fails!\n");

                                /* 将 text 的中包含的参数列表子字串删除 */
                                g_string_erase (text, 0, arg_list->len);
                                
                                padding->right = m5_get_head_spaces (text->str);
                                
                                /* 迎合 indir 的语法，将参数列表的括号删除 */
                                g_string_erase (arg_list, 0, strlen ("("));
                                g_string_erase (arg_list,
                                                strlen (arg_list->str) - 1,
                                                strlen (")"));
                                
                                /* 产生 indir */
                                if (g_utf8_strlen (padding->left->str, -1) == 0
                                    && g_utf8_strlen (padding->right->str, -1) == 0) {
                                        g_string_printf (m5_add_padding,
                                                         "indir(`%s',%s)",
                                                         macro_name->str,
                                                         arg_list->str);
                                } else {
                                        g_string_printf (m5_add_padding,
                                                         "patsubst(indir(`%s', %s),`\n',`%s\n%s')",
                                                         macro_name->str,
                                                         arg_list->str,
                                                         padding->right->str,
                                                         padding->left->str);
                                }
                        } else {
                                padding->right = m5_get_head_spaces (text->str);

                                /* 产生 indir */
                                if (g_utf8_strlen (padding->left->str, -1) == 0
                                    && g_utf8_strlen (padding->right->str, -1) == 0) {
                                        g_string_printf (m5_add_padding,
                                                         "indir(`%s')", macro_name->str);
                                } else {
                                        g_string_printf (m5_add_padding,
                                                         "patsubst(indir(`%s'),`\n',`%s\n%s')",
                                                         macro_name->str,
                                                         padding->right->str,
                                                         padding->left->str);
                                }
                        }
                        (g_list_previous (it))->data = m5_add_padding;
                        g_string_free (macro_name, TRUE);
                        g_slice_free (M5MacroPadding, padding);
                }
        }

        /* 输出调整后的宏定义 */
        for (GList *it = g_list_first (new_text_list);
             it != NULL;
             it = g_list_next (it)) {
                text = it->data;
                g_string_append (new_definition, text->str);
                g_string_free (text, TRUE);
        }

        g_list_free (new_text_list);
        
        return new_definition;
}

GList *
m5_input_build_macro_set (GList *contents)
{
        GList *macro_set = NULL;
        g_list_foreach (contents, build_macro_token, NULL);
        macro_set = m5_input_merge_same_macro (contents);
        
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
                        new_macro_content = m5_input_fix_macro_definition (t2, t1);
                        if (new_macro_content) {
                                g_string_free (t2->content, TRUE);
                                t2->content = new_macro_content;
                        }
                }
        }
                
        return macro_set;
}
