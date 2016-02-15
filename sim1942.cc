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

#include "sim1942.h"

#include "logo.inl"

#define OUR_FRAME_RATE 15
#define OVERLAY_ALPHA 0.85

const char normal_icons[] = u8"\uf12d   \uf26c";
const char active_eraser_icons[] = u8"<span foreground='#FFF68FE6'>\uf12d</span>   \uf26c";

Sim1942::Sim1942(int width, int height, double mu, int delay) :
    grid_width_{width}, grid_height_{height}, mu_(mu),
    worker_{width,height,mu,delay}
{
    //Glib::signal_timeout().connect(sigc::mem_fun(*this, &Sim1942::on_timeout), 1000.0/OUR_FRAME_RATE );

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
    }, false);

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

Sim1942::~Sim1942()
{
    worker_.stop();
    worker_thread_->join();
}

void Sim1942::on_realize() {
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
    const char *app = "1942";
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

void Sim1942::on_unrealize() {
    Gtk::DrawingArea::on_unrealize();
}

void Sim1942::on_size_allocate(Gtk::Allocation& allocation) {
    Gtk::DrawingArea::on_size_allocate(allocation);

    device_width_ = allocation.get_width();
    device_height_ = allocation.get_height();

    double w = 1.0*device_width_/grid_width_;
    double h = 1.0*device_height_/grid_height_;

    cairo_scale_ = std::min(w,h);

    draw_width_ = 1.0*grid_width_*cairo_scale_;
    draw_height_ = 1.0*grid_height_*cairo_scale_;
    west_ = (device_width_-draw_width_)/2.0;
    north_ = (device_height_-draw_height_)/2.0;
    east_ = west_+draw_width_;
    south_ = north_ + draw_height_;

    // Logo Position
    assert(logo_);
    int logo_top = south_, logo_mid = south_;
    logo_top = south_-logo_->get_height()-0.025*(draw_height_);
    pos_logo_ = { west_+0.025*(draw_width_), logo_top };

    // Text
    int text_width, text_height;

    // Name
    assert(layout_name_);
    layout_name_->get_pixel_size(text_width,text_height);
    pos_name_ = {(west_+east_)/2.0-text_width/2.0, north_+(logo_top-north_)/2.0-text_height/2.0};

    // Icons
    update_iconbar_position();
}

void Sim1942::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    Gtk::DrawingArea::on_screen_changed(previous_screen);
    Sim1942::create_our_pango_layouts();
}

bool Sim1942::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
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

    if(show_iconbar_) {
        cr->move_to(pos_icon_.first, pos_icon_.second);
        layout_icon_->show_in_cairo_context(cr);
    }

    int text_x, text_y, text_width, text_height;
    char msg[128];
    snprintf(msg, 128, "Generation: %'llu", data.second);
    layout_note_->set_text(msg);
    layout_note_->get_pixel_size(text_width,text_height);
    cr->move_to(east_-text_width-0.025*draw_width_,south_-text_height-0.025*draw_height_);
    layout_note_->show_in_cairo_context(cr);

    return true;
}

// bool Sim1942::on_event(GdkEvent* event) {
//     //std::cerr << "    Event " << event->type << "\n";
//     return false;
// }

