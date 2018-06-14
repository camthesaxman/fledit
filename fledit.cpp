#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>

#include "settings.hpp"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct TextFile
{
    bool modified;
    char filename[FL_PATH_MAX];
    char title[FL_PATH_MAX];
    Fl_Text_Buffer *buffer;
    Fl_Group *tab;
    struct TextFile *next;
};

static void set_current_tab(struct TextFile *f);

static Fl_Window *s_mainWindow;
static Fl_Text_Editor *s_textEditor;
static Fl_Tabs *s_tabBar;
static Fl_Window *s_findReplaceDlg;
static struct TextFile *s_textFiles = NULL;
static struct TextFile *s_currTextFile = NULL;

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
    struct TextFile *f = (struct TextFile *)p;

    if (nInserted == 0 && nDeleted == 0)
        return;

    /*
    printf("modified: pos=%i, nInserted=%i, nDeleted=%i, nRestyled=%i\n",
        pos, nInserted, nDeleted, nRestyled);
    */

    if (!f->modified)
    {
        f->modified = true;
        update_file_title(f);
        s_mainWindow->label(f->title);
        f->tab->label(f->title);
        s_tabBar->redraw();
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
}

static struct TextFile *open_text_file(const char *filename)
{
    struct TextFile *f = new TextFile;
    
    memset(f, 0, sizeof(*f));
    f->buffer = new Fl_Text_Buffer;
    strcpy(f->title, "");
    
    if (filename != NULL)
    {
        if (f->buffer->loadfile(filename) == 0)  // succeeded
            strcpy(f->filename, filename);
        else
            fl_alert("Could not open file");
    }
    update_file_title(f);

    f->buffer->add_modify_callback(cb_modified, f);

    // add tab
    f->tab = new Fl_Group(0, 40, 600, 360, f->title);
    f->tab->user_data(f);
    s_tabBar->add_resizable(*f->tab);
    s_mainWindow->redraw();  // Force everything to redraw, because it doesn't happen automatically.

    file_list_append(f);
    return f;
}

static bool save_text_file(struct TextFile *f, const char *filename)
{
    if (f->buffer->savefile(filename) != 0)
    {
        fl_alert(strerror(errno));
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
        fl_alert(chooser.errmsg());
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
        fl_alert(chooser.errmsg());
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
    struct TextFile *next;

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

static void menu_cb_exit(Fl_Widget *, void *)
{
    exit(0);
}

static void menu_cb_find(Fl_Widget *, void *)
{
    s_findReplaceDlg->show();
}

static void menu_cb_line_numbers(Fl_Widget *w, void *)
{
    //Fl_Menu_Item *item = (Fl_Menu_Item *)w;

    //if (item->checked())
    g_settings.lineNumbers = !g_settings.lineNumbers;
    if (g_settings.lineNumbers)
    {
        //puts("line numbers enabled");
        s_textEditor->linenumber_width(50);
        s_textEditor->linenumber_size(g_settings.fontSize);
    }
    else
    {
        //puts("line numbers disabled");
        s_textEditor->linenumber_width(0);
        s_textEditor->linenumber_size(0);
    }
    s_textEditor->redraw();
}

static void menu_cb_gui_theme(Fl_Widget *, void *p)
{
    const char *schemeName = (const char *)p;
    
    Fl::scheme(schemeName);
}

static const Fl_Menu_Item s_menuItems[] =
{
    {"&File", 0, NULL, NULL, FL_SUBMENU},
        {"&New",    FL_COMMAND + 'n', menu_cb_new},
        {"&Open",   FL_COMMAND + 'o', menu_cb_open},
        {"&Save",   FL_COMMAND + 's', menu_cb_save},
        {"Save As", 0,                menu_cb_save_as},
        {"Close",   FL_COMMAND + 'w', menu_cb_close},
        {"Exit",  0,                menu_cb_exit},
        {0},
    {"&Edit", 0, NULL, NULL, FL_SUBMENU},
        {"&Find", FL_COMMAND + 'f', menu_cb_find},
        {0},
    {"&View", 0, NULL, NULL, FL_SUBMENU},
        {"Line Numbers", 0, menu_cb_line_numbers, NULL, FL_MENU_TOGGLE},
        {"GUI Theme",    0, NULL, NULL, FL_SUBMENU},
            {"None",    0, menu_cb_gui_theme, (void *)"none",    FL_MENU_RADIO},
            {"Plastic", 0, menu_cb_gui_theme, (void *)"plastic", FL_MENU_RADIO},
            {"GTK+",    0, menu_cb_gui_theme, (void *)"gtk+",    FL_MENU_RADIO},
            {"Gleam",   0, menu_cb_gui_theme, (void *)"gleam",   FL_MENU_RADIO},
            {0},
        {0},
    {0},
};

static Fl_Window *create_main_window(void)
{
    Fl_Window *w = new Fl_Double_Window(600, 400);

    w->label("FLedit");

    // build widgets
    w->begin();
    {
        Fl_Menu_Bar *m = new Fl_Menu_Bar(0, 0, 600, 20);
        m->copy(s_menuItems);
        s_tabBar = new Fl_Tabs(0, 20, 600, 380);
        s_tabBar->end();
        s_tabBar->callback(cb_tab_change);

        //s_textEditor = new Fl_Text_Editor(0, 20, 600, 380);
        s_textEditor = new Fl_Text_Editor(0+5, 40+5, 600-10, 360-10);
        s_textEditor->textfont(FL_COURIER);
        s_textEditor->textsize(g_settings.fontSize);

        s_findReplaceDlg = new Fl_Window(300, 100, "Find/Replace");
        {
            Fl_Input *findInput = new Fl_Input(80, 10, 210, 25, "Find:");
            findInput->align(FL_ALIGN_LEFT);
            
            Fl_Input *replaceInput = new Fl_Input(80, 40, 210, 25, "Replace:");
            replaceInput->align(FL_ALIGN_LEFT);
            
            Fl_Button *replaceAllBtn = new Fl_Button(10, 70, 90, 25, "Replace All");
            
            Fl_Button *replaceNextBtn = new Fl_Button(105, 70, 120, 25, "Replace Next");
            
            Fl_Button *cancelBtn = new Fl_Button(230, 70, 60, 25, "Cancel");
        }
        s_findReplaceDlg->end();
        s_findReplaceDlg->set_modal();
    }
    w->end();

    w->resizable(s_textEditor);
    return w;
}

static void set_current_tab(struct TextFile *f)
{
    s_currTextFile = f;
    s_textEditor->buffer(f->buffer);
    s_mainWindow->label(f->title);
    s_tabBar->value(f->tab);
}

int main(int argc, char **argv)
{
    struct TextFile *initFile;
    int exitCode;

    //Fl::scheme("gtk+");
    settings_load();

    s_mainWindow = create_main_window();
    
    // adjust menu settings
    menu_cb_line_numbers(NULL, NULL);

    //s_mainWindow->show(argc, argv);
    s_mainWindow->show();
    if (argc > 1)
        initFile = open_text_file(argv[1]);
    else
        initFile = open_text_file(NULL);
    set_current_tab(initFile);

    exitCode = Fl::run();
    puts("exiting");
    settings_save();
    return exitCode;
}
