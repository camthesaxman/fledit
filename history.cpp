#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>

#include "fledit.hpp"

enum HistoryCommandAction {ACTION_ADD, ACTION_DELETE};

struct HistoryCommand
{
    struct HistoryCommand *prev;
    struct HistoryCommand *next;
    enum HistoryCommandAction action;
    unsigned int pos;
    char *text;
};

// Adds a new command to the current point in history, deleting the old redo path.
static struct HistoryCommand *history_new_cmd(struct History *h)
{
    struct HistoryCommand *cmd = new struct HistoryCommand;
    cmd->prev = NULL;
    cmd->next = NULL;

    // Delete the old redo path
    struct HistoryCommand *redo = h->redoCmd;
    while (redo != NULL)
    {
        struct HistoryCommand *next = redo->next;
        free(redo->text);
        delete redo;
        redo = next;
    }

    // Append the new command to the end of the list as the current undo command.
    if (h->undoCmd != NULL)
    {
        h->undoCmd->next = cmd;
        cmd->prev = h->undoCmd;
    }
    h->undoCmd = cmd;
    h->redoCmd = cmd->next;
    return cmd;
}

void history_record_text_insert(struct History *h, unsigned int pos, unsigned int nInserted)
{
    struct HistoryCommand *cmd = h->undoCmd;
    char *newText = h->textbuf->text_range(pos, pos + nInserted);

    // Add this to the current command if the insert was immediately after it.
    if (cmd != NULL && cmd->action == ACTION_ADD
     && pos == cmd->pos + strlen(cmd->text))
    {
        int newSize = strlen(cmd->text) + nInserted + 1;

        cmd->text = (char *)realloc(cmd->text, newSize);
        strcat(cmd->text, newText);
        free(newText);
    }
    // Otherwise, make a new command.
    else
    {
        cmd = history_new_cmd(h);
        cmd->action = ACTION_ADD;
        cmd->text = newText;
        cmd->pos = pos;
    }
    
    //printf("history: added '%s' at %i\n", cmd->text, pos);
}

void history_record_text_delete(struct History *h, unsigned int pos, unsigned int nDeleted)
{
    struct HistoryCommand *cmd = h->undoCmd;
    char *deletedText = h->textbuf->text_range(pos, pos + nDeleted);

    // Add this to the current command if the delete was immediately before it. (backspacing multiple characters)
    if (cmd != NULL && cmd->action == ACTION_DELETE
     && pos + strlen(deletedText) == cmd->pos)
    {
        int newSize = strlen(cmd->text) + nDeleted + 1;

        char *buf = (char *)malloc(newSize);
        strcpy(buf, deletedText);
        strcat(buf, cmd->text);
        free(deletedText);
        free(cmd->text);
        cmd->text = buf;
        cmd->pos = pos;
    }
    // Add this to the current command if the position is the same. (using the delete key on multiple characters)
    else if (cmd != NULL && cmd->action == ACTION_DELETE
     && pos == cmd->pos)
    {
        int newSize = strlen(cmd->text) + nDeleted + 1;

        cmd->text = (char *)realloc(cmd->text, newSize);
        strcat(cmd->text, deletedText);
        free(deletedText);
    }
    // Otherwise, make a new command.
    else
    {
        cmd = history_new_cmd(h);
        cmd->action = ACTION_DELETE;
        cmd->text = deletedText;
        cmd->pos = pos;
    }

    //printf("history: deleted '%s' at %i\n", cmd->text, pos);
}

void history_undo(struct History *h)
{
    struct HistoryCommand *cmd = h->undoCmd;

    if (cmd == NULL)
        return;

    switch (cmd->action)
    {
    case ACTION_ADD:
        h->textbuf->remove(cmd->pos, cmd->pos + strlen(cmd->text));
        break;
    case ACTION_DELETE:
        h->textbuf->insert(cmd->pos, cmd->text);
        break;
    }

    h->redoCmd = h->undoCmd;
    h->undoCmd = h->undoCmd->prev;
}

void history_redo(struct History *h)
{
    struct HistoryCommand *cmd = h->redoCmd;

    if (cmd == NULL)
        return;

    switch (cmd->action)
    {
    case ACTION_ADD:
        h->textbuf->insert(cmd->pos, cmd->text);
        break;
    case ACTION_DELETE:
        h->textbuf->remove(cmd->pos, cmd->pos + strlen(cmd->text));
        break;
    }

    h->undoCmd = h->redoCmd;
    h->redoCmd = h->redoCmd->next;
}
