#!/usr/bin/env python3
"""Generuje strumien ramek (ze wstrzyknietymi smieciami) do test_parser.cpp.

Wartosci sa wyliczane z numeru ramki, zeby strona C mogla je sprawdzic
bez przekazywania oczekiwan osobnym kanalem.
"""
import random
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "pc-app"))

from acbridge.protocol import MAGIC, Frame  # noqa: E402

N = 200
out = bytearray()
for i in range(N):
    f = Frame()
    f.speed_kmh = i * 1.5
    f.gear = (i % 7) - 1
    f.drift_angle = float(i) - 50.0
    f.rpm = 1000 + i * 30
    f.car_name = "ks_toyota_ae86_drift"
    f.track_name = "ek_ebisu_minami"
    out += f.frame(i)
    if i == 7:
        out += b"\x00\xff\x13\x37 smieci z serial monitora\n"
    if i == 15:
        out += MAGIC + b"\x01\x02"            # falszywy magic
    if i == 30:
        random.seed(1)
        out += bytes(random.randrange(256) for _ in range(300))

dest = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "stream.bin")
dest.write_bytes(out)
print(f"zapisano {len(out)} B -> {dest}  ({N} ramek + smieci)")
