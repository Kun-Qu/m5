#ifndef M5_INPUT_H
#define M5_INPUT_H

#include <glib.h>

typedef enum {
        M5_NON_MACRO_TEXT,
        M5_MACRO_TOKEN
} M5ContentType;

typedef struct _M5Content M5Content;
struct _M5Content {
        M5ContentType type;
        gpointer data;
};

GList *m5_input_split (gchar *filename);
GList *m5_input_build_macro_set (GList *contents);

#endif
