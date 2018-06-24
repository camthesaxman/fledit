#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))

class Fl_Text_Buffer;
class Fl_Text_Editor;

/* history.cpp */

struct HistoryCommand;

struct History
{
    struct HistoryCommand *undoCmd;
    struct HistoryCommand *redoCmd;
    Fl_Text_Buffer *textbuf;
};

void history_record_text_insert(struct History *h, unsigned int pos, unsigned int nInserted);
void history_record_text_delete(struct History *h, unsigned int pos, unsigned int nDeleted);
void history_undo(struct History *h);
void history_redo(struct History *h);
void history_free(struct History *h);

/* settings.cpp */

struct Settings
{
    bool lineNumbers;
    unsigned int fontFace;
    unsigned int fontSize;
    unsigned int theme;
    bool syntaxHighlighting;
    bool markDoubleClickedWord;
};

extern struct Settings g_settings;

void settings_load(void);
void settings_save(void);

/* font_dialog.cpp */

void font_dialog_init(void (*applyCallback)(void));
void font_dialog_open(void);

/* find_dialog.cpp */

void find_dialog_init(void);
void find_dialog_show(Fl_Text_Buffer *textBuf);

/* colorize.cpp */

Fl_Text_Buffer *colorize_init(Fl_Text_Buffer *textbuf);
void colorize_update(Fl_Text_Editor *editor, Fl_Text_Buffer *textbuf, Fl_Text_Buffer *stylebuf);
void colorize_clear(Fl_Text_Editor *editor, Fl_Text_Buffer *stylebuf);
void colorize_update_font(Fl_Font font, int size);
