#pragma once

#include <stdint.h>
#include <vector>
enum prof_types { PROF_TYPE_EXPORT, PROF_TYPE_IMPORT };

enum prof_ids {
  /* Export side */
  PROF_RECEIVE_HTTP_REQ,  // Received HTTP Request
  PROF_LOCK,              // Received switch's reply for LOCK request
  PROF_ADD,               // Received switch's reply for ADD request
  PROF_EXPORT_TCP,        // export: convert the TCP state into a standardized format, and put it into a handoff request
  PROF_EXPORT_TLS,
  PROF_EXPORT_HTTP,
  PROF_TCP_CLOSE,
  PROF_SERIALIZE,         // serialize the handoff request (to send it over network)
  PROF_SEND_PROTO_STATES,

  /* Import side */
  PROF_HANDOFF,
  PROF_IMPORT_HTTP,
  PROF_HANDLE_HTTP_REQ,
  PROF_IMPORT_TCP,
  PROF_IMPORT_TLS,
  PROF_CHOWN,
  PROF_FORWARDING,
  PROF_HTTP_RES
};

enum prof_cpu_ovhd_ids {
  /* Export side */
  PROFCPU_EXPORT_TCP,
  PROFCPU_EXPORT_TLS,
  // PROF_EXPORT_3,
  // PROF_EXPORT_4,
  // PROF_EXPORT_5,
  
  /* Import side */
  PROFCPU_IMPORT_TCP,
  PROFCPU_IMPORT_TLS,
};

static const char* prof_cpu_ovhd_id_names[] = {
   /* Export side */
  "PROFCPU_EXPORT_TCP",
  "PROFCPU_EXPORT_TLS",
  // "PROF_EXPORT_3",
  // "PROF_EXPORT_4",
  // "PROF_EXPORT_5",
  
  /* Import side */
  "PROFCPU_IMPORT_TCP",
  "PROFCPU_IMPORT_TLS",
};


struct prof {
  enum prof_ids id;
  uint32_t peer_addr;
  uint16_t peer_port;
  struct timeval tstamp;
};
void prof_start_tstamp(uint64_t tstamp);
void prof_end_tstamp(enum prof_cpu_ovhd_ids id, uint64_t tstamp);

void prof_tstamp(enum prof_types type, enum prof_ids id, uint32_t peer_addr,
                 uint16_t peer_port);
void prof_print_result();
