"""Compile the production gesture code for Cortex-M4 and execute it in Unicorn."""
import subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0

root = Path(__file__).resolve().parents[1]
cache = Path.home() / ".cache" / "ha-button"
gcc = cache / "zephyr-sdk-0.17.4/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc.exe"
output = cache / "gesture-tests.elf"
subprocess.run([
    str(gcc), "-mcpu=cortex-m4", "-mthumb", "-Os", "-g", "-std=c11",
    "-Wall", "-Wextra", "-Werror", "-ffreestanding", "-fno-builtin", "-nostdlib",
    "-I", str(root / "src"), str(root / "src/button_gestures.c"),
    str(root / "tests/gestures_test.c"), str(root / "src/bthome_packet.c"),
    str(root / "tests/packet_test.c"),
    "-Wl,-e,run_tests,-Ttext=0x10000,-Tdata=0x20000000", "-o", str(output)
], check=True)

cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
cpu.mem_map(0x10000, 0x10000)
cpu.mem_map(0x20000000, 0x10000)
with output.open("rb") as stream:
    elf = ELFFile(stream)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
            cpu.mem_write(segment["p_vaddr"], segment.data())
    symbols = elf.get_section_by_name(".symtab")
    passed_address = symbols.get_symbol_by_name("tests_passed")[0]["st_value"]
    entry = elf.header["e_entry"]
cpu.reg_write(UC_ARM_REG_SP, 0x2000FFF0)
cpu.reg_write(UC_ARM_REG_LR, 0x1FFF1)
finished = False


def stop_at_return(cpu, address, size, context):
    global finished
    if address == 0x1FFF0:
        finished = True
        cpu.emu_stop()


cpu.hook_add(UC_HOOK_CODE, stop_at_return)
cpu.emu_start(entry | 1, 0, count=1000000)
if not finished:
    raise SystemExit("FAIL: test execution did not return")
failure_line = cpu.reg_read(UC_ARM_REG_R0)
if failure_line:
    location = (f"packet_test.c:{failure_line - 1000}" if failure_line >= 1000
                else f"gestures_test.c:{failure_line}")
    raise SystemExit(f"FAIL: {location}")
passed = int.from_bytes(cpu.mem_read(passed_address, 4), "little")
assert passed == 19, passed
print(f"PASS: {passed} Cortex-M4 scenarios: 13 gestures + 6 BTHome packets")
