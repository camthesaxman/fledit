#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>

#include "fledit.hpp"

#define APP_VERSION "0.1"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TOOLBAR_HEIGHT 32

enum
{
    FILE_ACTION_ERROR = -1,
    FILE_ACTION_OK = 0,
    FILE_ACTION_CANCELED = 1,
};

struct TextFile
{
    struct TextFile *next;
    bool modified;
    char filename[FL_PATH_MAX];
    char title[FL_PATH_MAX];
    Fl_Text_Buffer *textbuf;
    Fl_Text_Buffer *stylebuf;
    Fl_Group *tab;
    struct History history;
};

static void set_current_tab(struct TextFile *f);

static Fl_Window *s_mainWindow;
static Fl_Menu_Bar *s_menuBar;
static Fl_Text_Editor *s_textEditor;
static Fl_Tabs *s_tabBar;
static struct TextFile *s_textFiles = NULL;
static struct TextFile *s_currTextFile = NULL;
static const char *const s_themeNames[] = {"none", "plastic", "gtk+", "gleam"};
static bool s_updateHistoryOnModify = true;

static char *get_base_filename(char *filename)
{
    char *slash = strrchr(filename, '/');

    if (slash != NULL)
        return slash + 1;
    else
        return filename;
}

static void update_file_title(struct TextFile *f)
{
    if (f->filename[0] == 0)
        strcpy(f->title, "(untitled)");
    else
        strcpy(f->title, get_base_filename(f->filename));
    if (f->modified)
        f->title[MIN(strlen(f->title), sizeof(f->title) - 1)] = '*';
}

static void cb_tab_change(Fl_Widget *, void *)
{
    Fl_Group *tab = (Fl_Group *)s_tabBar->value();
    struct TextFile *f = (struct TextFile *)tab->user_data();

    set_current_tab(f);
}

static bool s_ignoreRecursion = false;

static void cb_modified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char *deletedText, void *p)
{
    /*
    static bool ignoreRecursion;

    if (ignoreRecursion)
    {
        assert(nDeleted == 0 && nInserted == 0);
        return;
    }

    if (g_settings.markDoubleClickedWord && Fl::event_is_click() && Fl::event_clicks())
    {
        ignoreRecursion = true;

        char *text = s_currTextFile->textbuf->selection_text();
        int pos = 0;
        int foundPos;
        int length = strlen(text);

        printf("highlighting '%s'\n", text);
        while (1)
        {
            printf("searching at pos %i\n", pos);
            if (!s_currTextFile->textbuf->search_forward(pos, text, &foundPos, false))
                break;

            assert(foundPos >= pos);
            s_currTextFile->textbuf->highlight(foundPos, foundPos + length);
            pos = foundPos + length;
            
            printf("foundPos = %i, pos = %i\n", foundPos, pos);
        }
        
        free(text);
        
        ignoreRecursion = false;
    }
    */

    if (nInserted == 0 && nDeleted == 0)
        return;

    if (g_settings.syntaxHighlighting)
        colorize_update(s_textEditor, s_currTextFile->textbuf, s_currTextFile->stylebuf);

    if (!s_updateHistoryOnModify)
        return;

    struct TextFile *f = (struct TextFile *)p;

    //printf("modified: pos=%i, nInserted=%i, nDeleted=%i, nRestyled=%i\n",
    //    pos, nInserted, nDeleted, nRestyled);

    if (s_updateHistoryOnModify)
    {
        if (nInserted != 0)
        {
            assert(nDeleted == 0);
            history_record_text_insert(&s_currTextFile->history, pos, nInserted);
        }
    }

    if (!f->modified)
    {
        f->modified = true;
        update_file_title(f);
        s_mainWindow->label(f->title);
        f->tab->label(f->title);
        s_tabBar->redraw();
    }
}

static void cb_predelete(int pos, int nDeleted, void *data)
{
    if (s_updateHistoryOnModify)
    {
        if (nDeleted != 0)
        {
            printf("deleting: pos=%i, nDeleted=%i\n", pos, nDeleted);
            history_record_text_delete(&s_currTextFile->history, pos, nDeleted);
        }
    }
}

