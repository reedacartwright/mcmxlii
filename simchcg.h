#ifndef CARTWRIGHT_SIMCHCG_H
#define CARTWRIGHT_SIMCHCG_H

#include <gtkmm/drawingarea.h>

#include "worker.h"

class SimCHCG : public Gtk::DrawingArea
{
public:
  SimCHCG();
  virtual ~SimCHCG();

protected:
  //Override default signal handler:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

  bool on_timeout();

  int grid_width;
  int grid_height;

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
  {0.650980392156863, 0.807843137254902, 0.890196078431372, 1},
  {0.12156862745098, 0.470588235294118, 0.705882352941177, 1},
  {0.698039215686274, 0.874509803921569, 0.541176470588235, 1},
  {0.2, 0.627450980392157, 0.172549019607843, 1},
  {0.984313725490196, 0.603921568627451, 0.6, 1},
  {0.890196078431372, 0.101960784313725, 0.109803921568627, 1},
  {0.992156862745098, 0.749019607843137, 0.435294117647059, 1},
  {1, 0.498039215686275, 0, 1},
  {0.792156862745098, 0.698039215686274, 0.83921568627451, 1},
  {0.415686274509804, 0.23921568627451, 0.603921568627451, 1},
  {1, 1, 0.6, 1},
  {0.694117647058824, 0.349019607843137, 0.156862745098039, 1}
};
constexpr size_t num_alleles = sizeof(col_set)/sizeof(color_rgb);

#endif // GTKMM_EXAMPLE_CLOCK_H