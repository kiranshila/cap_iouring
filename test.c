#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

const int UDP_PAYLOAD = 8200;
const int NUM_PACKETS = 100000000;

int cmp64(const void *a, const void *b) {
  uint64_t *x = (uint64_t *)a;
  uint64_t *y = (uint64_t *)b;
  return *x - *y;
}

int cmpint(const void *a, const void *b) {
  int *x = (int *)a;
  int *y = (int *)b;
  return *x - *y;
}

int main() {
  int fd = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in udp_sock;
  udp_sock.sin_family = AF_INET;
  udp_sock.sin_port = htons(60000);
  udp_sock.sin_addr.s_addr = inet_addr("0.0.0.0");
  if (bind(fd, (struct sockaddr *)&udp_sock, sizeof(udp_sock)) == -1) {
    printf("Failed to bind");
    return -1;
  }

  size_t rcv_bytes = 0;
  uint8_t buf[8200];
  uint64_t packets = 0;

  // Allocate counts on heap (too big for stack)
  uint64_t *counts = (uint64_t *)malloc(NUM_PACKETS * sizeof(uint64_t));
  int *deltas = (int *)malloc((NUM_PACKETS - 1) * sizeof(int));

  while (packets < NUM_PACKETS) {
    rcv_bytes = recvfrom(fd, buf, UDP_PAYLOAD, 0, NULL, NULL);
    if (rcv_bytes == UDP_PAYLOAD) {
      // Process packet
      counts[packets] = __builtin_bswap64(*(uint64_t *)buf);
      packets += 1;
    } else if (rcv_bytes == -1) {
      // Check if WOULD_BLOCK (EAGAIN on *nix)
      if (errno != EAGAIN) {
        return -1;
      }
    }
  }

  // Sort counts
  qsort(counts, NUM_PACKETS, sizeof(uint64_t), cmp64);

  // And compute deltas
  for (int i = 0; i < NUM_PACKETS - 1; i++) {
    deltas[i] = (int)(counts[i + 1] - counts[i]);
  }

  // And sort the deltas
  qsort(deltas, NUM_PACKETS - 1, sizeof(int), cmpint);

  // Then print deltas
  int last_delta = 0;
  int last_delta_cnt = 0;
  for (int i = 0; i < NUM_PACKETS - 1; i++) {
    int d = deltas[i];
    if (d != last_delta) {
      printf("%d,%d\n", last_delta, last_delta_cnt);
      last_delta = d;
      last_delta_cnt = 1;
    } else {
      last_delta_cnt += 1;
    }
  }

  // not that this matters, the OS will do it for us
  free(counts);
}