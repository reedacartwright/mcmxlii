#include "worker.h"
#include "simchcg.h"
#include "rexp.h"

#include <glibmm/timer.h>
#include <iostream>
#include <cassert>

Worker::Worker(int width, int height, double mu,int delay) :
  width_{width}, height_{height}, mu_{mu},
  pop_a_{new pop_t(width*height)},
  pop_b_{new pop_t(width*height)},
  rand{create_random_seed()},
  delay_{delay}
{

}

void Worker::stop() {
    go_ = false;
    do_next_generation();
}

const double mutation[128] = {
  2.0, 1.5, 1.2, 1.2, 1.2, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 0.999, 0.999, 0.99, 0.99, 0.99,
  0.95, 0.95, 0.95, 0.9, 0.9, 0.9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
};

void Worker::do_work(SimCHCG* caller)
{
    static_assert(num_alleles < 256, "Too many colors.");
    go_ = true;
    next_generation_ = false;
    gen_ = 0;
    sleep(delay_);

    while(go_) {
        Glib::Threads::RWLock::ReaderLock lock{data_lock_};

        //boost::timer::auto_cpu_timer measure_speed(std::cerr,  "do_work: " "%ws wall, %us user + %ss system = %ts CPU (%p%)\n");
        const pop_t &a = *pop_a_.get();
        pop_t &b = *pop_b_.get();
        b = a;
        int m = static_cast<int>(rand_exp(rand,mu_));
        for(int y=0;y<height_;++y) {
            for(int x=0;x<width_;++x) {
                int pos = x+y*width_;
                if(a[pos].is_null()) {
                    continue; // cell is null
                }

                double w, weight = rand_exp(rand, a[pos].fitness);
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
                if(m > 0) {
                    m -= 1;
                    continue;
                }
                m = static_cast<int>(rand_exp(rand,mu_));
                static_assert(sizeof(mutation)/sizeof(double) == 128, "number of possible mutations is not 128");
                uint64_t r = rand.get_uint64();
                b[pos].fitness *= mutation[r >> 57]; // use top 7 bits for phenotype
                r &= 0x01FFFFFFFFFFFFFF;
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
                    uint64_t color = aa.type & 0xFF;
                    aa.fitness = aa.fitness/m; // + (DBL_EPSILON/2.0);
                    aa.type = (aa.type & 0xFFFFFFFFFFFFFF00) | color;
                }
            }
        }
        lock.release();
        swap_buffers();

        caller->notify_queue_draw();
        Glib::Threads::Mutex::Lock slock{sync_mutex_};
        while(!next_generation_) {
            sync_.wait(sync_mutex_);
        }
        next_generation_ = false;
    }
}

std::pair<pop_t,unsigned long long> Worker::get_data() {
    Glib::Threads::RWLock::ReaderLock lock{data_lock_};
    return {*pop_a_.get(),gen_};
}

void Worker::swap_buffers() {
    Glib::Threads::RWLock::WriterLock lock{data_lock_};
    gen_ += 1;
    std::swap(pop_a_,pop_b_);
    apply_toggles();
    char buf[128];
    std::sprintf(buf, "%0.2fs: Generation %llu done.\n", timer_.elapsed(),gen_);
    std::cout << buf;
    std::cout.flush();
}

void Worker::do_next_generation() {
    Glib::Threads::Mutex::Lock lock{sync_mutex_};
    next_generation_ = true;
    sync_.signal();
}

void Worker::do_clear_nulls() {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    clear_all_nulls_ = true;
}

void Worker::toggle_cell(int x, int y, bool on) {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    assert(0 <= x < width_ && 0 <= y < height_);
    toggle_map_[{x,y}] = on;
}

const std::pair<int,int> erase_area_[] = {
    {-1,0},{0,-1},{1,0},{0,1},
    {-1,-1},{1,-1},{1,1},{-1,1}
};

void Worker::apply_toggles() {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    pop_t &a = *pop_a_.get();

    if(clear_all_nulls_) {
        toggle_map_.clear();
        for(auto && pos : null_cells_) {
            int x = pos.first;
            int y = pos.second;
            a[x+y*width_].toggle_off();
        }
        null_cells_.clear();
        clear_all_nulls_ = false;
        return;
    }
    for(auto && cell : toggle_map_) {
    	int x = cell.first.first;
    	int y = cell.first.second;
    	assert(0 <= x && x < width_ && 0 <= y && y < height_);
    	if(cell.second) {
            null_cells_.insert(cell.first);
    		a[x+y*width_].toggle_on();
    	} else if(null_cells_.erase(cell.first) > 0) {
    		a[x+y*width_].toggle_off();
    	} else {
            for(auto && off : erase_area_) {
                if(null_cells_.erase({cell.first.first+off.first,cell.first.second+off.second}) > 0) {
                    a[(x+off.first)+(y+off.second)*width_].toggle_off();
                    break;
                }
            }
        }
    }
    toggle_map_.clear();
}
