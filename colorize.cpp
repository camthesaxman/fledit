#include <ctype.h>
#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>

#include "fledit.hpp"

const char *const s_keywords[] = {
    "asm",
    "break",
    "case",
    "const",
    "continue",
    "default",
    "do",
    "else",
    "extern",
    "false",
    "for",
    "goto",
    "if",
    "return",
    "static",
    "struct",
    "switch",
    "true",
    "typedef",
    "union",
    "volatile",
    "while",
};

Fl_Text_Display::Style_Table_Entry s_styleTable[] = {
    {FL_BLACK,       FL_COURIER, 14},  // plain
    {FL_DARK_GREEN,  FL_COURIER, 14},  // comment
    {FL_DARK_YELLOW, FL_COURIER, 14},  // string
    {FL_BLUE,        FL_COURIER, 14},  // keyword
    {FL_RED,         FL_COURIER, 14},  // preprocessor
};

Fl_Text_Buffer *colorize_init(Fl_Text_Buffer *textbuf)
{
    size_t length = textbuf->length();
    Fl_Text_Buffer *stylebuf = new Fl_Text_Buffer(length);
    char *style = new char[length + 1];
    unsigned int i;

    memset(style, 'A', length);
    style[length] = 0;
    stylebuf->text(style);
    delete[] style;
    return stylebuf;
}

static bool is_word_char(int c)
{
    return (isalnum(c) || c == '_');
}

static void highlight_c(Fl_Text_Buffer *textbuf, char *style, int length)
{
    enum
    {
        NORMAL,
        MULTI_COMMENT,
        SINGLE_COMMENT,
        DOUBLE_QUOTE_STRING,
        SINGLE_QUOTE_STRING,
        PREPROC_DIRECTIVE,
    };
    int state = NORMAL;
    int pos;
    int prevChar = 0;
    int currChar;
    bool backslashEscape = false;  // whether the current char is escaped by a backslash
    bool isCleanLine = true;  // whether the current line contains only comments or whitespace
    int wordStart = -1;

    for (pos = 0; pos < length; pos++)
    {
        currChar = *textbuf->address(pos);
        style[pos] = 'A';
        backslashEscape = backslashEscape ? false : (prevChar == '\\');
        if (state != NORMAL)
            wordStart = -1;

        switch (state)
        {
        case NORMAL:
            style[pos] = 'A';
            if (prevChar == '/' && currChar == '*')
            {
                state = MULTI_COMMENT;
                style[pos - 1] = 'B';
                style[pos] = 'B';
            }
            else if (prevChar == '/' && currChar == '/')
            {
                state = SINGLE_COMMENT;
                style[pos - 1] = 'B';
                style[pos] = 'B';
            }
            else if (currChar == '"')
            {
                state = DOUBLE_QUOTE_STRING;
                style[pos] = 'C';
            }
            else if (currChar == '\'')
            {
                state = SINGLE_QUOTE_STRING;
                style[pos] = 'C';
            }
            else if (currChar == '#' && isCleanLine)
            {
                state = PREPROC_DIRECTIVE;
                style[pos] = 'E';
            }
            else
            {
                if (wordStart == -1)  // last character was not a letter
                {
                    if (is_word_char(currChar))  // start of word
                        wordStart = pos;
                }
                else
                {
                    if (!is_word_char(currChar))  // end of word
                    {
                        // find keyword
                        unsigned int i, j;
                        int wordLen = pos - wordStart;

                        for (i = 0; i < ARRAY_LENGTH(s_keywords); i++)
                        {
                            const char *keyword = s_keywords[i];

                            for (j = 0; j < wordLen && keyword[j] != 0; j++)
                            {
                                if (*textbuf->address(wordStart + j) != keyword[j])
                                    break;
                            }
                            if (j == wordLen && keyword[j] == 0)  // found keyword
                            {
                                for (j = wordStart; j < pos; j++)
                                    style[j] = 'D';
                                break;
                            }
                        }
                        wordStart = -1;
                    }
                }

                if (!isspace(currChar))
                    isCleanLine = false;
                else if (currChar == '\n')
                    isCleanLine = true;
            }
            break;
        case MULTI_COMMENT:
            style[pos] = 'B';
            if (prevChar == '*' && currChar == '/')
                state = NORMAL;
            break;
        case SINGLE_COMMENT:
            style[pos] = 'B';
            if (currChar == '\n')
            {
                state = NORMAL;
                isCleanLine = true;
            }
            break;
        case DOUBLE_QUOTE_STRING:
            style[pos] = 'C';
            if (currChar == '"' && !backslashEscape)
                state = NORMAL;
            break;
        case SINGLE_QUOTE_STRING:
            style[pos] = 'C';
            if (currChar == '\'' && !backslashEscape)
                state = NORMAL;
            break;
        case PREPROC_DIRECTIVE:
            style[pos] = 'E';
            if (currChar == '\n')
            {
                state = NORMAL;
                isCleanLine = true;
            }
            break;
        }

        prevChar = currChar;
    }
}

void colorize_update(Fl_Text_Editor *editor, Fl_Text_Buffer *textbuf, Fl_Text_Buffer *stylebuf)
{
    int length = textbuf->length();
    char *style = new char[length + 1];
    int i;

    highlight_c(textbuf, style, length);
    style[length] = 0;
    stylebuf->text(style);
    delete[] style;
    editor->highlight_data(stylebuf, s_styleTable, ARRAY_LENGTH(s_styleTable),
        'A', NULL, NULL);
}

void colorize_clear(Fl_Text_Editor *editor, Fl_Text_Buffer *stylebuf)
{
    editor->highlight_data(stylebuf, s_styleTable, 1, 'A', NULL, NULL);
}

void colorize_update_font(Fl_Font font, int size)
{
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH(s_styleTable); i++)
    {
        s_styleTable[i].font = font;
        s_styleTable[i].size = size;
    }
}
