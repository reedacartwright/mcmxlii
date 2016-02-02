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

SimCHCG::SimCHCG(int width, int height, double mu, int delay, bool fullscreen) :
    grid_width_{width}, grid_height_{height}, mu_(mu),
    worker_{width,height,mu,delay},
    fullscreen_{fullscreen}
{
    //Glib::signal_timeout().connect(sigc::mem_fun(*this, &SimCHCG::on_timeout), 1000.0/OUR_FRAME_RATE );
    
    Glib::signal_timeout().connect([&]() -> bool {
        this->worker_.signal_next_generation(); return true;
        }, 1000.0/OUR_FRAME_RATE );

    signal_queue_draw().connect(sigc::mem_fun(*this, &SimCHCG::queue_draw));

    add_events(Gdk::POINTER_MOTION_MASK|Gdk::BUTTON_PRESS_MASK);

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
 
    none_cursor_ = Gdk::Cursor::create(p->get_display(), "none");
    cell_cursor_  = Gdk::Cursor::create(p->get_display(), "cell");
    p->set_cursor(none_cursor_);
 
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
    const char *app = "SimHCG";
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

void SimCHCG::on_size_allocate(Gtk::Allocation& allocation) {
    Gtk::DrawingArea::on_size_allocate(allocation);

    device_width_ = allocation.get_width();
    device_height_ = allocation.get_height();

    double w = 1.0*device_width_/grid_width_;
    double h = 1.0*device_height_/grid_height_;

    if(w <= h) {
        // width is the limiting space
        double size = 1.0*grid_height_*device_width_/grid_width_;
        cairo_xoffset_ = 0;
        cairo_yoffset_ = (device_height_-size)/2;
        cairo_scale_ = w; 
    } else {
        // height is the limiting space
        double size = 1.0*grid_width_*device_height_/grid_height_;
        cairo_xoffset_ = (device_width_-size)/2;
        cairo_yoffset_ = 0;
        cairo_scale_ = h;
    }
}

bool SimCHCG::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    //boost::timer::auto_cpu_timer measure_speed(std::cerr,  "on_draw: " "%ws wall, %us user + %ss system = %ts CPU (%p%)\n");

    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->save();
    cr->translate(cairo_xoffset_,cairo_yoffset_);
    cr->scale(cairo_scale_,cairo_scale_);
    cr->set_source_rgba(0.0,0.0,0.0,1.0);
    cr->paint();

    auto data = worker_.get_data();

    for(int y=0;y<grid_height_;++y) {
        for(int x=0;x<grid_width_;++x) {
            int a = static_cast<int>(data.first[x+y*grid_width_].type & 0xFF);
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
    sprintf(msg,"Generation: %llu", data.second);

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

    return true;
}

bool SimCHCG::on_button_press_event(GdkEventButton* button_event) {
    auto xy = device_to_cell(button_event->x,button_event->y);
    if(xy.first == -1 || xy.second == -1)
        return false;
    lastx_ = xy.first;
    lasty_ = xy.second;
    //std::cerr << "Button Pressed on cell " << lastx_ << "x" << lasty_ << "\n";
    if(button_event->button != 1) {
        return false;
    }

    worker_.toggle_cell(lastx_,lasty_);
    return false;
}

// http://stackoverflow.com/a/4609795
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

bool SimCHCG::on_motion_notify_event(GdkEventMotion* motion_event) {
    auto xy = device_to_cell(motion_event->x,motion_event->y);
    if(xy.first == -1 || xy.second == -1)
        return false;    
    //std::cerr << "Pointer Moved to cell " << xy.first << "x" << xy.second << "\n";
    cursor_timeout_.disconnect();
    if(gdk_device_get_source(motion_event->device) != GDK_SOURCE_TOUCHSCREEN) {
        get_window()->set_cursor(cell_cursor_);
        cursor_timeout_ = Glib::signal_timeout().connect([&]() -> bool {
            this->get_window()->set_cursor(this->none_cursor_);
            return false;
        }, 500);
    }
    if(!(motion_event->state & GDK_BUTTON1_MASK)) {
        lastx_ = xy.first;
        lasty_ = xy.second;
        return false;
    }
    int dx = xy.first - lastx_;
    int dy = xy.second - lasty_;
    if(dx == 0 && dy == 0) {
        return false;
    }
    if(abs(dx) > abs(dy)) {
        int o = sgn(dx);
        for(int d=o; d != dx; d += o) {
            int x = lastx_+d;
            int y = lasty_+(d*dy)/dx;
            worker_.toggle_cell(x,y);
        }
    } else {
        int o = sgn(dy);
        for(int d=o; d != dy; d += o) {
            int y = lasty_+d;
            int x = lastx_+(d*dx)/dy;
            worker_.toggle_cell(x,y);
        }
    }
    lastx_ = xy.first;
    lasty_ = xy.second;
    worker_.toggle_cell(lastx_,lasty_);
    return false;
}

std::pair<int,int> SimCHCG::device_to_cell(int x, int y) {
    if(x < 0 || x >= device_width_ || y < 0 || y >= device_height_)
        return {-1,-1};
    return {(x-cairo_xoffset_)/cairo_scale_,(y-cairo_yoffset_)/cairo_scale_};
}
