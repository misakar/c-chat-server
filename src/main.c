/***
 * c-chat-server
 * `````````````
 *
 * a chat server written in c
 */
#include "uv.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#define SERVER_HOST "0.0.0.0"
#define SERVER_PORT 3000

struct user_s
{
  QUEUE node;
  uv_tcp_t handle;
  char ip[32];
};

void on_connected(uv_stream_t *, int);
void get_user_ip(struct user_s *);
void broadcast(struct user_s*, const char *, ...);
void on_write(uv_write_t *, int);
void unicast(struct user_s *, const char *);
void on_alloc(uv_handle_t *, size_t, uv_buf_t *);
void on_read(uv_stream_t *, ssize_t, const uv_buf_t *);
void on_close(uv_handle_t *);

static QUEUE userq; // 用户队列

int main(int argc, char **argv)
{
  QUEUE_INIT(&userq);
  uv_tcp_t server;
  uv_tcp_init(uv_default_loop(), &server);
  struct sockaddr_in addr;
  uv_ip4_addr(SERVER_HOST, SERVER_PORT, &addr);
  if (uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0))
  {
    fprintf(stderr, "Error => uv_tcp_bind");
    return 1;
  }
  if (uv_listen((uv_stream_t *)&server, 128, on_connected))
  {
    fprintf(stderr, "Error => uv_listen");
    return 1;
  }
  fprintf(stderr, "chat-server listening at <%s:%d>\n", SERVER_HOST, SERVER_PORT);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  return 0;
}

void on_connected(uv_stream_t *server, int status)
{
  assert(status == 0);

  struct user_s *user = malloc(sizeof(struct user_s));
  uv_tcp_init(uv_default_loop(), &user->handle);

  if (uv_accept(server, (uv_stream_t *)&user->handle))
  {
    fprintf(stderr, "Error => uv_accept");
    exit(1);
  }

  QUEUE_INSERT_TAIL(&userq, &user->node);
  get_user_ip(user);

  char sysmsg[1024] = {0};
  int cnt = 0;
  QUEUE *q;
  QUEUE_FOREACH(q, &userq)
    { cnt++; }
  snprintf(sysmsg, sizeof(sysmsg), "\n[online now: %d people]\n\n", cnt);
  unicast(user, sysmsg);
  broadcast(user, "chat-server > %s joined!\n", user->ip);

  uv_read_start((uv_stream_t *)&user->handle, on_alloc, on_read);
}

void on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
  // malloc a buffer
  *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

// This data type is used to represent the sizes of blocks that can be read or written 
// in a single operation. It is similar to size_t, but must be a signed type.
void on_read(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
  struct user_s *user = QUEUE_DATA(handle, struct user_s, handle);
  if (nread < 0)
  {
    // user disconnected
    QUEUE_REMOVE(&user->node);
    broadcast(user, "chat-server > %s left the room!\n", user->ip);
    uv_close((uv_handle_t *)&user->handle, on_close);
    return;
  }

  broadcast(user, "%s > %.*s\n", user->ip, (int)nread, buf->base);
  
  if (buf->base)
  {
    free(buf->base);
  }
}

void on_close(uv_handle_t *handle)
{
  struct user_s *user = QUEUE_DATA(handle, struct user_s, handle);
  free(user);
}

void get_user_ip(struct user_s *user)
{
  char _ip[32] = {0};
  int _namelen = 32;
  int *namelen = &_namelen;
  struct sockaddr name;
  if (uv_tcp_getpeername((const uv_tcp_t *)&user->handle, &name, namelen))
  {
    fprintf(stderr, "Error => uv_tcp_getsockname error");
    exit(1);
  }
  uv_ip4_name((const struct sockaddr_in *)&name, _ip, sizeof(_ip));
  // void *
  // memcpy(void *restrict dst, const void *restrict src, size_t n);
  memcpy(user->ip, _ip, sizeof(_ip));
}

void broadcast(struct user_s *current_user, const char *fmt, ...)
{
  QUEUE *q;
  char msg[512];
  va_list ppap; // 处理不确定参数

  va_start(ppap, fmt); // 字符串拷贝
    vsnprintf(msg, sizeof(msg), fmt, ppap);
  va_end(ppap);

  // 遍历队列中所有的用户发通知
  QUEUE_FOREACH(q, &userq)
  {
    struct user_s *user = QUEUE_DATA(q, struct user_s, node);
    if (strcmp(current_user->ip, user->ip))
    {
      unicast(user, msg);
    }
  }
}

void unicast(struct user_s *user, const char *msg)
{
  size_t len = strlen(msg);
  // 内存布局
  uv_write_t *req = malloc(sizeof(uv_write_t) + len); // req on the heap
  void *buf_base = req + 1; // => uv_write_t, buf_base 也被分配在堆上
  memcpy(buf_base, msg, len);
  uv_buf_t buf = uv_buf_init((char *)buf_base, len); // buf in stack
  // 将缓冲区中的msg写入user tcp socket
  uv_write(req, (uv_stream_t *)&user->handle,
          (const uv_buf_t *)&buf, 1, on_write);
}

void on_write(uv_write_t *req, int status)
{
  free(req); // buf 也会被free
}
