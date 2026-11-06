#!/usr/bin/env python3
"""Run Cortex-M4 regression tests in Unicorn. Requires the GNU Arm toolchain."""
import argparse
import subprocess
import tempfile
from pathlib import Path
from test_support import ROOT, SOURCES, SUITES, tool

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("toolchain", type=Path, help="GNU Arm toolchain bin directory")
    parser.add_argument("--suite", choices=SUITES, action="append")
    args = parser.parse_args()
    try:
        from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
    except ImportError:
        parser.error("Install Unicorn first: python -m pip install unicorn==2.1.4")

    def run(name, *arguments):
        return subprocess.check_output([tool(name, args.toolchain), *map(str, arguments)], text=True)

    with tempfile.TemporaryDirectory(prefix="iring-arm-") as output:
        for suite in args.suite or SUITES:
            elf, binary = Path(output) / f"{suite}.elf", Path(output) / f"{suite}.bin"
            run("arm-none-eabi-g++", "-std=c++11", "-Os", "-mcpu=cortex-m4", "-mthumb",
                "-ffreestanding", "-fno-exceptions", "-fno-rtti", "-nostdlib",
                "-Wl,-Ttext=0x10000", "-Wl,-e,run_tests", *SOURCES,
                ROOT / f"tests/{suite}.cpp", "-lc", "-lgcc", "-o", elf)
            run("arm-none-eabi-objcopy", "-O", "binary", elf, binary)
            symbols = run("arm-none-eabi-nm", elf)
            entry = int(next(line.split()[0] for line in symbols.splitlines()
                             if line.endswith(" run_tests")), 16)
            cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
            cpu.mem_map(0x10000, 0x10000)
            cpu.mem_write(0x10000, binary.read_bytes())
            cpu.mem_map(0x20000000, 0x10000)
            cpu.reg_write(UC_ARM_REG_SP, 0x2000fff0)
            cpu.reg_write(UC_ARM_REG_LR, 0x1fff1)
            cpu.emu_start(entry | 1, 0x1fff0, count=1_000_000)
            if cpu.reg_read(UC_ARM_REG_PC) != 0x1fff0:
                raise SystemExit(f"{suite}: execution did not return")
            line = cpu.reg_read(UC_ARM_REG_R0)
            if line:
                raise SystemExit(f"{suite}.cpp:{line}: assertion failed")
            print(f"PASS {suite} (Cortex-M4)", flush=True)

if __name__ == "__main__":
    main()
