#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))

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

/* settings.cpp */

struct Settings
{
    bool lineNumbers;
    unsigned int fontFace;
    unsigned int fontSize;
    unsigned int theme;
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
