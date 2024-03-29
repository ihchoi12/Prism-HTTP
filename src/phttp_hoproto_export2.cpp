#include <unistd.h>

#include <tcp_export.h>
#include <tls_export.h>
#include <http_export.h>
#include <phttp_server.h>
#include <phttp_handoff_server.h>
#include <phttp_prof.h>
#include <util.h>

#include <prism_switch/prism_switch_client.h>
#include <chrono>
uint64_t start_time, end_time;

#define PROF(_id)                                                              \
  do {                                                                         \
    prof_tstamp(PROF_TYPE_EXPORT, _id, hcs->peername_cache.peer_addr,          \
                hcs->peername_cache.peer_port);                                \
  } while (0)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

struct handoff_ctx {
  uv_write_t req;
  uv_buf_t buf[2];
  std::string *serialized_data;
  http_client_socket_t *hcs;
};

static void
after_close_tcp_monitor(uv_handle_t *_monitor)
{
  uv_tcp_monitor_t *monitor = (uv_tcp_monitor_t *)_monitor;
  http_client_socket_t *hcs = container_of(monitor, http_client_socket_t, monitor);  // TODO Fix this
  http_client_socket_deinit(hcs);
  free(hcs);
}

static void
handoff_done(uv_write_t *req, int status)
{
  // prof_end_tstamp(PROF_SEND_PROTO_STATES, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  struct handoff_ctx *ctx = (struct handoff_ctx *)req->data;
  http_client_socket_t *hcs = ctx->hcs;

  if (status != 0) {
    uv_perror("handoff_done", status);
  }

  assert(status == 0);

  // PROF(PROF_SEND_PROTO_STATES);
  
  delete ctx->serialized_data;
  free(ctx->buf[0].base);
  free(ctx);

  prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  int evfd;
  uv_fileno((uv_handle_t *)&hcs->monitor, &evfd);
  uv_close((uv_handle_t *)&hcs->monitor, after_close_tcp_monitor);
  close(evfd);
  prof_end_tstamp(PROF_EXPORT_5, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
}

static void
after_real_close(uv_tcp_monitor_t *monitor)
{
  int error;
  bool serialize_ok;
  http_client_socket_t *hcs =
    container_of(monitor, http_client_socket_t, monitor);  // TODO Fix this
  http_server_handoff_data_t *ho_data =
      (http_server_handoff_data_t *)hcs->res.handoff_data;
  prism::HTTPHandoffReq *ho_req = (prism::HTTPHandoffReq *)hcs->export_data;

  // PROF(PROF_TCP_CLOSE);
  
  struct handoff_ctx *ctx = (struct handoff_ctx *)malloc(sizeof(*ctx));
  assert(ctx != NULL);

  ctx->serialized_data = new std::string(ho_req->ByteSizeLong(), '\0');
  // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  serialize_ok = ho_req->SerializeToString(ctx->serialized_data);
  // prof_end_tstamp(PROF_SERIALIZE, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
  assert(serialize_ok);
  // PROF(PROF_SERIALIZE);
  // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  uint32_t padlen = ctx->serialized_data->size() % 8 == 0
                        ? 0
                        : 8 - ctx->serialized_data->size() % 8;

  struct phttp_ho_header *header =
      (struct phttp_ho_header *)malloc(sizeof(*header) + padlen);
  assert(header != NULL);
  header->length = htonl(ctx->serialized_data->size());

  ctx->buf[0].base = (char *)header;
  ctx->buf[0].len = sizeof(*header) + padlen;
  ctx->buf[1].base = const_cast<char *>(ctx->serialized_data->c_str());
  ctx->buf[1].len = ctx->serialized_data->size();
  ctx->hcs = hcs;
  ctx->req.data = ctx;
  DEBUG("phttp_hoproto_export2::uv_write() -- migrate tcp and request to target\n");
  error = uv_write(&ctx->req, (uv_stream_t *)&ho_data->dest, ctx->buf, 2,
                   handoff_done);
  assert(error == 0);

  delete ho_req;
}

static int
export_tcp(int sock, prism::TCPState *tcp_state)
{
  return tcp_export(sock, tcp_state);
}

static int
export_tls(int sock, struct TLSContext *tls, prism::TLSState *tls_state)
{
  int error;
  error = tls_unmake_ktls(tls, sock);
  assert(error == 0);
  return tls_export(tls, tls_state);
}

static int
export_http(struct http_request *http, prism::HTTPReq *http_state)
{
  return http_request_export(http, http_state);
}

static int
export_all(uv_tcp_t *client, prism::HTTPHandoffReq **ho_reqp)
{
  int error, sock;
  http_client_socket_t *hcs = (http_client_socket_t *)client->data;
  prism::HTTPHandoffReq *ho_req = new prism::HTTPHandoffReq();
  prism::TCPState *tcp;
  prism::TLSState *tls;
  prism::HTTPReq *http;

  prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  uv_fileno((uv_handle_t *)client, &sock);
  tcp = new prism::TCPState();
  error = export_tcp(sock, tcp);
  assert(error == 0);
  ho_req->set_allocated_tcp(tcp);
  prof_end_tstamp(PROF_EXPORT_1, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
  // PROF(PROF_EXPORT_TCP);
  // prof_end_tstamp(PROF_EXPORT_TCP, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  

  if (hcs->tls != NULL) {
    tls = new prism::TLSState();
    error = export_tls(sock, hcs->tls, tls);
    assert(error == 0);
    ho_req->set_allocated_tls(tls);
    PROF(PROF_EXPORT_TLS);
  }

  http = new prism::HTTPReq();
  error = export_http(&hcs->req, http);
  assert(error == 0);
  ho_req->set_allocated_http(http);
  // PROF(PROF_EXPORT_HTTP);
  // prof_end_tstamp(PROF_EXPORT_HTTP, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
  *ho_reqp = ho_req;

  return 0;
}

static void
after_close(uv_handle_t *_client)
{
  int error;
  uv_tcp_t *client = (uv_tcp_t *)_client;
  http_client_socket_t *hcs = (http_client_socket_t *)client->data;
  
  prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  error = uv_tcp_monitor_wait_close(&hcs->monitor, after_real_close);
  prof_end_tstamp(PROF_EXPORT_3, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
  assert(error == 0);
  // free(client);
}

static void
after_configure_switch(struct psw_req_base *req, void *data)
{
  int error;
  uv_tcp_t *client = (uv_tcp_t *)data;
  http_client_socket_t *hcs = (http_client_socket_t *)client->data;

  assert(req->status == 0);

  if (req->type == PSW_REQ_ADD) {
    // PROF(PROF_ADD);
  } else {
    // PROF(PROF_LOCK);
    // prof_end_tstamp(PROF_LOCK, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  }

  prism::HTTPHandoffReq *ho_req;
  error = export_all(client, &ho_req);
  assert(error == 0);

  hcs->export_data = ho_req;
  prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  uv_close((uv_handle_t *)client, after_close);
  prof_end_tstamp(PROF_EXPORT_2, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  
}

int
phttp_start_handoff(uv_tcp_t *client)
{
  // prof_start_tstamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  DEBUG("phttp_hoproto_export2.cpp::phttp_start_handoff2\n");
  int error;
  struct global_config *gconf = (struct global_config *)client->loop->data;
  prism_switch_client_t *sw_client = (prism_switch_client_t *)gconf->sw_client;
  http_client_socket_t *hcs = (http_client_socket_t *)client->data;

  PROF(PROF_RECEIVE_HTTP_REQ);
  

  error = uv_read_stop((uv_stream_t *)client);
  assert(error == 0);

  if (hcs->imported) {
    struct psw_lock_req lock_req;
    lock_req.type = PSW_REQ_LOCK;
    lock_req.status = 0;
    lock_req.peer_addr = hcs->peername_cache.peer_addr;
    lock_req.peer_port = hcs->peername_cache.peer_port;
    DEBUG("psw_lock_req setup\n");
    error = prism_switch_client_queue_task(sw_client,
                                           (struct psw_req_base *)&lock_req,
                                           after_configure_switch, client);
  } else {
    struct psw_add_req add_req;
    add_req.type = PSW_REQ_ADD;
    add_req.status = 0;
    add_req.peer_addr = hcs->peername_cache.peer_addr;
    add_req.peer_port = hcs->peername_cache.peer_port;
    add_req.virtual_addr = hcs->server_sock->server_addr;
    add_req.virtual_port = hcs->server_sock->server_port;
    add_req.owner_addr = hcs->server_sock->server_addr;
    add_req.owner_port = hcs->server_sock->server_port;
    memcpy(add_req.owner_mac, hcs->server_sock->server_mac, 6);
    add_req.lock = 1;
    DEBUG("psw_add_req setup\n");
    error = prism_switch_client_queue_task(sw_client,
                                           (struct psw_req_base *)&add_req,
                                           after_configure_switch, client);
    //test
    // DEBUG("call after_configure_switch()\n");
    // after_configure_switch((struct psw_req_base *)&add_req, client);
  }

  return error;
}
