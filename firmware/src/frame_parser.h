// Reassembles telemetry frames from the USB CDC byte stream.
//
// Deliberately free of Arduino dependencies, so the very same code can be
// tested on a PC (see tools/test_parser.cpp).
#pragma once

#include <string.h>

#include "protocol.h"

class FrameParser {
 public:
  // Feed raw bytes from the port. Returns true if at least one valid frame
  // was assembled; `out` then holds the FRESHEST one - at 60 Hz a backlog of
  // stale frames is worthless, so there is no point queueing them.
  bool feed(const uint8_t* data, size_t n, AcPacket& out) {
    bool got = false;
    while (n > 0) {
      if (len_ == sizeof(buf_)) dropFront(len_ / 2);  // should never happen
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
  uint32_t bad() const { return bad_; }  // resyncs: garbage or a bad CRC

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
      // 1. Line up on the nearest "ACT5".
      size_t i = 0;
      while (i + 4 <= len_ && memcmp(buf_ + i, AC_MAGIC_BYTES, 4) != 0) i++;
      if (i > 0) {
        dropFront(i);  // garbage before the frame
        bad_++;
      }
      if (len_ < AC_FRAME_SIZE) return got;  // wait for the rest

      // 2. The CRC decides whether this is a real frame start or just
      //    payload bytes that happen to look like the magic.
      uint16_t want;
      memcpy(&want, buf_ + AC_PACKET_SIZE, sizeof(want));
      if (ac_crc16(buf_, AC_PACKET_SIZE) == want) {
        memcpy(&out, buf_, AC_PACKET_SIZE);
        dropFront(AC_FRAME_SIZE);
        good_++;
        got = true;
      } else {
        dropFront(1);  // false lead - keep hunting for the magic
        bad_++;
      }
    }
  }

  // Holds 7 frames - headroom for a long frame where drawing overruns.
  uint8_t buf_[2048];
  size_t len_ = 0;
  uint32_t good_ = 0;
  uint32_t bad_ = 0;
};
