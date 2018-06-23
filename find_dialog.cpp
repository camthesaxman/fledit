#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/fl_ask.H>

#include "fledit.hpp"

static Fl_Window *s_findDialog;
static Fl_Input *s_findInput;
static Fl_Text_Buffer *s_textBuf;
static int s_currPos;

static void cb_on_find(Fl_Widget *, void *data)
{
    const char *text = s_findInput->value();
    int length = strlen(text);
    int foundPos;
    int matchCase = 0;

    if (length != 0)
    {
        bool forward = (bool)data;
        bool found;

        if ((bool)forward)
            found = s_textBuf->search_forward(s_currPos, text, &foundPos, matchCase);
        else
            found = s_textBuf->search_backward(s_currPos, text, &foundPos, matchCase);

        if (found)
        {
            s_textBuf->highlight(foundPos, foundPos + length);
            s_currPos = foundPos + (forward ? length : -1);
        }
        else
            fl_alert("'%s' was not found.", text);
    }
}

static void cb_on_cancel(Fl_Widget *, void *)
{
    s_findDialog->hide();
}

void find_dialog_init(void)
{
    s_findDialog = new Fl_Window(300, 100, "Find/Replace");
    {
        s_findInput = new Fl_Input(80, 10, 210, 25, "Find:");
        s_findInput->align(FL_ALIGN_LEFT);

        Fl_Input *replaceInput = new Fl_Input(80, 40, 210, 25, "Replace:");
        replaceInput->align(FL_ALIGN_LEFT);

        Fl_Button *findPrev = new Fl_Button(10, 70, 100, 25, "@<- Previous");
        findPrev->callback(cb_on_find, (void *)false);

        Fl_Button *findNext = new Fl_Button(115, 70, 100, 25, "@-> Next");
        findNext->callback(cb_on_find, (void *)true);

        //Fl_Button *replaceAllBtn = new Fl_Button(105, 70, 120, 25, "Replace All");

        Fl_Button *cancelBtn = new Fl_Button(230, 70, 60, 25, "Cancel");
        cancelBtn->callback(cb_on_cancel);
    }
    s_findDialog->end();
    s_findDialog->set_modal();

}

void find_dialog_show(Fl_Text_Buffer *textBuf)
{
    s_currPos = 0;
    s_textBuf = textBuf;
    s_findDialog->show();
}