static void file_list_append(struct TextFile *f)
{
    if (s_textFiles == NULL)
    {
        s_textFiles = f;
    }
    else
    {
        struct TextFile *prev = s_textFiles;

        while (prev->next != NULL)
            prev = prev->next;
        prev->next = f;
    }
}

static void file_list_remove(struct TextFile *f)
{
    if (f == s_textFiles)
    {
        s_textFiles = s_textFiles->next;
    }
    else
    {
        struct TextFile *prev = s_textFiles;

        while (prev->next != f)
        {
            assert(prev->next != NULL);
            prev = prev->next;
        }
        prev->next = f->next;
    }

    // Remove the buffer from the editor before deleting it, so FLTK won't try to
    // use it after it's been freed.
    if (s_textEditor->buffer() == f->textbuf)
        s_textEditor->buffer(NULL);
    delete f->textbuf;
    delete f->stylebuf;
    Fl::delete_widget(f->tab);
    history_free(&f->history);
    delete f;
}

static struct TextFile *open_text_file(const char *filename)
{
    struct TextFile *f = new TextFile;

    memset(f, 0, sizeof(*f));
    f->textbuf = f->history.textbuf = new Fl_Text_Buffer;
    strcpy(f->title, "");

    if (filename != NULL)
    {
        if (f->textbuf->loadfile(filename) == 0)  // succeeded
            strcpy(f->filename, filename);
        else
            fl_alert("Could not open file");
    }
    update_file_title(f);

    f->stylebuf = colorize_init(f->textbuf);

    f->textbuf->add_modify_callback(cb_modified, f);
    f->textbuf->add_predelete_callback(cb_predelete, f);

    // add tab
    f->tab = new Fl_Group(0, 40+TOOLBAR_HEIGHT, 600, 360-TOOLBAR_HEIGHT, f->title);
    f->tab->user_data(f);
    s_tabBar->add_resizable(*f->tab);
    s_mainWindow->redraw();  // Force everything to redraw, because it doesn't happen automatically.

    file_list_append(f);
    return f;
}

static bool save_text_file(struct TextFile *f, const char *filename)
{
    printf("save_text_file: filename='%s'\n", filename);
    if (f->textbuf->savefile(filename) != 0)
    {
        fl_alert("Failed to save file: %s", strerror(errno));
        return false;
    }
    f->modified = false;
    if (f->filename != filename)
        strcpy(f->filename, filename);
    update_file_title(f);
    return true;
}

// Callbacks

static void menu_cb_new(Fl_Widget *, void *)
{
    struct TextFile *f = open_text_file(NULL);
    set_current_tab(f);
}

static void menu_cb_open(Fl_Widget *, void *)
{
    Fl_Native_File_Chooser chooser;
    struct TextFile *f;

    chooser.title("Open");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    switch (chooser.show())
    {
    case FILE_ACTION_OK:
        f = open_text_file(chooser.filename());
        set_current_tab(f);
        break;
    case FILE_ACTION_ERROR:
        fl_alert("Failed to open file: %s", chooser.errmsg());
        break;
    }
}

static int do_save_as(struct TextFile *f)
{
    Fl_Native_File_Chooser chooser;
    const char *filename;
    int result;

    chooser.title("Save As");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    result = chooser.show();
    switch (result)
    {
    case FILE_ACTION_OK:
        filename = chooser.filename();
        printf("saving file to '%s'\n", filename);
        if (save_text_file(f, filename))
            result = FILE_ACTION_OK;
        else
            result = FILE_ACTION_ERROR;
        break;
    case FILE_ACTION_CANCELED:
        break;
    case FILE_ACTION_ERROR:
        fl_alert("Failed to save file: %s", chooser.errmsg());
        break;
    }
    return result;
}

static int do_save(struct TextFile *f)
{
    if (s_currTextFile->filename[0] == 0)
    {
        return do_save_as(f);
    }
    else
    {
        printf("saving file to '%s'\n", f->filename);
        if (save_text_file(f, f->filename))
            return FILE_ACTION_OK;
        else
            return FILE_ACTION_ERROR;
    }
}

