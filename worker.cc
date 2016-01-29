#include "worker.h"
#include "simchcg.h"
#include "rexp.h"
#include <glibmm/timer.h>

#include <iostream>

Worker::Worker(int width, int height, double mu,int delay) :
  go{true},
  width_{width}, height_{height}, mu_{mu},
  pop_a_{new pop_t(width*height)},
  pop_b_{new pop_t(width*height)},
  rand{create_random_seed()},
  delay_{delay}
{

}

const pop_t& Worker::get_data() const
{
  return *pop_a_.get();
}

void Worker::stop() {
  go = false;
  sync_.signal();
}

const double mutation[128] = {
  2.0, 1.5, 1.2, 1.1, 1.1, 1.1, 1.1, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  0.95, 0.95, 0.9, 0.9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
};

void Worker::do_work(SimCHCG* caller)
{
  static_assert(num_alleles <= 256, "Too many colors.");
  go = true;
  gen_ = 0;
  sleep(delay_);

  while(go) {
    {
        Glib::Threads::Mutex::Lock lock{mutex_};
        swap_buffers();
    }
    //sync_.wait(mutex_);
    //gen_ += 1;
    const pop_t &a = *pop_a_.get();
    pop_t &b = *pop_b_.get();
    b = a;
    int m = static_cast<int>(rand_exp(rand,mu_));
    for(int y=0;y<height_;++y) {
      for(int x=0;x<width_;++x) {
        double w, weight = rand_exp(rand, a[x+y*width_].fitness);
        int pos = x+y*width_;
        if(x > 0 && (w = rand_exp(rand, a[(x-1)+y*width_].fitness)) < weight ) {
          weight = w;
          b[pos] = a[(x-1)+y*width_];
        }
        if(y > 0 && (w = rand_exp(rand, a[x+(y-1)*width_].fitness)) < weight ) {
          weight = w;
          b[pos] = a[x+(y-1)*width_];
        }
        if(x < width_-1 && (w = rand_exp(rand, a[(x+1)+y*width_].fitness)) < weight ) {
          weight = w;
          b[pos] = a[(x+1)+y*width_];
        }
        if(y < height_-1 && (w = rand_exp(rand, a[x+(y+1)*width_].fitness)) < weight ) {
          weight = w;
          b[pos] = a[x+(y+1)*width_];
        }
        if(b[pos].type != a[pos].type || m == 0) {
          //caller->signal_queue_draw_cell().emit(x,y);
        }
        if(m > 0) {
          m -= 1;
          continue;
        }
        m = static_cast<int>(rand_exp(rand,mu_));
        uint64_t r = rand.get_uint64();
        b[pos].fitness *= mutation[r >> 57]; // use top 7 bits for phenotype
        r &= 0x00FFFFFFFFFFFFFF;
        // Get the color of the parent
        uint64_t color = b[pos].type & 0xFF;
        // Mutate color so that it does not match the parent
        color = (color + r % (num_alleles-1)) % num_alleles;
        // Store the allele color in the bottom 8 bits.
        b[pos].type = (b[pos].type & 0xFFFFFFFFFFFFFF00) | color;
      }
    }
    // Every so often rescale fitnesses to prevent underflow/overflow
    if((1+gen_) % 10000 == 0) {
      auto it = std::max_element(b.begin(),b.end());
      double m = it->fitness;
      if(m > 1e6) {
        for(auto &aa : b) {
          uint64_t x = aa.type;
          aa.fitness = aa.fitness/m;
          aa.type = (aa.type & 0xFFFFFFFFFFFFFF00) | (x & 0xFF);
        }
      }
    }
  }
}

void Worker::swap_buffers() {
  std::swap(pop_a_,pop_b_);
  char buf[128];
  std::sprintf(buf, "%0.2fs: Generation %llu done.\n", timer_.elapsed(),gen_);
  std::cout << buf;
  std::cout.flush();  
}