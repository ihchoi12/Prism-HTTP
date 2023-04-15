#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdbool>
#include <vector>
#include <unistd.h>
#include <sys/time.h>
#include <phttp_prof.h>

bool initialized = false;
FILE *prof_file;
// FILE *export_side;
// FILE *import_side;

uint64_t start_tstamp, end_tstamp;


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
prof_end_tstamp(enum prof_ids id, uint64_t tstamp)
{
#ifdef PHTTP_PROF
  end_tstamp = tstamp;
  printf("%d: %lu\n", id, end_tstamp - start_tstamp);
#else
  return;
#endif
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
  fprintf(f, "%d,0x%x:%u,%ld\n", id, peer_addr, peer_port,
          (tstamp.tv_sec * 1000000 + tstamp.tv_usec)%100000000);
  // fprintf(f, "%d,0x%x:%u,%03d.%06d\n", id, peer_addr, peer_port,
  //         (int)(tstamp.tv_sec % 1000), (int)tstamp.tv_usec);
  fflush(f);
#else
  return;
#endif
}