static int do_close(struct TextFile *f)
{
    int result;

    if (s_currTextFile->modified)
    {
        int button = fl_choice(
            "The file '%s' is not saved.\n"
            "Do you want to save it before closing?",
            "Cancel", "Save", "Don't save",
            s_currTextFile->title);
        switch (button)
        {
        case 0:  // Cancel
            return FILE_ACTION_CANCELED;
        case 1:  // Save
            result = do_save(s_currTextFile);
            if (result != FILE_ACTION_OK)
                return result;
            break;
        case 2:  // Don't save
            break;
        }
    }

    // remove the file from the editor
    s_tabBar->remove(s_currTextFile->tab);
    file_list_remove(s_currTextFile);
    return FILE_ACTION_OK;
}

static void menu_cb_save_as(Fl_Widget *, void *)
{
    if (do_save_as(s_currTextFile) == FILE_ACTION_OK)
    {
        s_mainWindow->label(s_currTextFile->title);
        s_currTextFile->tab->label(s_currTextFile->title);
        s_tabBar->redraw();
    }
}

static void menu_cb_save(Fl_Widget *, void *)
{
    if (do_save(s_currTextFile) == FILE_ACTION_OK)
    {
        s_mainWindow->label(s_currTextFile->title);
        s_currTextFile->tab->label(s_currTextFile->title);
        s_tabBar->redraw();
    }
}

static void menu_cb_close(Fl_Widget *, void *)
{
    if (do_close(s_currTextFile) == FILE_ACTION_OK)
    {
        if (s_textFiles == NULL)
            s_mainWindow->hide();
        else
        {
            set_current_tab(s_textFiles);
            s_mainWindow->redraw();
        }
    }
}

static void dump_files(void)
{
    struct TextFile *f = s_textFiles;

    puts("files:");
    while (f != NULL)
    {
        printf("%p {next=%p, title=%s}\n", f, f->next, f->title);
        f = f->next;
    }
}

static void menu_cb_exit(Fl_Widget *, void *)
{
    struct TextFile *f = s_textFiles;

    dump_files();
    while (f != NULL)
    {
        struct TextFile *next = f->next;

        set_current_tab(f);
        s_mainWindow->redraw();
        do_close(f);
        printf("maybe closed file %p\n", f);
        dump_files();
        f = next;
    }
    //s_mainWindow->hide();
    if (s_textFiles == NULL)
        s_mainWindow->hide();
}

static void menu_cb_undo(Fl_Widget *, void *)
{
    // Undoing and redoing modifies the textbuf. The callbacks must ignore this
    // and not try to record it again in the command history.
    s_updateHistoryOnModify = false;
    history_undo(&s_currTextFile->history);
    s_updateHistoryOnModify = true;
}

static void menu_cb_redo(Fl_Widget *, void *)
{
    // Undoing and redoing modifies the textbuf. The callbacks must ignore this
    // and not try to record it again in the command history.
    s_updateHistoryOnModify = false;
    history_redo(&s_currTextFile->history);
    s_updateHistoryOnModify = true;
}

static void menu_cb_cut(Fl_Widget *, void *)

static void menu_cb_copy(Fl_Widget *, void *)

static void menu_cb_paste(Fl_Widget *, void *)

static void menu_cb_find(Fl_Widget *, void *)
{
    find_dialog_show(s_currTextFile->textbuf);
}

static void menu_cb_line_numbers(Fl_Widget *, void *data)
{
    g_settings.lineNumbers = !g_settings.lineNumbers;
    if (g_settings.lineNumbers)
    {
        s_textEditor->linenumber_width(50);
        s_textEditor->linenumber_font(g_settings.fontFace);
        s_textEditor->linenumber_size(g_settings.fontSize);
    }
    else
    {
        puts("line numbers disabled");
        s_textEditor->linenumber_width(0);
    }
    s_textEditor->redraw();
}

static void menu_cb_font(Fl_Widget *, void *)
{
    font_dialog_open();
}

static void menu_cb_syntax_highlighting(Fl_Widget *, void *)
{
    g_settings.syntaxHighlighting = !g_settings.syntaxHighlighting;
    if (g_settings.syntaxHighlighting)
        colorize_update(s_textEditor, s_currTextFile->textbuf, s_currTextFile->stylebuf);
    else
        colorize_clear(s_textEditor, s_currTextFile->stylebuf);
}

