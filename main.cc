#include "simchcg.h"
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

namespace boost { namespace program_options {

template<>
typed_value<bool> *value(bool *v) {
    return bool_switch(v);
}

}}


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

// use X-Macros to specify argument variables
struct arg_t {
#define XM(lname, sname, desc, type, def) type XV(lname) ;
# include "main.xmh"
#undef XM
    std::string run_name;
    std::string run_path;
};

arg_t process_command_line(po::options_description *opt_desc, int argc, char** argv);

int main(int argc, char** argv) {
    auto app = Gtk::Application::create(argc, argv, "ht.cartwrig.simchcg",
   		Gio::APPLICATION_HANDLES_COMMAND_LINE);
    app->signal_command_line().connect(
      sigc::bind(sigc::ptr_fun(on_cmd), app), false);

    po::options_description desc{"Allowed Options"};
    arg_t arg;
    try {
        arg = process_command_line(&desc, argc, argv);
    } catch(std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    Gtk::Window win{};
    win.set_title("Human and Comparative Genomics Laboratory");
    win.set_default_size(arg.win_width,arg.win_height);
    win.set_border_width(0);

    if(arg.fullscreen) {
        win.fullscreen();
    }

    win.add_events(Gdk::KEY_PRESS_MASK);
    win.signal_key_press_event().connect(
        sigc::bind(sigc::ptr_fun(&on_key), &win),false);

    if(arg.help) {
        std::cerr << "Usage:\n  " << arg.run_name << " [ options ]\n";
        std::cerr << desc << "\n";
        return 0;
    }

    if(arg.width <= 0 || arg.height <= 0 || arg.mu <= 0.0) {
        std::cerr << "Invalid command line arguments." << std::endl;
        return 1;
    }

    SimCHCG s(arg.width,arg.height,arg.mu,arg.delay);
    s.name(arg.text.c_str());
    s.name_scale(arg.text_scale);
    win.add(s);
    s.show();

    return app->run(win);
}

arg_t process_command_line(po::options_description *opt_desc, int argc, char** argv) {
    po::variables_map vm;
    arg_t arg;
    boost::filesystem::path bin_path(argv[0]);
    arg.run_name = bin_path.filename().generic_string();
    arg.run_path = bin_path.parent_path().generic_string();

    opt_desc->add_options()
    #define XM(lname, sname, desc, type, def) ( \
        XS(lname) IFD(sname, "," BOOST_PP_STRINGIZE sname), \
        po::value< type >(&arg.XV(lname))->default_value(def), \
        desc )
    #   include "main.xmh"
    #undef XM
        ;


    po::store(po::command_line_parser(argc, argv).options(*opt_desc).run(), vm);
    po::notify(vm);

    return arg;
}