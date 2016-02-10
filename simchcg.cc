#include <cmath>
#include <cassert>
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
#define OVERLAY_ALPHA 0.75

const char normal_icons[] = u8"\uf12d   \uf26c";
const char active_eraser_icons[] = u8"<span foreground='#FFF68FE6'>\uf12d</span>   \uf26c";

SimCHCG::SimCHCG(int width, int height, double mu, int delay) :
    grid_width_{width}, grid_height_{height}, mu_(mu),
    worker_{width,height,mu,delay}
{
    //Glib::signal_timeout().connect(sigc::mem_fun(*this, &SimCHCG::on_timeout), 1000.0/OUR_FRAME_RATE );
    
    Glib::signal_timeout().connect([&]() -> bool {
        this->worker_.do_next_generation(); return true;
        }, 1000.0/OUR_FRAME_RATE );

    draw_dispatcher_.connect([&]() {this->queue_draw();});

    add_events(Gdk::POINTER_MOTION_MASK |
        Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK |
        Gdk::KEY_PRESS_MASK|Gdk::TOUCH_MASK);
    set_can_focus();

    signal_touch_event().connect([&](GdkEventTouch* touch_event) -> bool {
    	return this->on_touch_event(touch_event);
    });

    logo_ = Gdk::Pixbuf::create_from_inline(-1,logo_inline,false);

	font_name_.set_weight(Pango::WEIGHT_BOLD);
    font_name_.set_family("TeX Gyre Adventor");
    font_name_.set_size(48*PANGO_SCALE);

	font_note_.set_weight(Pango::WEIGHT_BOLD);
    font_note_.set_family("Source Sans Pro");
    font_note_.set_size(20*PANGO_SCALE);

	font_icon_.set_weight(Pango::WEIGHT_BOLD);
    font_icon_.set_family("Font Awesome");
    font_icon_.set_size(28*PANGO_SCALE);

    // auto gesture = Gtk::GestureSwipe::create(*this);
    // gesture->signal_swipe().connect([&](double vx, double vy) {
    //     std::cerr << "    Hello World    \n";
    // });

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

    cairo_scale_ = std::min(w,h);

    width_ = 1.0*grid_width_*cairo_scale_;
    height_ = 1.0*grid_height_*cairo_scale_;
    west_ = (device_width_-width_)/2.0;
    north_ = (device_height_-height_)/2.0;
    east_ = west_+width_;
    south_ = north_ + height_;

    // Logo Position
    assert(logo_);
    int logo_top = south_, logo_mid = south_;
    logo_top = south_-logo_->get_height()-0.025*(height_);
    pos_logo_ = { west_+0.025*(width_), logo_top };

    // Text
    int text_width, text_height;
    
    // Name
    assert(layout_name_);
    layout_name_->get_pixel_size(text_width,text_height);
    pos_name_ = {(west_+east_)/2.0-text_width/2.0, north_+(logo_top-north_)/2.0-text_height/2.0};

    // Icons
    update_icon_bar_position();
}

void SimCHCG::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    Gtk::DrawingArea::on_screen_changed(previous_screen);
    SimCHCG::create_our_pango_layouts();
}

bool SimCHCG::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    //boost::timer::auto_cpu_timer measure_speed(std::cerr,  "on_draw: " "%ws wall, %us user + %ss system = %ts CPU (%p%)\n");

    auto data = worker_.get_data();
    
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->save();
    cr->translate(west_,north_);
    cr->scale(cairo_scale_,cairo_scale_);
    cr->set_source_rgba(0.0,0.0,0.0,1.0);
    cr->paint();

    for(int y=0;y<grid_height_;++y) {
        for(int x=0;x<grid_width_;++x) {
            int a = static_cast<int>(data.first[x+y*grid_width_].color());
            assert(a < num_colors);
            cr->set_source_rgba(
                col_set[a].red, col_set[a].blue,
                col_set[a].green, col_set[a].alpha
            );
            cr->rectangle(x,y,1.0,1.0);
            cr->fill();
        }
    }
    cr->restore();

    cr->set_antialias(Cairo::ANTIALIAS_GRAY);
    Gdk::Cairo::set_source_pixbuf(cr, logo_, pos_logo_.first, pos_logo_.second);
    cr->paint_with_alpha(OVERLAY_ALPHA);

    cr->set_source_rgba(1.0,1.0,1.0,OVERLAY_ALPHA);
    cr->move_to(pos_name_.first, pos_name_.second);
    layout_name_->show_in_cairo_context(cr);

    if(has_nulls_) {
        cr->move_to(pos_icon_.first, pos_icon_.second);
        layout_icon_->show_in_cairo_context(cr);
    }

    int text_x, text_y, text_width, text_height;
    char msg[128];
    sprintf(msg,"Generation: %llu", data.second);
    layout_note_->set_text(msg);
    layout_note_->get_pixel_size(text_width,text_height);
    cr->move_to(east_-text_width-0.025*width_,south_-text_height-0.025*height_);
    layout_note_->show_in_cairo_context(cr);

    return true;
}

bool SimCHCG::on_key_press_event(GdkEventKey* key_event) {
    if(key_event->keyval == GDK_KEY_F5) {
        clear_clicked();
        return true;
    } else if(key_event->keyval == GDK_KEY_space && has_nulls_) {
        eraser_clicked();
        return true;
    }
    return false;
}

