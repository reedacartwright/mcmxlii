#include "worker.h"
#include "sim1942.h"
#include "rexp.h"

#include <glibmm/timer.h>
#include <iostream>
#include <cassert>
#include <array>

Worker::Worker(int width, int height, double mu,int delay) :
  grid_width_{width}, grid_height_{height}, mu_{mu},
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

void Worker::do_work(Sim1942* caller)
{
    static_assert(num_alleles < 256, "Too many colors.");
    go_ = true;
    next_generation_ = false;
    gen_ = 0;
    sleep(delay_);

    std::array<int,num_colors> color_count; 
    std::vector<int> empty_colors(num_colors);

    while(go_) {
        Glib::Threads::RWLock::ReaderLock lock{data_lock_};
        //boost::timer::auto_cpu_timer measure_speed(std::cerr,  "do_work: " "%ws wall, %us user + %ss system = %ts CPU (%p%)\n");
        
        const pop_t &a = *pop_a_.get();
        pop_t &b = *pop_b_.get();
        b = a;
        color_count.fill(0);
        for(int y=0;y<grid_height_;++y) {
            for(int x=0;x<grid_width_;++x) {
                int pos = x+y*grid_width_;
                if(a[pos].is_null()) {
                    continue; // cell is null
                }

                double w;
                double weight = a[pos].is_fertile() ? rand_exp(rand, a[pos].fitness) : INFINITY;
                int pos2 = (x-1)+y*grid_width_;
                if(x > 0 && a[pos2].is_fertile() && (w = rand_exp(rand, a[pos2].fitness)) < weight ) {
                    weight = w;
                    b[pos] = a[pos2];
                }
                pos2 = x+(y-1)*grid_width_;
                if(y > 0 && a[pos2].is_fertile() && (w = rand_exp(rand, a[pos2].fitness)) < weight ) {
                    weight = w;
                    b[pos] = a[pos2];
                }
                pos2 = (x+1)+y*grid_width_;
                if(x < grid_width_-1 && a[pos2].is_fertile() && (w = rand_exp(rand, a[pos2].fitness)) < weight ) {
                    weight = w;
                    b[pos] = a[pos2];
                }
                pos2 = x+(y+1)*grid_width_;
                if(y < grid_height_-1 && a[pos2].is_fertile() && (w = rand_exp(rand, a[pos2].fitness)) < weight ) {
                    weight = w;
                    b[pos] = a[pos2];
                }
                color_count[b[pos].color()] += 1;
            }
        }
        // Do Mutation
        int pos  = static_cast<int>(floor(rand_exp(rand,mu_)));
        if(pos < grid_width_*grid_height_) {
            // Setup colors since will will have to do at least one mutation
            empty_colors.clear();
            for(int color = 0; color < num_alleles; ++color) {
                if(color_count[color] == 0) 
                    empty_colors.emplace_back(color);
            }
        }
        while(pos < grid_width_*grid_height_) {
            // save pos
            int opos = pos;
            pos += static_cast<int>(floor(rand_exp(rand,mu_)));
            if(!b[opos].is_fertile())
                continue;
            // mutate
            uint64_t r = rand.get_uint64();
            static_assert(sizeof(mutation)/sizeof(double) == 128, "number of possible mutations is not 128");
            b[opos].fitness *= mutation[r >> 57]; // use top 7 bits for phenotype
            r &= 0x01FFFFFFFFFFFFFF;
            uint64_t color;
            if(empty_colors.empty()) {
                // Get the color of the parent
                color = b[opos].color();
                // Mutate color so that it does not match the parent
                color = (color + r % (num_alleles-1)) % num_alleles;
            } else {
                // retrieve random empty color and erase it
                int col = r % empty_colors.size();
                color = empty_colors[col];
                if(pos < grid_width_*grid_height_)
                    empty_colors.erase(empty_colors.begin()+col);                
            }
            // Store the allele color in the bottom 8 bits.
            b[opos].type = (b[opos].type & CELL_FITNESS_MASK) | color;            
        }

        // Every so often rescale fitnesses to prevent underflow/overflow
        if((1+gen_) % 10000 == 0) {
            auto it = std::max_element(b.begin(),b.end());
            double m = it->fitness;
            if(m > 1e6) {
                for(auto &aa : b) {
                    uint64_t color = aa.color();
                    aa.fitness = aa.fitness/m + (DBL_EPSILON/2.0);
                    aa.type = (aa.type & CELL_FITNESS_MASK) | color;
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

bool Worker::is_cell_valid(int x, int y) const {
    return (0 <= x && x < grid_width_ && 0 <= y && y < grid_height_); 
}

void Worker::toggle_cell(int x, int y, bool on) {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    if(is_cell_valid(x,y))
        toggle_map_[{x,y}] = on;
}

// http://stackoverflow.com/a/4609795
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

// toggle a line beginning at {x1,y1} and ending at {x2,y2}
// assumes that {x1,y1} has already been toggled
void Worker::toggle_line(int x1, int y1, int x2, int y2, bool on) {
    Glib::Threads::Mutex::Lock lock{toggle_mutex_};
    int dx = x2 - x1;
    int dy = y2 - y1;
    if(dx == 0 && dy == 0) {
        return;
    }
    if(abs(dx) > abs(dy)) {
        int o = sgn(dx);
        for(int d=o; d != dx; d += o) {
            int nx = x1+d;
            int ny = y1+(d*dy)/dx;
            if(is_cell_valid(nx,ny))
                toggle_map_[{nx,ny}] = on;
        }
    } else {
        int o = sgn(dy);
        for(int d=o; d != dy; d += o) {
            int ny = y1+d;
            int nx = x1+(d*dx)/dy;
            if(is_cell_valid(nx,ny))
                toggle_map_[{nx,ny}] = on;
        }
    }
    if(is_cell_valid(x2,y2))
        toggle_map_[{x2,y2}] = on;
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
            a[x+y*grid_width_].toggle_off();
        }
        null_cells_.clear();
        clear_all_nulls_ = false;
        return;
    }
    for(auto && cell : toggle_map_) {
    	int x = cell.first.first;
    	int y = cell.first.second;
    	if(cell.second) {
            null_cells_.insert(cell.first);
    		a[x+y*grid_width_].toggle_on();
    	} else if(null_cells_.erase(cell.first) > 0) {
    		a[x+y*grid_width_].toggle_off();
    	} else {
            for(auto && off : erase_area_) {
                if(null_cells_.erase({cell.first.first+off.first,cell.first.second+off.second}) > 0) {
                    a[(x+off.first)+(y+off.second)*grid_width_].toggle_off();
                    break;
                }
            }
        }
    }
    toggle_map_.clear();
}
