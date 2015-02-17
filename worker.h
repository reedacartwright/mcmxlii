#ifndef CARTWRIGHT_WORKER_H
#define CARTWRIGHT_WORKER_H

#include <gtkmm.h>

#include <atomic>
#include <memory>
#include <vector>

#include "xorshift64.h"

class SimCHCG;

union cell {
  cell() : fitness{1.0} {};
  double fitness;
  uint64_t type;
};

typedef std::vector<cell> pop_t;

class Worker
{
public:
  Worker(int width, int height, double mu);

  // Thread function.
  void do_work(SimCHCG* caller);

  const pop_t& get_data() const;

  void stop();

  // Synchronizes access to member data.
  mutable Glib::Threads::RWLock lock_;

private:
 
  // Data used by both GUI thread and worker thread.
  std::atomic<bool> go;

  int width_;
  int height_;
  double mu_;

  std::unique_ptr<pop_t> pop_a_;
  std::unique_ptr<pop_t> pop_b_;

  xorshift64 rand;
};

#endif // GTKMM_EXAMPLEWORKER_H