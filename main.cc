#include "simchcg.h"
#include <gtkmm/application.h>
#include <gtkmm/window.h>

int main(int argc, char** argv)
{
   auto app = Gtk::Application::create(argc, argv, "ht.cartwrig.simchcg");

   Gtk::Window win;
   win.set_title("CHCG Demo");
   win.set_default_size(600,600);
   //win.fullscreen();

   SimCHCG s;
   win.add(s);
   s.show();

   return app->run(win);
}
