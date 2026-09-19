// Skladanie ramek telemetrii ze strumienia bajtow USB CDC.
//
// Celowo bez zaleznosci od Arduino - dzieki temu ten sam kod da sie
// przetestowac na PC (patrz tools/test_parser.cpp).
#pragma once

#include <string.h>

#include "protocol.h"

class FrameParser {
 public:
  // Wrzuca surowe bajty z portu. Zwraca true, jesli zlozyla sie choc jedna
  // poprawna ramka; `out` dostaje wtedy NAJSWIEZSZA z nich - przy 60 Hz
  // zalegle ramki sa bez wartosci, wiec nie ma sensu ich kolejkowac.
  bool feed(const uint8_t* data, size_t n, AcPacket& out) {
    bool got = false;
    while (n > 0) {
      if (len_ == sizeof(buf_)) dropFront(len_ / 2);  // nie powinno wystapic
      size_t space = sizeof(buf_) - len_;
      size_t take = (n < space) ? n : space;
      memcpy(buf_ + len_, data, take);
      len_ += take;
      data += take;
      n -= take;
      got |= parse(out);
    }
    return got;
  }

  void reset() { len_ = 0; }

  uint32_t good() const { return good_; }
  uint32_t bad() const { return bad_; }  // resynchronizacje: smieci lub zle CRC

 private:
  void dropFront(size_t k) {
    if (k >= len_) {
      len_ = 0;
      return;
    }
    memmove(buf_, buf_ + k, len_ - k);
    len_ -= k;
  }

  bool parse(AcPacket& out) {
    bool got = false;
    for (;;) {
      // 1. Ustaw sie na najblizszym "ACT5".
      size_t i = 0;
      while (i + 4 <= len_ && memcmp(buf_ + i, AC_MAGIC_BYTES, 4) != 0) i++;
      if (i > 0) {
        dropFront(i);  // smieci przed ramka
        bad_++;
      }
      if (len_ < AC_FRAME_SIZE) return got;  // czekamy na reszte

      // 2. CRC rozstrzyga, czy to prawdziwy poczatek ramki, czy przypadkowe
      //    bajty danych, ktore wygladaja jak magic.
      uint16_t want;
      memcpy(&want, buf_ + AC_PACKET_SIZE, sizeof(want));
      if (ac_crc16(buf_, AC_PACKET_SIZE) == want) {
        memcpy(&out, buf_, AC_PACKET_SIZE);
        dropFront(AC_FRAME_SIZE);
        good_++;
        got = true;
      } else {
        dropFront(1);  // falszywy trop - szukaj magic dalej
        bad_++;
      }
    }
  }

  // Miesci 7 ramek - zapas na przycieta klatke, gdy rysowanie potrwa dluzej.
  uint8_t buf_[2048];
  size_t len_ = 0;
  uint32_t good_ = 0;
  uint32_t bad_ = 0;
};
