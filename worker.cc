#include "worker.h"
#include "simchcg.h"
#include "rexp.h"

#include <glibmm/timer.h>
#include <iostream>
#include <cassert>

Worker::Worker(int width, int height, double mu,int delay) :
  go{true},
  width_{width}, height_{height}, mu_{mu},
  pop_a_{new pop_t(width*height)},
  pop_b_{new pop_t(width*height)},
  rand{create_random_seed()},
  delay_{delay}
{

}

void Worker::stop() {
    go = false;
    do_next_generation();
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
    static_assert(num_alleles < 256, "Too many colors.");
    go = true;
    next_generation = false;
    gen_ = 0;
    sleep(delay_);

    while(go) {
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
        lock.release();

        swap_buffers();
        caller->signal_queue_draw().emit();
        Glib::Threads::Mutex::Lock slock{sync_mutex_};
        while(!next_generation) {
            sync_.wait(sync_mutex_);
        }
        next_generation = false;
    }
}

std::pair<pop_t,unsigned long long> Worker::get_data() const {
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
    next_generation = true;
    sync_.signal();
}

void Worker::do_clear_nulls() {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    clear_all_nulls_ = true;
}

void Worker::toggle_cell(int x, int y) {
    assert(0 <= x < width_ && 0 <= y < height_);
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    std::pair<int,int> p{x,y};
    if(std::find(toggle_list_.rbegin(),toggle_list_.rend(),p) == toggle_list_.rend()) { 
        toggle_list_.emplace_back(p);
    }
}

bool Worker::has_nulls() {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    return has_nulls_;    
}

void Worker::apply_toggles() {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    pop_t &a = *pop_a_.get();

    if(clear_all_nulls_) {
        if(!has_nulls_)
            return;
        for(auto && aa : a) {
            if(aa.is_null()) {
                aa.toggle_off();
            }
        }
        toggle_list_.clear();
        has_nulls_ = clear_all_nulls_ = false;
        return;
    }
    if(!toggle_list_.empty())
        has_nulls_ = true;
    while(!toggle_list_.empty()) {
        auto xy = toggle_list_.front();
        assert(0 <= xy.first < width_ && 0 <= xy.second < height_);
        a[xy.first+xy.second*width_].toggle();
        toggle_list_.pop_front();
    }
}