bool SimCHCG::on_button_press_event(GdkEventButton* button_event) {
    if(button_event->button != 1) {
        return false;
    }
    update_cursor_timeout();
    int x = button_event->x;
    int y = button_event->y;
    if(has_nulls_ && box_icon_->contains_point(x,y)) {
        int index, trailing;
        int xx = (x-pos_icon_.first)*PANGO_SCALE;
        int yy = (y-pos_icon_.second)*PANGO_SCALE;
        if(layout_icon_->xy_to_index(xx, yy, index,trailing)) {
            switch(index) {
                case 0: // eraser
                case 1:
                    eraser_clicked();
                    break;
                case 2: // space
                case 3:
                case 4:
                    goto no_icon;
                case 5: // clear screen
                case 6:
                    clear_clicked();
                    break;
                default:
                    goto no_icon;
            };
        }
        lastx_ = -1;
        lasty_ = -1;
        return true;
    }
no_icon:
    if(!device_to_cell(&x,&y)) {
        lastx_ = -1;
        lasty_ = -1;
        return true;
    }

    has_nulls_ = true;
    worker_.toggle_cell(x,y,!erasing_);
    lastx_ = x;
    lasty_ = y;

    return true;
}

bool SimCHCG::on_touch_event(GdkEventTouch* touch_event) {
	//std::cerr << "Hello World\n";
	return false;
}


// http://stackoverflow.com/a/4609795
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

void SimCHCG::update_cursor_timeout() {
    cursor_timeout_.disconnect();
    get_window()->set_cursor(cell_cursor_);
    cursor_timeout_ = Glib::signal_timeout().connect([&]() -> bool {
        this->get_window()->set_cursor(this->none_cursor_);
        return false;
    }, 500);
}

bool SimCHCG::on_motion_notify_event(GdkEventMotion* motion_event) {
    //std::cerr << "Pointer Moved to cell " << xy.first << "x" << xy.second << "\n";
    if(gdk_device_get_source(motion_event->device) != GDK_SOURCE_TOUCHSCREEN) {
        update_cursor_timeout();
    }
    int x = motion_event->x;
    int y = motion_event->y;
    if(!device_to_cell(&x,&y) || !(motion_event->state & GDK_BUTTON1_MASK)) {
        lastx_ = -1;
        lasty_ = -1;        
        return true;
    }
    if(0 <= lastx_ && lastx_ < width_ && 0 <= lasty_ && lasty_ < height_) {
        int dx = x - lastx_;
        int dy = y - lasty_;
        if(dx == 0 && dy == 0) {
            return true;
        }
        if(abs(dx) > abs(dy)) {
            int o = sgn(dx);
            for(int d=o; d != dx; d += o) {
                int nx = lastx_+d;
                int ny = lasty_+(d*dy)/dx;
                worker_.toggle_cell(nx,ny,!erasing_);
            }
        } else {
            int o = sgn(dy);
            for(int d=o; d != dy; d += o) {
                int ny = lasty_+d;
                int nx = lastx_+(d*dx)/dy;
                worker_.toggle_cell(nx,ny,!erasing_);
            }
        }
    }
    lastx_ = x;
    lasty_ = y;
    worker_.toggle_cell(x,y,!erasing_);
    return true;
}

bool SimCHCG::device_to_cell(int *x, int *y) {
    assert(x != nullptr && y != nullptr);
    int xx = *x;
    int yy = *y;
    if(!(west_ <= xx && xx < east_ && north_ <= yy && yy < south_ )) {
    	return false;
    }
    xx = (xx-west_)/cairo_scale_;
    yy = (yy-north_)/cairo_scale_;
    *x = xx;
    *y = yy;
    return true;
}

void SimCHCG::eraser_clicked() {
    erasing_ = !erasing_;
    if(erasing_) {
        set_icon_bar_markup(active_eraser_icons);
    } else {
        set_icon_bar_markup(normal_icons);
    }
}

void SimCHCG::clear_clicked() {
    if(erasing_) {
        eraser_clicked();
    }
    has_nulls_ = false;
    worker_.do_clear_nulls();
}

void SimCHCG::set_icon_bar_markup(const char *ss) {
    assert(ss != nullptr);
    assert(layout_icon_);
    layout_icon_->set_markup(ss);
    update_icon_bar_position();
}

void SimCHCG::update_icon_bar_position() {
    assert(layout_icon_);

    int text_width, text_height;
    layout_icon_->get_pixel_size(text_width,text_height);
    pos_icon_ = {east_-text_width-0.025*width_, north_+0.025*height_};

    box_icon_ = Cairo::Region::create({
        static_cast<int>(pos_icon_.first),
        static_cast<int>(pos_icon_.second),
        text_width, text_height});
}

void SimCHCG::notify_queue_draw() {
    draw_dispatcher_.emit();
}


void SimCHCG::create_our_pango_layouts() {
    layout_name_ = create_pango_layout(name_.c_str());
    layout_name_->set_font_description(font_name_);
    layout_name_->set_alignment(Pango::ALIGN_CENTER);

    layout_note_ = create_pango_layout("");
    layout_note_->set_font_description(font_note_);
    layout_note_->set_alignment(Pango::ALIGN_CENTER);

    layout_icon_ = create_pango_layout(normal_icons);
    layout_icon_->set_font_description(font_icon_);
    layout_icon_->set_alignment(Pango::ALIGN_CENTER);
}