static void menu_cb_gui_theme(Fl_Widget *, void *p)
{
    unsigned int theme = (uintptr_t)p;
    g_settings.theme = theme;
    Fl::scheme(s_themeNames[theme]);
}

static void menu_cb_mark_occurrences(Fl_Widget *, void *)
{
    g_settings.markDoubleClickedWord = !g_settings.markDoubleClickedWord;
}

static void menu_cb_about(Fl_Widget *, void *)
{
    fl_message("FLedit " APP_VERSION "\nCopyright (c) 2018 Cameron Hall");
}

static void menu_cb_debug(Fl_Widget *, void *)
{
    dump_files();
}

static Fl_Menu_Item s_menuItems[] =
{
    {"&File", 0, NULL, NULL, FL_SUBMENU},
        {"&New",    FL_COMMAND + 'n', menu_cb_new},
        {"&Open",   FL_COMMAND + 'o', menu_cb_open},
        {"&Save",   FL_COMMAND + 's', menu_cb_save},
        {"Save As", 0,                menu_cb_save_as},
        {"Close",   FL_COMMAND + 'w', menu_cb_close, NULL, FL_MENU_DIVIDER},
        {"Exit",  0,                menu_cb_exit},
        {0},
    {"&Edit", 0, NULL, NULL, FL_SUBMENU},
        {"Undo",  FL_COMMAND + 'z', menu_cb_undo},
        {"Redo",  FL_COMMAND + 'y', menu_cb_redo, NULL, FL_MENU_DIVIDER},
        {"Cut",   FL_COMMAND + 'x', menu_cb_cut},
        {"Copy",  FL_COMMAND + 'c', menu_cb_copy},
        {"Paste", FL_COMMAND + 'v', menu_cb_paste, NULL, FL_MENU_DIVIDER},
        {"&Find", FL_COMMAND + 'f', menu_cb_find},
        {0},
    {"&View", 0, NULL, NULL, FL_SUBMENU},
        {"Line Numbers",        0, menu_cb_line_numbers, &s_menuItems[12], FL_MENU_TOGGLE},
        {"Font...",             0, menu_cb_font},
        {"Syntax Highlighting", 0, menu_cb_syntax_highlighting, NULL, FL_MENU_TOGGLE},
        {"GUI Theme",    0, NULL, NULL, FL_SUBMENU},
            {"None",    0, menu_cb_gui_theme, (void *)0, FL_MENU_RADIO},
            {"Plastic", 0, menu_cb_gui_theme, (void *)1, FL_MENU_RADIO},
            {"GTK+",    0, menu_cb_gui_theme, (void *)2, FL_MENU_RADIO},
            {"Gleam",   0, menu_cb_gui_theme, (void *)3, FL_MENU_RADIO},
            {0},
        {0},
    {"&Tools", 0, NULL, NULL, FL_SUBMENU},
        {"Mark occurrences of double clicked word", 0, menu_cb_mark_occurrences, NULL, FL_MENU_TOGGLE},
        {0},
    {"&Help", 0, NULL, NULL, FL_SUBMENU},
        {"About", 0, menu_cb_about},
        {"Debug", FL_COMMAND + 'd', menu_cb_debug},
        {0},
    {0},
};

struct ToolbarButton
{
    const char *label;
    Fl_Color color;
    const char *tooltip;
    Fl_Callback *callback;
};

static const struct ToolbarButton s_toolbarBtns[] =
{
    {"@filenew", FL_WHITE, "New", menu_cb_new},
    {"@fileopen", FL_YELLOW, "Open", menu_cb_open},
    {"@filesave", FL_BLUE, "Save", menu_cb_save},
    {"@search", FL_BLACK, "Find", menu_cb_find},
    {0},
};

