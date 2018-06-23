#include <FL/Fl.H>
#include <FL/Fl_Window.H>

int main(void)
{
    Fl_Window *win = new Fl_Window(400, 400, "Test App");
    {
        
    }
    win->end();
    
    win->show();
    Fl::run();
}