struct Settings
{
    bool lineNumbers;
    int fontSize;
};

extern struct Settings g_settings;

void settings_load(void);
void settings_save(void);
