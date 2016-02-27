#include "sim1942.h"
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/std_pair.hpp>
namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;


namespace boost { namespace program_options {

template<>
typed_value<bool> *value(bool *v) {
    return bool_switch(v);
}

}}

#include <iostream>
#include <string>
#include <exception>

bool on_key(GdkEventKey* event, Gtk::Window *win) {
	if(event->keyval == GDK_KEY_Escape) {
		win->hide();

		return false;
	}
	return false;
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

barriers_t process_map_file(const std::string& name);

int main(int argc, char** argv) {
    Glib::set_application_name("1942");

    auto app = Gtk::Application::create(argc, argv, "ht.cartwrig.mcmxlii",
   		Gio::APPLICATION_HANDLES_COMMAND_LINE);
    app->signal_command_line().connect(
        [&](const Glib::RefPtr<Gio::ApplicationCommandLine> &) -> int {
            app->activate();
            return 0;
        }, false);


    po::options_description desc{"Allowed Options"};
    arg_t arg;
    try {
        arg = process_command_line(&desc, argc, argv);
    } catch(std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    Gtk::ApplicationWindow win;
    win.set_title("1942");
    win.set_icon_name("mcmxlii");
    win.set_default_size(arg.win_width,arg.win_height);
    win.set_border_width(0);

    win.add_events(Gdk::KEY_PRESS_MASK);
    win.signal_key_press_event().connect([&](GdkEventKey* event) -> bool {
        return on_key(event,&win);
    }, true);

    if(arg.fullscreen) {
        auto m = win.get_display()->get_device_manager()->get_client_pointer();
        auto s = win.get_screen();
        int mx,my;
        m->get_position(s,mx,my);
        win.set_default_size(1,1);
        win.move(mx,my);
        win.fullscreen();
    }

    if(arg.help) {
        std::cerr << "Usage:\n  " << arg.run_name << " [ options ]\n";
        std::cerr << desc << "\n";
        return 0;
    }

    if(arg.width <= 0 || arg.height <= 0 || arg.mu <= 0.0) {
        std::cerr << "Invalid command line arguments." << std::endl;
        return 1;
    }
    barriers_t barriers;
    if(!arg.map_file.empty()) {
		std::cout << "Reading map from file \"" << arg.map_file << "\".\n";
        barriers = process_map_file(arg.map_file);
        if(barriers.empty()) {
            std::cerr << "Unable to process map file." << std::endl;
            return 2;
        }
    }

    Sim1942 s(arg.width,arg.height,arg.mu,arg.delay);
    s.name(arg.text.c_str());
    s.name_scale(arg.text_scale);
    if(!barriers.empty()) {
        s.barriers(barriers);
    }

    if(arg.colortest) {
    	s.signal_draw().connect([&](const Cairo::RefPtr<Cairo::Context>& cr) -> bool {
    		auto rect = s.get_allocation();
    		double width = rect.get_width();
    		double height = rect.get_height();

    		cr->set_antialias(Cairo::ANTIALIAS_NONE);
    		cr->set_source_rgba(0.0,0.0,0.0,1.0);
    		cr->paint();
    		double w = width/num_colors/2;
    		double h = height/num_colors;

    		for(int a=0;a<num_colors;++a) {
    			cr->set_source_rgba(
                	col_set[a].red, col_set[a].blue,
                	col_set[a].green, col_set[a].alpha
            		);
            	cr->rectangle(2*w*a,0.0,w,height);
            	cr->fill();
                for(int b=0;b<num_colors;++b) {
                    cr->set_source_rgba(
                        col_set[b].red, col_set[b].blue,
                        col_set[b].green, col_set[b].alpha
                        );
                    cr->rectangle(w+2*w*a,b*h,w,h);
                    cr->fill();
        		}
            }
    		return true;
    	}, false);
    }

   	win.add(s);
    win.show_all();

    int status = app->run(win);
    app->remove_window(win);

    return status;
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

barriers_t process_map_file(const std::string& name) {
    using qi::int_;
    using qi::lexeme;
    using qi::lit;

    std::ifstream map_file(name, std::ios::binary);
    if(!map_file) {
        // unable to open file, return empty vector
        return {};
    }
    map_file.unsetf(std::ios::skipws);

    spirit::ascii::space_type space;
    barriers_t map_data;

    auto b = spirit::istream_iterator(map_file);
    auto e = spirit::istream_iterator();
    bool r = qi::phrase_parse(b, e, +(lexeme[int_] >> lit(',') >> int_),
        space, qi::skip_flag::postskip, map_data);
    if(!r || b != e) {
        // parsing failed or was incomplete, return empty vector
        return {};
    }
    return map_data;
}
