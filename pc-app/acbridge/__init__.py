"""Assetto Corsa -> M5Stack Tab5 telemetry bridge."""

from .protocol import PACKET_SIZE, VERSION, Frame

__all__ = ["Frame", "PACKET_SIZE", "VERSION"]
__version__ = "1.0.0"
