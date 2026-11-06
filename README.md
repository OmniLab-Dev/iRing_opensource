# iRing

Hardware and firmware for the iRing research prototype.

- [Firmware](firmware/iRing_OS/README.md): nRF52832 Arduino application, OLED UI,
  capacitive controls, BLE media keys, and optical heart-rate/SpO₂ acquisition.
- `pcb/`: PCB layout and schematic files.
- `3d/`: reserved for enclosure and mechanical design files.

Start with the [firmware build guide](firmware/iRing_OS/README.md) for board
connections, dependencies, controls, and BLE commands. The BLE device name is
`VisRing` for compatibility with existing pairings and companion software.

Third-party files retain their original notices; see [THIRD_PARTY.md](THIRD_PARTY.md).
