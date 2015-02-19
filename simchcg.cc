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

  double lwd = 2.0;
  if(w <= h) {
    // width is the limiting space
    double size = 1.0*grid_height_*width/grid_width_;
    lwd /= size;
    cr->translate(0,(height-size)/2);
    cr->scale(width,size);
  } else {
    // height is the limiting space
    double size = 1.0*grid_width_*height/grid_height_;
    lwd /= size;
    cr->translate((width-size)/2,0);
    cr->scale(size,height);
  }
  cr->set_line_width(lwd);
  
  double yl = 1.0/grid_height_;
  double xl = 1.0/grid_width_;

  cr->set_source_rgba(0.0,0.0,0.0,1.0);
  cr->paint();
  pop_t data(grid_width_*grid_height_);

  {
    Glib::Threads::RWLock::ReaderLock lock{worker_.lock_};
    data = worker_.get_data();
  for(int y=0;y<grid_height_;++y) {
    for(int x=0;x<grid_width_;++x) {
      int a = static_cast<int>(data[x+y*grid_width_].type & 0xF);
      cr->set_source_rgba(
        col_set[a].red, col_set[a].blue,
        col_set[a].green, col_set[a].alpha
      );
      cr->rectangle(x*xl,y*yl,xl,yl);
      cr->fill_preserve();
      cr->stroke();
    }
  }
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