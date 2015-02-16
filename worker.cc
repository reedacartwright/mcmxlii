#include "worker.h"
#include "simchcg.h"
#include "rexp.h"

Worker::Worker(int width, int height) :
  lock_{}, go{true}, width_{width}, height_{height},
  pop_a_{new pop_t(width*height)},
  pop_b_{new pop_t(width*height)},
  rand{create_random_seed()}
{

}

const pop_t& Worker::get_data() const
{
  return *pop_a_.get();
}

void Worker::stop() {
  go = false;
}

void Worker::do_work(SimCHCG* caller)
{
  static_assert(num_alleles <= 16, "Too many colors.");
  // Simulate a long calculation.
  go = true;
  while(go) {
    {
      Glib::Threads::RWLock::ReaderLock lock{lock_};
      const pop_t &a = *pop_a_.get();
      pop_t &b = *pop_b_.get();
      b = a;
      for(int y=0;y<height_;++y) {
        for(int x=0;x<width_;++x) {
          double w, weight = rand_exp(rand, a[x+y*width_].fitness);
          if(x > 0 && (w = rand_exp(rand, a[(x-1)+y*width_].fitness)) < weight ) {
            weight = w;
            b[x+y*width_] = a[(x-1)+y*width_];
          }
          if(y > 0 && (w = rand_exp(rand, a[x+(y-1)*width_].fitness)) < weight ) {
            weight = w;
            b[x+y*width_] = a[x+(y-1)*width_];
          }
          if(x < width_-1 && (w = rand_exp(rand, a[(x+1)+y*width_].fitness)) < weight ) {
            weight = w;
            b[x+y*width_] = a[(x+1)+y*width_];
          }
          if(y < height_-1 && (w = rand_exp(rand, a[x+(y+1)*width_].fitness)) < weight ) {
            weight = w;
            b[x+y*width_] = a[x+(y+1)*width_];
          }

          if(rand.get_double52() < 1e-6) {
            b[x+y*width_].fitness *= 2;
            b[x+y*width_].type = (b[x+y*width_].type & 0xFFFFFFFFFFFFFFF0) |
              (((b[x+y*width_].type & 0xF) + rand.get_uint64(num_alleles-1)) % num_alleles);
          }
        }
      }
      //Glib::usleep(100); // microseconds
    }
    {
      Glib::Threads::RWLock::WriterLock lock{lock_};
      std::swap(pop_a_,pop_b_);
    }
  }
}