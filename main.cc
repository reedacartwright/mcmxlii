#include "simchcg.h"
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include <iostream>
#include <string>
#include <exception>

int on_cmd(const Glib::RefPtr<Gio::ApplicationCommandLine> &,
  Glib::RefPtr<Gtk::Application> &app) {
    app->activate();
    return 0;
}

bool on_key(GdkEventKey* event, Gtk::Window *win) {
	if(event->keyval == GDK_KEY_Escape) {
		win->hide();
		return true;
	}
	return true;
}

int main(int argc, char** argv)
{
   auto app = Gtk::Application::create(argc, argv, "ht.cartwrig.simchcg",
   		Gio::APPLICATION_HANDLES_COMMAND_LINE);
    app->signal_command_line().connect(
      sigc::bind(sigc::ptr_fun(on_cmd), app), false);

   Gtk::Window win{};
   win.set_title("Center for Human and Comparative Genomics");
   win.set_default_size(1920,1080);
   win.set_border_width(0);
   win.fullscreen();

   win.add_events(Gdk::KEY_PRESS_MASK);
   win.signal_key_press_event().connect(
   	sigc::bind(sigc::ptr_fun(&on_key), &win),false);


   if(argc < 4) {
   	std::cerr << "Usage: " << argv[0] << " width height mutation_rate [name] [name scale]"  << std::endl;
   	return 1;
   }
   int width, height;
   double mu, scale;
   try {
   	width = std::stoi(argv[1]);
   	height = std::stoi(argv[2]);
   	mu = std::stod(argv[3]);
   } catch(std::exception &e) {
   	std::cerr << "Invalid command line arguments." << std::endl;
   	return 1;
   }
   if(width <= 0 || height <= 0 || mu <= 0.0) {
   	std::cerr << "Invalid command line arguments." << std::endl;
   	return 1;   	
   }

   SimCHCG s(width,height,mu);
   if(argc >= 5)
      s.name(argv[4]);
   if(argc >= 6) {
     try {
      scale = std::stoi(argv[5]);
     } catch(std::exception &e) {
      std::cerr << "Invalid command line arguments." << std::endl;
      return 1;
     }
     s.name_scale(scale);
  }
   win.add(s);
   s.show();

   return app->run(win);
}