static Fl_Pack *create_toolbar(const struct ToolbarButton *btns)
{
    Fl_Box *toolbar = new Fl_Box(0, 20, 600, 32);
    {
        const struct ToolbarButton *b;
        int x = 0;

        for (b = btns; b->label != NULL; b++)
        {
            Fl_Button *button = new Fl_Button(x, 20, 32, 32, b->label);
            button->box(FL_NO_BOX);
            button->tooltip(b->tooltip);
            button->callback(b->callback);
            button->labelcolor(b->color);
            x += 32;
        }
    }
    //toolbar->end();

    toolbar->box(FL_UP_BOX);
    return (Fl_Pack *)toolbar;
}

static void cb_on_font_apply(void)
{
    s_textEditor->textfont(g_settings.fontFace);
    s_textEditor->textsize(g_settings.fontSize);
    s_textEditor->linenumber_font(g_settings.fontFace);
    s_textEditor->linenumber_size(g_settings.fontSize);
    colorize_update_font(g_settings.fontFace, g_settings.fontSize);
    s_textEditor->redraw();
}

static Fl_Window *create_main_window(void)
{
    Fl_Window *w = new Fl_Double_Window(600, 400, "FLedit");
    {
        s_menuBar = new Fl_Menu_Bar(0, 0, 600, 20);
        s_menuBar->menu(s_menuItems);

        create_toolbar(s_toolbarBtns);

        s_tabBar = new Fl_Tabs(0, 20+TOOLBAR_HEIGHT, 600, 380-TOOLBAR_HEIGHT);
        s_tabBar->end();
        s_tabBar->callback(cb_tab_change);

        s_textEditor = new Fl_Text_Editor(0+5, 40+5+TOOLBAR_HEIGHT, 600-10, 360-10-TOOLBAR_HEIGHT);
        s_textEditor->textfont(g_settings.fontFace);
        s_textEditor->textsize(g_settings.fontSize);
        s_textEditor->linenumber_font(g_settings.fontFace);
        s_textEditor->linenumber_size(g_settings.fontSize);
        colorize_update_font(g_settings.fontFace, g_settings.fontSize);

        s_textEditor->remove_key_binding('z', FL_COMMAND);

        find_dialog_init();
        font_dialog_init(cb_on_font_apply);
    }
    w->end();

    w->callback(menu_cb_exit);
    w->resizable(s_textEditor);
    return w;
}

static void set_current_tab(struct TextFile *f)
{
    s_currTextFile = f;
    s_textEditor->buffer(f->textbuf);
    s_mainWindow->label(f->title);
    s_tabBar->value(f->tab);
    if (g_settings.syntaxHighlighting)
        colorize_update(s_textEditor, s_currTextFile->textbuf, s_currTextFile->stylebuf);
}

static void apply_initial_settings(void)
{
    Fl_Menu_Item *item;

    // Line Numbers
    if (g_settings.lineNumbers)
    {
        item = &s_menuItems[17];
        assert(strcmp(item->text, "Line Numbers") == 0);
        item->set();
        s_textEditor->linenumber_width(50);
    }

    // Syntax Highlighting
    if (g_settings.syntaxHighlighting)
    {
        item = &s_menuItems[19];
        assert(strcmp(item->text, "Syntax Highlighting") == 0);
        item->set();
    }

    // Theme
    if (g_settings.theme >= ARRAY_LENGTH(s_themeNames))
        g_settings.theme = 0;
    item = &s_menuItems[20];
    assert(strcmp(item->text, "GUI Theme") == 0);
    item[1 + g_settings.theme].set();
    Fl::scheme(s_themeNames[g_settings.theme]);
    
    // Mark occurrences of double clicked word
    if (g_settings.markDoubleClickedWord)
    {
        item = &s_menuItems[28];
        assert(strcmp(item->text, "Mark occurrences of double clicked word") == 0);
        item->set();
    }
}

int main(int argc, char **argv)
{
    struct TextFile *initFile = NULL;
    int exitCode;
    int i;

    settings_load();

    s_mainWindow = create_main_window();
    s_mainWindow->show();

    for (i = 1; i < argc; i++)
        initFile = open_text_file(argv[i]);
    if (initFile == NULL)
        initFile = open_text_file(NULL);
    set_current_tab(initFile);

    apply_initial_settings();

    exitCode = Fl::run();
    settings_save();
    puts("exited");
    return exitCode;
}
