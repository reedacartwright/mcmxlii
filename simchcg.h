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

    bool on_timeout();

    void on_queue_draw_cells();

    int grid_width_;
    int grid_height_;
    double mu_;
    std::string name_{"Human and Comparative Genomics Laboratory"};
    double name_scale_{1.0};

    double cairo_scale_;
    double cairo_xoffset_;
    double cairo_yoffset_;

    Glib::RefPtr<Gdk::Pixbuf> logo_;

    Worker worker_;
    Glib::Threads::Thread* worker_thread_{nullptr};

    signal_queue_draw_t signal_queue_draw_;
};

#endif
