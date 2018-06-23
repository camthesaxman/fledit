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

struct TextFile
{
    bool modified;
    char filename[FL_PATH_MAX];
    char title[FL_PATH_MAX];
    Fl_Text_Buffer *textbuf;
    Fl_Group *tab;
    struct TextFile *next;
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
static bool s_ignoreModifyCallbacks;

static void history_dump(void)
{
}

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

    //printf("selected tab '%s'\n", f->title);
    set_current_tab(f);
}

static void cb_modified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char *deletedText, void *p)
{
    if (s_ignoreModifyCallbacks)
        return;

    struct TextFile *f = (struct TextFile *)p;

    if (nInserted == 0 && nDeleted == 0)
        return;

    //printf("modified: pos=%i, nInserted=%i, nDeleted=%i, nRestyled=%i\n",
    //    pos, nInserted, nDeleted, nRestyled);

    if (nInserted != 0)
    {
        assert(nDeleted == 0);
        history_record_text_insert(&s_currTextFile->history, pos, nInserted);
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
    if (s_ignoreModifyCallbacks)
        return;

    if (nDeleted == 0)
        return;
    printf("deleting: pos=%i, nDeleted=%i\n", pos, nDeleted);
    history_record_text_delete(&s_currTextFile->history, pos, nDeleted);
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
    if (f->textbuf->savefile(filename) != 0)
    {
        fl_alert("Failed to save file: %s", strerror(errno));
        return false;
    }
    f->modified = false;
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
    case 0:
        f = open_text_file(chooser.filename());
        set_current_tab(f);
        break;
    case -1:
        fl_alert("Failed to open file: %s", chooser.errmsg());
        break;
    }
}

static int do_save_file_as(struct TextFile *f)
{
    Fl_Native_File_Chooser chooser;
    const char *filename;
    int result;

    chooser.title("Save As");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    result = chooser.show();
    switch (result)
    {
    case 0:  // picked file
        filename = chooser.filename();
        printf("saving file to '%s'\n", filename);
        if (save_text_file(f, filename))
            result = 0;
        else
            result = -1;
        break;
    case 1:  // canceled
        break;
    case -1:  // errored
        fl_alert("Failed to save file: %s", chooser.errmsg());
        break;
    }
    return result;
}

static int do_save(struct TextFile *f)
{
    if (s_currTextFile->filename[0] == 0)
    {
        return do_save_file_as(f);
    }
    else
    {
        printf("saving file to '%s'\n", f->filename);
        if (save_text_file(f, f->filename))
            return 0;
        else
            return -1;
    }
}

static void menu_cb_save_as(Fl_Widget *, void *)
{
    if (do_save_file_as(s_currTextFile) == 0)
    {
        s_mainWindow->label(s_currTextFile->title);
        s_currTextFile->tab->label(s_currTextFile->title);
        s_tabBar->redraw();
    }
}

static void menu_cb_save(Fl_Widget *, void *)
{
    if (do_save(s_currTextFile) == 0)
    {
        s_mainWindow->label(s_currTextFile->title);
        s_currTextFile->tab->label(s_currTextFile->title);
        s_tabBar->redraw();
    }
}

static void menu_cb_close(Fl_Widget *, void *)
{
    if (s_currTextFile->modified)
    {
        int choice = fl_choice(
            "The file '%s' is not saved.\n"
            "Do you want to save it before closing?",
            "Cancel", "Save", "Don't save",
            s_currTextFile->title);
        switch (choice)
        {
        case 0:  // Cancel
            return;
        case 1:  // Save
            if (do_save(s_currTextFile) != 0)
                return;
            break;
        case 2:  // Don't save
            break;
        }
    }

    // remove the file from the editor
    s_tabBar->remove(s_currTextFile->tab);
    delete s_currTextFile->tab;
    file_list_remove(s_currTextFile);
    if (s_textFiles == NULL)
        Fl::delete_widget(s_mainWindow);
    else
    {
        set_current_tab(s_textFiles);
        s_mainWindow->redraw();
    }
}

static void menu_cb_undo(Fl_Widget *, void *)
{
    // Undoing and redoing modifies the textbuf. The callbacks must ignore this
    // and not try to record it again in the command history.
    s_ignoreModifyCallbacks = true;
    history_undo(&s_currTextFile->history);
    s_ignoreModifyCallbacks = false;
}

