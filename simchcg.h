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

struct color_rgb {
  double red;
  double green;
  double blue;
  double alpha;
};

constexpr color_rgb col_set[] = {
  {0.650980392156863, 0.807843137254902, 0.890196078431372, 1.0},
  {0.12156862745098,  0.470588235294118, 0.705882352941177, 1.0},
  {0.698039215686274, 0.874509803921569, 0.541176470588235, 1.0},
  {0.2,               0.627450980392157, 0.172549019607843, 1.0},
  {0.984313725490196, 0.603921568627451, 0.6,               1.0},
  {0.890196078431372, 0.101960784313725, 0.109803921568627, 1.0},
  {0.992156862745098, 0.749019607843137, 0.435294117647059, 1.0},
  {1.0,               0.498039215686275, 0.0,               1.0},
  {0.792156862745098, 0.698039215686274, 0.83921568627451,  1.0},
  {0.415686274509804, 0.23921568627451,  0.603921568627451, 1.0},
  {1.0,               1.0,               0.6,               1.0},
  {0.694117647058824, 0.349019607843137, 0.156862745098039, 1.0}
};
constexpr size_t num_alleles = sizeof(col_set)/sizeof(color_rgb);

#endif // GTKMM_EXAMPLE_CLOCK_H