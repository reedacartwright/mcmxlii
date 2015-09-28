#include <cmath>
#include <algorithm>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include <gdkmm/cursor.h>
#include <iostream>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <dbus/dbus.h>

#include "simchcg.h"

#include "logo.inl"

#define OUR_FRAME_RATE 15

SimCHCG::SimCHCG(int width, int height, double mu, int delay) :
    grid_width_{width}, grid_height_{height}, mu_(mu),
    worker_{width,height,mu,delay}
{
    Glib::signal_timeout().connect( sigc::mem_fun(*this, &SimCHCG::on_timeout), 1000.0/OUR_FRAME_RATE );

    #ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
    //Connect the signal handler if it isn't already a virtual method override:
    signal_draw().connect(sigc::mem_fun(*this, &SimCHCG::on_draw), false);
    #endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

    try {
        logo_ = Gdk::Pixbuf::create_from_inline(-1,logo_inline,false);
    } catch(...) {
        /* do nothing */
    }

    worker_thread_ = Glib::Threads::Thread::create([&]{
        worker_.do_work(this);
    });
}

SimCHCG::~SimCHCG()
{
    worker_.stop();
    worker_thread_->join();
}

void SimCHCG::on_realize() {
    // https://dxr.mozilla.org/mozilla-central/source/widget/gtk/WakeLockListener.cpp
    Gtk::DrawingArea::on_realize();
    auto p = get_window();
    auto cursor = Gdk::Cursor::create(Gdk::BLANK_CURSOR);
    p->set_cursor(cursor);

    uint32_t xid = GDK_WINDOW_XID(Glib::unwrap(p));

    DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
    if(connection == nullptr)
        return;

    DBusMessage* message = dbus_message_new_method_call(
        "org.gnome.SessionManager", "/org/gnome/SessionManager",
        "org.gnome.SessionManager", "Inhibit");
    if(message == nullptr)
        return;

    const uint32_t flags = (1 << 3); // Inhibit idle
    const char *app = "SimCHCG";
    const char *topic = "Fullscreen Mode";

    dbus_message_append_args(message,
        DBUS_TYPE_STRING, &app,   DBUS_TYPE_UINT32, &xid,
        DBUS_TYPE_STRING, &topic, DBUS_TYPE_UINT32, &flags,
        DBUS_TYPE_INVALID );

    dbus_connection_send(connection, message, nullptr);
    dbus_connection_flush(connection);
    dbus_message_unref(message);
    dbus_connection_unref(connection);
}

void SimCHCG::on_unrealize() {
    Gtk::DrawingArea::on_unrealize();
}

bool SimCHCG::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  cr->set_antialias(Cairo::ANTIALIAS_NONE);

  auto allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();

  double w = 1.0*width/grid_width_;
  double h = 1.0*height/grid_height_;

  double cell_size = 1.0;
  cr->save();
  if(w <= h) {
    // width is the limiting space
    double size = 1.0*grid_height_*width/grid_width_;
    cr->translate(0,(height-size)/2);
    cr->scale(w,w);
  } else {
    // height is the limiting space
    double size = 1.0*grid_width_*height/grid_height_;
    cr->translate((width-size)/2,0);
    cr->scale(h,h);
  }
  cr->set_source_rgba(0.0,0.0,0.0,1.0);
  cr->paint();
  pop_t data(grid_width_*grid_height_);

  data = worker_.get_data();
  unsigned long long gen = worker_.get_gen();
  for(int y=0;y<grid_height_;++y) {
    for(int x=0;x<grid_width_;++x) {
      int a = static_cast<int>(data[x+y*grid_width_].type & 0xFF);
      cr->set_source_rgba(
        col_set[a].red, col_set[a].blue,
        col_set[a].green, col_set[a].alpha
      );
      cr->rectangle(x,y,1.0,1.0);
      cr->fill();
    }
  }
  double east = 0, north = 0, west = grid_width_, south = grid_height_;
  cr->user_to_device(west,south);
  cr->user_to_device(east,north);
  cr->restore();

  int logo_top = south, logo_mid = south;
  if(logo_) {
    logo_top = south-logo_->get_height()-0.025*(south-north);
    //logo_mid = south-logo_->get_height()/2;
    Gdk::Cairo::set_source_pixbuf(cr, logo_, east+0.025*(west-east), logo_top);
    cr->paint_with_alpha(0.9);
  }

  Pango::FontDescription font;
  font.set_weight(Pango::WEIGHT_BOLD);

  cr->set_antialias(Cairo::ANTIALIAS_GRAY);
  auto layout = create_pango_layout(name_.c_str());
  int text_width, text_height;

  font.set_family("TeX Gyre Adventor");
  font.set_size(name_scale_*48*PANGO_SCALE);
  layout->set_font_description(font);
  layout->set_alignment(Pango::ALIGN_CENTER);
  layout->get_pixel_size(text_width,text_height);  
  cr->move_to(east+(west-east)/2.0-text_width/2.0, north+(logo_top-north)/2.0-text_height/2.0);
  layout->add_to_cairo_context(cr);
  cr->set_source_rgba(1.0,1.0,1.0,0.9);
  cr->fill();
  // NOTE: If you want to add an outline, uncomment these lines and comment the line above.
  //cr->fill_preserve();
  //cr->set_source_rgba(0.0,0.0,0.0,0.2);
  //cr->set_line_width(0.5);
  //cr->stroke();

  char msg[128];
  sprintf(msg,"Generation: %llu", gen);

  font.set_family("Source Sans Pro");
  font.set_size(20*PANGO_SCALE);
  layout->set_font_description(font);

  layout->set_text(msg);
  layout->get_pixel_size(text_width,text_height);
  //if( logo_mid == south )
  //  logo_mid = south - text_height/2;
  cr->move_to(west-text_width-0.025*(west-east),south-text_height-0.025*(south-north));
  layout->add_to_cairo_context(cr);
  cr->set_source_rgba(1.0,1.0,1.0,0.9);
  cr->fill();
  // NOTE: If you want to add an outline, uncomment these lines and comment the line above.
  //cr->fill_preserve();
  //cr->set_source_rgba(0.0,0.0,0.0,0.2);
  //cr->set_line_width(1.0);
  //cr->stroke();

  Glib::Threads::Mutex::Lock lock{worker_.mutex_};
  worker_.swap_buffers();
  worker_.sync_.signal();
  return true;
}

bool SimCHCG::on_timeout() {
    // force our program to redraw everything
    auto win = get_window();
    if (win) {
        Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                         get_allocation().get_height());
        win->invalidate_rect(r, false);
    }
    return true;
}