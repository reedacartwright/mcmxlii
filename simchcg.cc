#include <cmath>
#include <algorithm>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "simchcg.h"

SimCHCG::SimCHCG() :
  grid_width{400}, grid_height{225},
  worker_{400,225}, worker_thread_{nullptr}
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

bool SimCHCG::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  auto allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();

  double w = 1.0*width/grid_width;
  double h = 1.0*height/grid_height;

  double lwd = 2.0;
  if(w <= h) {
    // width is the limiting space
    double size = 1.0*grid_height*width/grid_width;
    lwd /= size;
    cr->translate(0,(height-size)/2);
    cr->scale(width,size);
  } else {
    // height is the limiting space
    double size = 1.0*grid_width*height/grid_height;
    lwd /= size;
    cr->translate((width-size)/2,0);
    cr->scale(size,height);
  }
  
  double yl = 1.0/grid_height;
  double xl = 1.0/grid_width;

  cr->set_source_rgba(0.0,0.0,0.0,1.0);
  cr->paint();

  {
    Glib::Threads::RWLock::ReaderLock lock{worker_.lock_};
    auto data = worker_.get_data();
    for(int y=0;y<grid_height;++y) {
      for(int x=0;x<grid_width;++x) {
        int a = static_cast<int>(data[x+y*grid_width].type & 0xF);
        cr->set_source_rgba(
          col_set[a].red, col_set[a].blue,
          col_set[a].green, col_set[a].alpha
        );
        cr->rectangle(x*xl,y*yl,xl,yl);
        cr->fill_preserve();
        cr->set_line_width(lwd);
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