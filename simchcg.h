#ifndef CARTWRIGHT_SIMCHCG_H
#define CARTWRIGHT_SIMCHCG_H

#include <gtkmm/drawingarea.h>

#include "worker.h"

class SimCHCG : public Gtk::DrawingArea
{
public:
  SimCHCG(int width, int height, double mu);
  virtual ~SimCHCG();

  void name(const char* n) {
    name_ = n;
  }
  void name_scale(double n) {
    name_scale_ = n;
  }


protected:
  //Override default signal handler:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  virtual void on_realize();
  virtual void on_unrealize();

  bool on_timeout();

  int grid_width_;
  int grid_height_;
  double mu_;
  std::string name_;
  double name_scale_;

  Glib::RefPtr<Gdk::Pixbuf> logo_;

  Worker worker_;
  Glib::Threads::Thread* worker_thread_;
};

#endif // GTKMM_EXAMPLE_CLOCK_H