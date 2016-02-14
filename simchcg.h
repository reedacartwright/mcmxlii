#ifndef CARTWRIGHT_SIMCHCG_H
#define CARTWRIGHT_SIMCHCG_H

#include <gtkmm/drawingarea.h>

#include "worker.h"
#include <boost/timer/timer.hpp>

#include <tuple>

class SimCHCG : public Gtk::DrawingArea
{
public:
    SimCHCG(int width, int height, double mu, int delay);
    virtual ~SimCHCG();

    void name(const char* n) {
        name_ = n;
    }
    void name_scale(double n) {
        font_name_.set_size(n*48*PANGO_SCALE);
    }

    void notify_queue_draw();

protected:
    bool device_to_cell(int *x, int *y);
    bool process_iconbar_click(int x, int y);

    void create_our_pango_layouts();

    void eraser_clicked();
    void clear_clicked();
    void set_iconbar_markup(const char *ss);
    void update_iconbar_position();

    void update_cursor_timeout();

    //Override default signal handler:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    virtual void on_realize() override;
    virtual void on_unrealize() override;
    virtual void on_size_allocate(Gtk::Allocation& allocation) override;
    virtual void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) override;

    virtual bool on_button_press_event(GdkEventButton* button_event) override;
    virtual bool on_motion_notify_event(GdkEventMotion* motion_event) override;
    virtual bool on_key_press_event(GdkEventKey* key_event) override;
    bool on_touch_event(GdkEventTouch* touch_event);

    //virtual bool on_event(GdkEvent* event) override;

    int device_width_, device_height_;
    int grid_width_, grid_height_;
    double mu_;

    std::string name_{"Human and Comparative Genomics Laboratory"};

    double cairo_scale_;

    double draw_width_, draw_height_;
    double east_, north_, west_, south_;

 
    Glib::RefPtr<Gdk::Pixbuf> logo_;
    Glib::RefPtr<Gdk::Cursor> none_cursor_, cell_cursor_;
    sigc::connection cursor_timeout_;

	Pango::FontDescription font_name_, font_note_, font_icon_;
    std::pair<double,double> pos_name_, pos_note_, pos_icon_, pos_logo_;

    Glib::RefPtr<Pango::Layout> layout_name_, layout_note_, layout_icon_;

    Cairo::RefPtr<Cairo::Region> box_iconbar_;

    bool erasing_{false}, show_iconbar_{false};

    Worker worker_;
    Glib::Threads::Thread* worker_thread_{nullptr};

    Glib::Dispatcher draw_dispatcher_;

    typedef GdkEventSequence* gdk_event_sequence_t;
    typedef std::map<gdk_event_sequence_t,std::pair<int,int>> touch_lastxy_t;  

    std::pair<int,int> pointer_lastxy_{-1,-1};
    touch_lastxy_t touch_lastxy_;


};

#endif