static void menu_cb_redo(Fl_Widget *, void *)
{
    // Undoing and redoing modifies the textbuf. The callbacks must ignore this
    // and not try to record it again in the command history.
    s_ignoreModifyCallbacks = true;
    history_redo(&s_currTextFile->history);
    s_ignoreModifyCallbacks = false;
}

static void menu_cb_exit(Fl_Widget *, void *)
{
    s_mainWindow->hide();
}

static void menu_cb_find(Fl_Widget *, void *)
{
    find_dialog_show(s_currTextFile->textbuf);
}

static void menu_cb_line_numbers(Fl_Widget *, void *data)
{
    g_settings.lineNumbers = !g_settings.lineNumbers;
    if (g_settings.lineNumbers)
    {
        puts("line numbers enabled");
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

static void menu_cb_gui_theme(Fl_Widget *, void *p)
{
    unsigned int theme = (uintptr_t)p;
    g_settings.theme = theme;
    Fl::scheme(s_themeNames[theme]);
}

static void menu_cb_about(Fl_Widget *, void *)
{
    fl_message("FLedit " APP_VERSION "\nCopyright (c) 2018 Cameron Hall");
}

static void menu_cb_debug(Fl_Widget *, void *)
{
    history_dump();
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
        {"Redo",  FL_COMMAND + 'y', menu_cb_redo},
        {"&Find", FL_COMMAND + 'f', menu_cb_find},
        {0},
    {"&View", 0, NULL, NULL, FL_SUBMENU},
        {"Line Numbers", 0, menu_cb_line_numbers, &s_menuItems[12], FL_MENU_TOGGLE},
        {"Font...",      0, menu_cb_font},
        {"GUI Theme",    0, NULL, NULL, FL_SUBMENU},
            {"None",    0, menu_cb_gui_theme, (void *)0, FL_MENU_RADIO},
            {"Plastic", 0, menu_cb_gui_theme, (void *)1, FL_MENU_RADIO},
            {"GTK+",    0, menu_cb_gui_theme, (void *)2, FL_MENU_RADIO},
            {"Gleam",   0, menu_cb_gui_theme, (void *)3, FL_MENU_RADIO},
            {0},
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
    puts("font changed");
    s_textEditor->textfont(g_settings.fontFace);
    s_textEditor->textsize(g_settings.fontSize);
    s_textEditor->linenumber_font(g_settings.fontFace);
    s_textEditor->linenumber_size(g_settings.fontSize);
    s_textEditor->redraw();
}

static Fl_Window *create_main_window(void)
{
    Fl_Window *w = new Fl_Double_Window(600, 400);

    w->label("FLedit");

    // build widgets
    w->begin();
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

        s_textEditor->remove_key_binding('z', FL_COMMAND);

        find_dialog_init();
        font_dialog_init(cb_on_font_apply);
    }
    w->end();

    w->resizable(s_textEditor);
    return w;
}

static void set_current_tab(struct TextFile *f)
{
    s_currTextFile = f;
    s_textEditor->buffer(f->textbuf);
    s_mainWindow->label(f->title);
    s_tabBar->value(f->tab);
}

int main(int argc, char **argv)
{
    struct TextFile *initFile;
    int exitCode;

    settings_load();

    s_mainWindow = create_main_window();

    //s_mainWindow->show(argc, argv);
    s_mainWindow->show();
    if (argc > 1)
    {
        int i;
        
        for (i = 1; i < argc; i++)
            initFile = open_text_file(argv[i]);
    }
    else
        initFile = open_text_file(NULL);
    set_current_tab(initFile);

    // adjust menu settings
    if (g_settings.lineNumbers)
    {
        Fl_Menu_Item *item = &s_menuItems[14];
        assert(strcmp(item->text, "Line Numbers") == 0);
        item->set();
        s_textEditor->linenumber_width(50);
    }
    if (g_settings.theme >= ARRAY_LENGTH(s_themeNames))
        g_settings.theme = 0;
    Fl::scheme(s_themeNames[g_settings.theme]);

    exitCode = Fl::run();
    settings_save();
    return exitCode;
}
