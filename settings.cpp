#include <stdio.h>

#include "settings.hpp"

#define CONFIG_FILE_NAME "~/.config/fledit.cfg"

struct Settings g_settings = {0};

static void load_defaults(void)
{
    g_settings.lineNumbers = true;
    g_settings.fontSize = 14;
}

void settings_load(void)
{
    FILE *file = fopen(CONFIG_FILE_NAME, "rb");

    if (file == NULL)
    {
        puts("could not load config file. using defaults");
        load_defaults();
        return;
    }
    
    fclose(file);
}

void settings_save(void)
{
    
}
