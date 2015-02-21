#include <cmath>
#include <algorithm>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include <iostream>

#include "simchcg.h"

#include <gdk/gdkx.h>
#include <dbus/dbus.h>

SimCHCG::SimCHCG(int width, int height, double mu) :
  grid_width_{width}, grid_height_{height}, mu_(mu),
  worker_{width,height,mu}, worker_thread_{nullptr}
{
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &SimCHCG::on_timeout), 1000/30 );

  #ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_draw().connect(sigc::mem_fun(*this, &SimCHCG::on_draw), false);
  #endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

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

  double lwd = 1.0;
  double cell_size = 1.0;
  cr->save();
  if(w <= h) {
    // width is the limiting space
    double size = 1.0*grid_height_*width/grid_width_;
    cr->translate(0,(height-size)/2);
    cr->scale(w,w);
    lwd /= w;
  } else {
    // height is the limiting space
    double size = 1.0*grid_width_*height/grid_height_;
    cr->translate((width-size)/2,0);
    cr->scale(h,h);
    lwd /= h;
  }
  cr->set_line_width(lwd);
  cr->set_source_rgba(0.0,0.0,0.0,1.0);
  cr->paint();
  pop_t data(grid_width_*grid_height_);

  {
    Glib::Threads::RWLock::ReaderLock lock{worker_.lock_};
    data = worker_.get_data();
    unsigned long long gen = worker_.get_gen();
    for(int y=0;y<grid_height_;++y) {
      for(int x=0;x<grid_width_;++x) {
        int a = static_cast<int>(data[x+y*grid_width_].type & 0xF);
        cr->set_source_rgba(
          col_set[a].red, col_set[a].blue,
          col_set[a].green, col_set[a].alpha
        );
        cr->rectangle(x,y,1.0,1.0);
        //cr->stoke_preserve();
        cr->fill();
      }
    }
    double west = grid_width_;
    double south = grid_height_;
    cr->user_to_device(west,south);
    cr->restore();
    cr->set_source_rgba(1.0,1.0,1.0,0.9);
    Pango::FontDescription font;
    font.set_family("Source Sans Pro");
    font.set_weight(Pango::WEIGHT_BOLD);
	font.set_size(20*PANGO_SCALE);

    char msg[128];
    sprintf(msg,"Generation: %llu", gen);
    auto layout = create_pango_layout(msg);
    layout->set_font_description(font);
    int text_width, text_height;
    layout->get_pixel_size(text_width,text_height);
    cr->move_to(west-text_width-10,south-text_height-10);
    layout->show_in_cairo_context(cr);

	font.set_family("TeX Gyre Adventor");
    font.set_size(48*PANGO_SCALE);
    layout->set_font_description(font);
    layout->set_alignment(Pango::ALIGN_CENTER);
    layout->set_text("Center for Human and Comparative Genomics");
    	//"The Biodesign Institute at Arizona State University");
    layout->get_pixel_size(text_width,text_height);  
    cr->move_to(width/2.0-text_width/2.0,height/2.0-text_height/2.0);

    layout->show_in_cairo_context(cr);


  }

  return true;
}


bool SimCHCG::on_timeout()
{
    // force our program to redraw the entire clock.
    auto win = get_window();
    if (win)
    {
        Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                get_allocation().get_height());
        win->invalidate_rect(r, false);
    }
    return true;
}