bool Sim1942::on_key_press_event(GdkEventKey* key_event) {
    if(key_event->keyval == GDK_KEY_F5 && show_iconbar_) {
        clear_clicked();
        return GDK_EVENT_STOP;
    } else if(key_event->keyval == GDK_KEY_BackSpace && show_iconbar_) {
        eraser_clicked();
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

bool Sim1942::on_touch_event(GdkEventTouch* touch_event) {
    if(!(touch_event->state & GDK_BUTTON1_MASK)) {
        return GDK_EVENT_PROPAGATE;
    }
    int x = touch_event->x;
    int y = touch_event->y;

    device_to_cell(&x,&y);
    //std::cerr << "  Touch: " << touch_event->type << " at " << x << "x" << y << "\n";
    switch(touch_event->type) {
    case GDK_TOUCH_BEGIN: {
        if(process_iconbar_click(touch_event->x,touch_event->y)) {
            return GDK_EVENT_STOP;
        }
        show_iconbar_ = true;
        touch_lastxy_[touch_event->sequence] = {x,y};
        worker_.toggle_cell(x,y,!erasing_);
        }
        break;
    case GDK_TOUCH_UPDATE: {
        auto it = touch_lastxy_.find(touch_event->sequence);
        if(it == touch_lastxy_.end())
            break;
        worker_.toggle_line(it->second.first, it->second.second, x, y, !erasing_);        
        it->second = {x,y};
        }
        break;
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL: {
        auto it = touch_lastxy_.find(touch_event->sequence);
        if(it == touch_lastxy_.end())
            break;
        worker_.toggle_line(it->second.first, it->second.second, x, y, !erasing_);
        touch_lastxy_.erase(it);
        }
        break;
    default:
        return GDK_EVENT_PROPAGATE;
    };

	return GDK_EVENT_STOP;
}

bool Sim1942::on_button_press_event(GdkEventButton* button_event) {
    update_cursor_timeout();
    if(button_event->button != GDK_BUTTON_PRIMARY) {
        return GDK_EVENT_PROPAGATE;
    }
    int x = button_event->x;
    int y = button_event->y;
    bool ret = device_to_cell(&x,&y);    
    pointer_lastxy_ = {x,y};
    if(process_iconbar_click(button_event->x,button_event->y)) {
        return GDK_EVENT_STOP;
    }
    if(ret) {
        show_iconbar_ = true;
        worker_.toggle_cell(x,y,!erasing_);
    }
    return GDK_EVENT_STOP;
}

bool Sim1942::on_motion_notify_event(GdkEventMotion* motion_event) {
    auto d = gdk_event_get_source_device((GdkEvent*)motion_event);
    if(d != nullptr && gdk_device_get_source(d) == GDK_SOURCE_TOUCHSCREEN) {
        return GDK_EVENT_PROPAGATE;
    }
    update_cursor_timeout();
    if(!(motion_event->state & GDK_BUTTON1_MASK)) {
        return GDK_EVENT_PROPAGATE;
    }
    int x = motion_event->x;
    int y = motion_event->y;
    device_to_cell(&x,&y);
    worker_.toggle_line(pointer_lastxy_.first, pointer_lastxy_.second, x, y, !erasing_);
    pointer_lastxy_ = {x,y};
    return GDK_EVENT_STOP;
}

bool Sim1942::process_iconbar_click(int x, int y) {
    if(!(show_iconbar_ && box_iconbar_->contains_point(x,y))) {
        return GDK_EVENT_PROPAGATE;
    }
    int index, trailing;
    int xx = (x-pos_icon_.first)*PANGO_SCALE;
    int yy = (y-pos_icon_.second)*PANGO_SCALE;
    if(layout_icon_->xy_to_index(xx, yy, index,trailing)) {
        switch(index) {
            case 0: // eraser
            case 1:
                eraser_clicked();
                return GDK_EVENT_STOP;
            case 2: // space
            case 3:
            case 4:
                break;
            case 5: // clear screen
            case 6:
                clear_clicked();
                return GDK_EVENT_STOP;
            default:
                break;
        };
    }
    return GDK_EVENT_PROPAGATE;
}


void Sim1942::update_cursor_timeout() {
    cursor_timeout_.disconnect();
    get_window()->set_cursor(cell_cursor_);
    cursor_timeout_ = Glib::signal_timeout().connect([&]() -> bool {
        this->get_window()->set_cursor(this->none_cursor_);
        return false;
    }, 500);
}

bool Sim1942::device_to_cell(int *x, int *y) {
    bool ret = true;
    if( x != nullptr ) {
        *x = (*x-west_)/cairo_scale_;
        ret = ret && 0 <= x < grid_width_;
    }
    if( y != nullptr ) {
        *y = (*y-north_)/cairo_scale_;
        ret = ret && 0 <= y < grid_height_;
    }
    return ret;
}

void Sim1942::eraser_clicked() {
    erasing_ = !erasing_;
    if(erasing_) {
        set_iconbar_markup(active_eraser_icons);
    } else {
        set_iconbar_markup(normal_icons);
    }
}

void Sim1942::clear_clicked() {
    if(erasing_) {
        eraser_clicked();
    }
    show_iconbar_ = false;
    worker_.do_clear_nulls();
}

void Sim1942::set_iconbar_markup(const char *ss) {
    assert(ss != nullptr);
    assert(layout_icon_);
    layout_icon_->set_markup(ss);
    update_iconbar_position();
}

void Sim1942::update_iconbar_position() {
    assert(layout_icon_);

    int text_width, text_height;
    layout_icon_->get_pixel_size(text_width,text_height);
    pos_icon_ = {east_-text_width-0.025*draw_width_, north_+0.025*draw_height_};

    box_iconbar_ = Cairo::Region::create({
        static_cast<int>(pos_icon_.first),
        static_cast<int>(pos_icon_.second),
        text_width, text_height});
}

void Sim1942::notify_queue_draw() {
    draw_dispatcher_.emit();
}

void Sim1942::create_our_pango_layouts() {
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
