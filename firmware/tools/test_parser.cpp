// Test parsera ramek na PC. Czyta strumien bajtow wygenerowany przez
// tools/make_stream.py (zawiera celowe smieci) i sprawdza, czy firmware
// odtwarza dokladnie te wartosci, ktore wyslal Python.
//
// Budowanie i uruchomienie:
//     c++ -std=c++17 -I src -o /tmp/test_parser tools/test_parser.cpp
//     python3 tools/make_stream.py /tmp/stream.bin && /tmp/test_parser /tmp/stream.bin
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "frame_parser.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "uzycie: %s <stream.bin>\n", argv[0]);
    return 2;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) {
    perror("fopen");
    return 2;
  }
  std::vector<uint8_t> data;
  uint8_t chunk[4096];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) data.insert(data.end(), chunk, chunk + n);
  fclose(f);

  // Karmimy nierownymi kawalkami - USB CDC tez nie dzieli po granicach ramek.
  FrameParser parser;
  AcPacket pkt{};
  std::vector<AcPacket> got;
  size_t off = 0;
  unsigned seed = 7;
  while (off < data.size()) {
    seed = seed * 1103515245u + 12345u;
    size_t take = 1 + (seed >> 16) % 700;
    if (off + take > data.size()) take = data.size() - off;
    if (parser.feed(data.data() + off, take, pkt)) got.push_back(pkt);
    off += take;
  }

  printf("bajtow na wejsciu : %zu\n", data.size());
  printf("ramek poprawnych  : %u\n", parser.good());
  printf("resync-ow         : %u\n", parser.bad());

  // Python wypelnia seq kolejno, a speed_kmh = seq * 1.5 (patrz make_stream.py).
  int errors = 0;
  for (size_t i = 0; i < got.size(); i++) {
    const AcPacket& p = got[i];
    if (p.magic != AC_MAGIC || p.version != AC_PROTO_VERSION) errors++;
    if (fabsf(p.speed_kmh - (float)p.seq * 1.5f) > 1e-3f) errors++;
    if (p.gear != (int8_t)(p.seq % 7) - 1) errors++;
    if (fabsf(p.drift_angle - ((float)p.seq - 50.0f)) > 1e-3f) errors++;
  }
  const AcPacket& last = got.empty() ? pkt : got.back();
  printf("auto / tor        : %.23s / %.23s\n", last.car_name, last.track_name);
  printf("ostatni seq       : %u, speed %.1f, bieg %d, drift %.1f\n",
         last.seq, last.speed_kmh, last.gear, last.drift_angle);
  printf("bledow pol        : %d\n", errors);

  if (parser.good() != 200 || errors != 0) {
    printf("\nBLAD: parser zgubil ramki albo przeklamal dane.\n");
    return 1;
  }
  printf("\nOK - firmware czyta dokladnie to, co wyslal Python.\n");
  return 0;
}
