#include <stdint.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Buffer.H>

#include "fledit.hpp"

static Fl_Window *s_fontDlg;
static Fl_Browser *s_fontBrowser;
static Fl_Browser *s_sizeBrowser;
static Fl_Input *s_fontPreview;
static int s_selectedSize;
static Fl_Font s_selectedFont;
static void (*s_fontApplyCallback)(void);

static const int s_defaultSizes[] = {
    6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 28, 32, 36,
    40, 48, 56, 64, 72
};

static void cb_on_font_select(Fl_Widget *, void *)
{
    Fl_Font font = s_fontBrowser->value() - 1;

    s_sizeBrowser->clear();
    if (font != -1)
    {
        int *sizes;
        int numSizes = Fl::get_font_sizes(font, sizes);
        int i;

        s_selectedFont = font;
        printf("numSizes = %i\n", numSizes);
        if (numSizes > 0)
        {
            if (sizes[0] == 0)  // Any size is possible
            {
                sizes = (int *)s_defaultSizes;
                numSizes = ARRAY_LENGTH(s_defaultSizes);
            }

            for (i = 0; i < numSizes; i++)
            {
                char buf[3];

                snprintf(buf, sizeof(buf), "%i", sizes[i]);
                s_sizeBrowser->add(buf, (void *)(uintptr_t)sizes[i]);
                if (sizes[i] == s_selectedSize)
                    s_sizeBrowser->select(i + 1);
            }
            s_fontPreview->textfont(s_selectedFont);
            s_fontPreview->redraw();
        }
    }
}

static void cb_on_size_select(Fl_Widget *, void *)
{
    s_selectedSize = (uintptr_t)s_sizeBrowser->data(s_sizeBrowser->value());
    s_fontPreview->textsize(s_selectedSize);
    s_fontPreview->redraw();
}

static void cb_on_cancel(Fl_Widget *, void *)
{
    s_fontDlg->hide();
}

static void cb_on_apply(Fl_Widget *, void *)
{
    g_settings.fontFace = s_selectedFont;
    g_settings.fontSize = s_selectedSize;
    s_fontApplyCallback();
}

static void cb_on_ok(Fl_Widget *, void *)
{
    s_fontDlg->hide();
    g_settings.fontFace = s_selectedFont;
    g_settings.fontSize = s_selectedSize;
    s_fontApplyCallback();
}

void font_dialog_init(void (*applyCallback)(void))
{
    Fl_Font i;
    const int btnHeight = 24;
    const int btnWidth = 80;

    s_fontApplyCallback = applyCallback;
    s_selectedFont = g_settings.fontFace;
    s_selectedSize = g_settings.fontSize;

    s_fontDlg = new Fl_Window(330, 410, "Font");
    {
        Fl_Button *cancelButton;
        Fl_Button *applyButton;
        Fl_Button *okButton;

        s_fontBrowser = new Fl_Hold_Browser(10, 20, 200, 300, "Font");
        s_fontBrowser->align(FL_ALIGN_TOP);
        for (i = 0; i <= 15; i++)
            s_fontBrowser->add(Fl::get_font_name(i));
        s_fontBrowser->callback(cb_on_font_select);

        s_sizeBrowser = new Fl_Hold_Browser(220, 20, 100, 300, "Size");
        s_sizeBrowser->align(FL_ALIGN_TOP);
        s_sizeBrowser->callback(cb_on_size_select);

        s_fontPreview = new Fl_Input(10, 340, 310, 24, "Preview");
        s_fontPreview->value("the quick brown fox jumped over the lazy dog");
        s_fontPreview->align(FL_ALIGN_TOP | FL_ALIGN_LEFT);

        cancelButton = new Fl_Button(330 - (btnWidth + 10) * 3, 410 - (btnHeight + 10), btnWidth, btnHeight, "Cancel");
        applyButton = new Fl_Button(330 - (btnWidth + 10) * 2, 410 - (btnHeight + 10), btnWidth, btnHeight, "Apply");
        okButton = new Fl_Return_Button(330 - (btnWidth + 10) * 1, 410 - (btnHeight + 10), btnWidth, btnHeight, "OK");
        cancelButton->callback(cb_on_cancel);
        applyButton->callback(cb_on_apply);
        okButton->callback(cb_on_ok);
    }
    s_fontDlg->end();

    s_fontDlg->set_modal();
}

void font_dialog_open(void)
{
    s_fontDlg->show();
}
