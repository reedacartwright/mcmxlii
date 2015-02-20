#include "worker.h"
#include "simchcg.h"
#include "rexp.h"
#include <glibmm/timer.h>

#include <iostream>

Worker::Worker(int width, int height, double mu) :
  lock_{}, go{true},
  width_{width}, height_{height}, mu_{mu},
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

const double mutation[128] = {
  2.0, 1.1, 1.1, 1.1, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  0.9, 0.9, 0.9, 0.9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
};

void Worker::do_work(SimCHCG* caller)
{
  static_assert(num_alleles <= 16, "Too many colors.");
  // Simulate a long calculation.
  go = true;
  gen_ = 0;
  Glib::Timer timer;
  while(go) {
    {
      Glib::Threads::RWLock::ReaderLock lock{lock_};
      const pop_t &a = *pop_a_.get();
      pop_t &b = *pop_b_.get();
      b = a;
      int m = static_cast<int>(rand_exp(rand,mu_));
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
          if(m > 0) {
            m -= 1;
            continue;
          }
          m = static_cast<int>(rand_exp(rand,mu_));
          uint64_t r = rand.get_uint64();
          b[x+y*width_].fitness *= mutation[r >> 57]; // use top 7 bits for phenotype
          r &= 0x0FFFFFFFFFFFFFFF;
          b[x+y*width_].type = (b[x+y*width_].type & 0xFFFFFFFFFFFFFFF0) |
              (((b[x+y*width_].type & 0xF) + r % (num_alleles-1)) % num_alleles);
        }
      }
      if((1+gen_) % 10000 == 0) {
        auto it = std::max_element(b.begin(),b.end());
        double m = it->fitness;
        if(m > 1e6) {
          for(auto &aa : b) {
            uint64_t x = aa.type;
            aa.fitness = aa.fitness/m;
            aa.type = (aa.type & 0xFFFFFFFFFFFFFFF0) | (x & 0xF);
          }
        }
      }
    }
    {
      Glib::Threads::RWLock::WriterLock lock{lock_};
      gen_ += 1;
      std::swap(pop_a_,pop_b_);
      char buf[128];
      std::sprintf(buf, "%0.2fs: Generation %llu done.\n", timer.elapsed(),gen_);
      std::cout << buf;
      std::cout.flush();
    }
  }
}