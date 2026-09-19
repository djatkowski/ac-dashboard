// Format pakietu telemetrii PC -> Tab5 (transport: USB CDC).
// LUSTRO pliku pc-app/acbridge/protocol.py - kazda zmiana musi trafic tam i tu.
//
// Ramka na drucie:
//     [4B magic "ACT5"] [264B reszta pakietu] [2B CRC16-CCITT] = 270 B
//
// Strumien bajtow nie ma granic pakietow, wiec magic sluzy za slowo
// synchronizujace, a CRC odsiewa przypadkowe bajty danych wygladajace
// jak magic.
#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t AC_MAGIC = 0x35544341UL;  // 'A','C','T','5' little-endian
static const uint8_t AC_MAGIC_BYTES[4] = {'A', 'C', 'T', '5'};
static const uint8_t AC_PROTO_VERSION = 2;

// Wypisujemy to w petli, dopoki nie ma telemetrii - po tym PC rozpoznaje,
// ze na tym porcie siedzi dashboard.
#define AC_HELLO_LINE "ACT5HELLO\n"

// flagi
enum : uint8_t {
  F_LIVE = 1 << 0,
  F_IN_PIT = 1 << 1,
  F_PIT_LIMITER = 1 << 2,
  F_ABS_ACTIVE = 1 << 3,
  F_TC_ACTIVE = 1 << 4,
  F_DRS = 1 << 5,
  F_VALID_LAP = 1 << 6,
  F_ENGINE_LIMITER = 1 << 7,
};

// Indeksy kol: 0 = FL, 1 = FR, 2 = RL, 3 = RR
enum { FL = 0, FR = 1, RL = 2, RR = 3 };

#pragma pack(push, 1)
struct AcPacket {
  uint32_t magic;
  uint8_t version;
  uint8_t flags;
  uint16_t seq;
  uint32_t ts_ms;

  float speed_kmh;
  float rpm;
  float max_rpm;

  int8_t gear;  // -1 = R, 0 = N, 1..n
  uint8_t fuel_pct;
  uint8_t tc_level;
  uint8_t abs_level;

  float throttle;  // 0..1
  float brake;     // 0..1
  float clutch;    // 0..1 (1 = wcisniete)
  float steer;     // -1..1
  float fuel_l;

  float drift_angle;  // stopnie, + = tyl ucieka w prawo
  float yaw_rate;     // stopnie/s
  float g_lat;
  float g_lon;
  float g_vert;

  float slip_angle[4];
  float wheel_slip[4];
  float tyre_temp[4];   // C
  float tyre_press[4];  // psi
  float brake_temp[4];  // C
  float tyre_dirt[4];
  float wheel_load[4];  // N

  float surface_grip;
  float air_temp;
  float road_temp;
  float turbo_boost;

  uint32_t lap_ms;
  uint32_t last_lap_ms;
  uint32_t best_lap_ms;
  uint16_t lap_count;
  uint8_t position;
  uint8_t tyres_out;

  float norm_pos;
  float distance_m;

  char car_name[24];
  char track_name[24];
};
#pragma pack(pop)

static const size_t AC_PACKET_SIZE = 268;
static const size_t AC_CRC_SIZE = 2;
static const size_t AC_FRAME_SIZE = AC_PACKET_SIZE + AC_CRC_SIZE;  // 270

static_assert(sizeof(AcPacket) == AC_PACKET_SIZE, "AcPacket rozjechal sie z protocol.py");

// CRC16-CCITT, wielomian 0x1021, init 0xFFFF.
// Ten sam algorytm co crc16() w pc-app/acbridge/protocol.py.
// Wartosc kontrolna dla "123456789" to 0x29B1.
inline uint16_t ac_crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}
