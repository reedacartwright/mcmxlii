#ifndef CARTWRIGHT_SIMCHCG_H
#define CARTWRIGHT_SIMCHCG_H

#include <gtkmm/drawingarea.h>

#include "worker.h"
#include <boost/timer/timer.hpp>

#include <tuple>

class SimCHCG : public Gtk::DrawingArea
{
public:
    SimCHCG(int width, int height, double mu, int delay, bool fullscreen);
    virtual ~SimCHCG();

    void name(const char* n) {
        name_ = n;
    }
    void name_scale(double n) {
        name_scale_ = n;
    }

    typedef sigc::signal<void> signal_queue_draw_t;
    signal_queue_draw_t signal_queue_draw() {
        return signal_queue_draw_;
    }

protected:
    //Override default signal handler:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    virtual void on_realize() override;
    virtual void on_unrealize() override;
    virtual void on_size_allocate(Gtk::Allocation& allocation) override;

    virtual bool on_button_press_event(GdkEventButton* button_event) override;
    virtual bool on_motion_notify_event(GdkEventMotion* motion_event) override;

    int device_width_, device_height_;
    int grid_width_, grid_height_;
    double mu_;
    bool fullscreen_;

    std::string name_{"Human and Comparative Genomics Laboratory"};
    double name_scale_{1.0};

    double cairo_scale_;
    double cairo_xoffset_;
    double cairo_yoffset_;

    std::pair<int,int> device_to_cell(int x, int y);
    int lastx_{-1}, lasty_{-1};

    Glib::RefPtr<Gdk::Pixbuf> logo_;
    Glib::RefPtr<Gdk::Cursor> none_cursor_, cell_cursor_;
    sigc::connection cursor_timeout_;

    Worker worker_;
    Glib::Threads::Thread* worker_thread_{nullptr};

    signal_queue_draw_t signal_queue_draw_;
};

#endif
