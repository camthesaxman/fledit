#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FL/Fl.H>

#include "fledit.hpp"

struct Settings g_settings = {0};

enum ConfigOptionType {TYPE_BOOL, TYPE_UINT};

struct ConfigOption
{
    const char *name;
    enum ConfigOptionType type;
    void *value;
};

static const struct ConfigOption s_configOptions[] =
{
    {"line_numbers",             TYPE_BOOL, &g_settings.lineNumbers},
    {"font_face",                TYPE_UINT, &g_settings.fontFace},
    {"font_size",                TYPE_UINT, &g_settings.fontSize},
    {"theme",                    TYPE_UINT, &g_settings.theme},
    {"syntax_highlighting",      TYPE_BOOL, &g_settings.syntaxHighlighting},
    {"mark_double_clicked_word", TYPE_BOOL, &g_settings.markDoubleClickedWord},
};

static char *s_configFileName = NULL;

static void load_defaults(void)
{
    g_settings.lineNumbers = true;
    g_settings.fontFace = FL_COURIER;
    g_settings.fontSize = 14;
    g_settings.theme = 0;
    g_settings.syntaxHighlighting = true;
    g_settings.markDoubleClickedWord = false;
}

static char *choose_config_file_path(void)
{
    const char *homeDir = getenv("HOME");
    static char path[256];

    if (homeDir == NULL || homeDir[0] == 0)
        return NULL;

    snprintf(path, sizeof(path), "%s/.config/fledit.cfg", homeDir);
    return path;
}

static char *line_split(char *text)
{
    while (*text != '\n')
    {
        if (*text == 0)
            return text;
        text++;
    }
    *text = 0;
    return text + 1;
}

static void config_parse_error(int lineNum, const char *text, ...)
{
    va_list args;

    fprintf(stderr, "error parsing config file: line %i: ", lineNum);
    va_start(args, text);
    vfprintf(stderr, text, args);
    va_end(args);
    fputc('\n', stderr);
}

static void parse_config_line(char *text, int lineNum)
{
    char *name;
    char *nameEnd;
    char *value;
    char *valueEnd;
    unsigned int i;

    name = text;
    // find next character that is a space or =
    while (!isspace(*text) && *text != '=' && *text != 0)
        text++;
    nameEnd = text;
    while (isspace(*text))
        text++;
    if (*text != '=')
    {
        config_parse_error(lineNum, "missing '='");
        return;
    }
    text++;
    while (isspace(*text))
        text++;
    value = text;
    while (!isspace(*text) && *text != 0)
        text++;
    valueEnd = text;

    *nameEnd = 0;
    *valueEnd = 0;

    for (i = 0; i < ARRAY_LENGTH(s_configOptions); i++)
    {
        const struct ConfigOption *option = &s_configOptions[i];

        if (strcmp(option->name, name) == 0)
        {
            switch (option->type)
            {
            case TYPE_BOOL:
                if (strcmp(value, "true") == 0)
                    *(bool *)option->value = true;
                else if (strcmp(value, "false") == 0)
                    *(bool *)option->value = false;
                else
                {
                    config_parse_error(lineNum, "option '%s' requires true or false", name);
                    return;
                }
                break;
            case TYPE_UINT:
                {
                    char *endptr;
                    unsigned long int n = strtoul(value, &endptr, 0);
                    if (endptr > value)
                        *(unsigned int *)option->value = n;
                    else
                    {
                        config_parse_error(lineNum, "option '%s' requires an integer", name);
                        return;
                    }
                }
                break;
            }
            return;
        }
    }
    config_parse_error(lineNum, "unknown option '%s'", name);
}

static void parse_config(char *text)
{
    char *line = text;
    int lineNum = 1;

    while (*line != 0)
    {
        char *nextLine = line_split(line);
        parse_config_line(line, lineNum);
        line = nextLine;
        lineNum++;
    }
}

static void write_config(FILE *file)
{
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH(s_configOptions); i++)
    {
        const struct ConfigOption *option = &s_configOptions[i];

        fputs(option->name, file);
        fputs(" = ", file);
        switch (option->type)
        {
        case TYPE_BOOL:
            fputs(*(bool *)option->value ? "true\n" : "false\n", file);
            break;
        case TYPE_UINT:
            fprintf(file, "%u\n", *(unsigned int *)option->value);
            break;
        }
    }
}

void settings_load(void)
{
    FILE *file;
    size_t size;
    char *buffer;

    load_defaults();

    s_configFileName = choose_config_file_path();
    if (s_configFileName == NULL)
    {
        fputs("could not determine path of config file. using defaults\n", stderr);
        return;
    }

    file = fopen(s_configFileName, "rb");
    if (file == NULL)
    {
        printf("could not load config file '%s'. using defaults\n", s_configFileName);
        return;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    buffer = (char *)malloc(size + 1);
    fseek(file, 0, SEEK_SET);
    if (fread(buffer, size, 1, file) != 1)
    {
        fprintf(stderr, "error reading from file '%s'.\n", s_configFileName);
        goto done;
    }
    buffer[size] = 0;

    parse_config(buffer);

done:
    free(buffer);
    fclose(file);
}

void settings_save(void)
{
    FILE *file;

    if (s_configFileName == NULL)
        return;

    file = fopen(s_configFileName, "wb");
    if (file == NULL)
    {
        fprintf(stderr, "could not open file '%s' for writing.\n", s_configFileName);
        return;
    }

    write_config(file);
    fclose(file);
}
