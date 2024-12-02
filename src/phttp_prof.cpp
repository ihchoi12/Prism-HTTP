#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdbool>
#include <vector>
#include <unistd.h>
#include <sys/time.h>
#include <phttp_prof.h>
#include <climits>

bool initialized = false;
FILE *prof_file;
// FILE *export_side;
// FILE *import_side;

uint64_t start_tstamp, end_tstamp;
std::vector<std::vector<int>> prof_vector = {{},{},{},{}};

void
prof_start_tstamp(uint64_t tstamp)
{
#ifdef PHTTP_PROF
  start_tstamp = tstamp;  
#else
  return;
#endif
}

void
prof_end_tstamp(enum prof_cpu_ovhd_ids id, uint64_t tstamp)
{
#ifdef PHTTP_PROF
  end_tstamp = tstamp;
  // printf("hi");
  if (id >= prof_vector.size()) {
    printf("WRONG PROFILING ID: %d\n", id);
  }
  prof_vector[id].push_back(end_tstamp - start_tstamp);
  // printf("%s,%lu\n", prof_cpu_ovhd_id_names[id], end_tstamp - start_tstamp);
#else
  return;
#endif
}
void prof_print_result() {
    size_t last_index = 0;
    for (size_t i = 0; i < prof_vector.size(); i++) {
        printf("%zu\n", prof_vector[i].size());
        if (last_index == 0 || prof_vector[i].size() < last_index) {
            last_index = prof_vector[i].size();
        }
    }

    if(last_index <= 10000){
      printf("need more migrations (now %zu)\n", last_index);
      exit(1);
    }

    // Compute and print average, min, max for each vector
    printf("ID,MIN(ns),MAX(ns),AVG(ns)\n");
    for (size_t j = 0; j < 4; j++) { // 0 to 3 for prof_vector[0] to prof_vector[3]
        long long sum = 0;
        long long min_val = LLONG_MAX;
        long long max_val = LLONG_MIN;

        for (size_t i = last_index - 10000; i < last_index; i++) {
            long long value = prof_vector[j][i];
            sum += value;
            if (value < min_val) min_val = value;
            if (value > max_val) max_val = value;
        }

        double avg = static_cast<double>(sum) / 10000.0;
        printf("PROFCPU_%zu,%lld,%lld,%.2f\n", j, min_val, max_val, avg);
    }

    printf("FINISH\n");
}
void
prof_tstamp(enum prof_types type, enum prof_ids id, uint32_t peer_addr,
            uint16_t peer_port)
{
#ifdef PHTTP_PROF
  FILE *f;
  struct timeval tstamp;

  if (!initialized) {
    char fname[64] = {0};
    sprintf(fname, "/homes/inho/data/prism_prof_%d.csv", getpid());
    prof_file = fopen(fname, "w");
    
    // sprintf(fname, "/homes/inho/data/prism_prof_export_%d.csv", getpid());
    // export_side = fopen(fname, "w");
    // sprintf(fname, "/homes/inho/data/prism_prof_import_%d.csv", getpid());
    // import_side = fopen(fname, "w");
    initialized = true;
  }
  f = prof_file;
  // if (type == PROF_TYPE_EXPORT) {
  //   f = export_side;
  // } else {
  //   f = import_side;
  // }

  gettimeofday(&tstamp, NULL);
  // fprintf(f, "%d,0x%x:%u,%ld\n", id, peer_addr, peer_port,
  //         (tstamp.tv_sec * 1000000 + tstamp.tv_usec)%100000000);
  // fprintf(f, "%d,0x%x:%u,%03d.%06d\n", id, peer_addr, peer_port,
  //         (int)(tstamp.tv_sec % 1000), (int)tstamp.tv_usec);
  fflush(f);
#else
  return;
#endif
}
