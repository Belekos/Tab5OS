#!/usr/bin/env python3
"""Inspect the actual SDK/cache configuration and linked LCD ISR residency.

This is a static verification, not a hardware flicker test. No SDK mutation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    cfg = args.sdk / "dio_qspi/include/sdkconfig.h"
    linker = args.sdk / "ld/sections.ld"
    definitions = dict(re.findall(r"^#define\s+(CONFIG_\w+)\s+([^\n]+)", cfg.read_text(), re.M))
    names = [
        "CONFIG_IDF_TARGET", "CONFIG_SPIRAM_SPEED", "CONFIG_CACHE_L2_CACHE_SIZE",
        "CONFIG_CACHE_L2_CACHE_LINE_SIZE", "CONFIG_SPIRAM_XIP_FROM_PSRAM",
        "CONFIG_SPI_FLASH_AUTO_SUSPEND", "CONFIG_LCD_DSI_ISR_HANDLER_IN_IRAM",
        "CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL", "CONFIG_GDMA_ISR_HANDLER_IN_IRAM",
        "CONFIG_GDMA_OBJ_DRAM_SAFE", "CONFIG_SPI_FLASH_YIELD_DURING_ERASE",
    ]
    symbols = [
        "mipi_dsi_dma_trans_done_cb", "mipi_dsi_bridge_isr_handler",
        "dw_gdma_channel_default_isr", "dw_gdma_channel_enable_ctrl",
        "dw_gdma_channel_use_link_list", "dw_gdma_link_list_get_item",
        "dw_gdma_lli_set_block_markers",
    ]
    ldtext = linker.read_text()
    # Linker fragments, not source IRAM_ATTR annotations, determine residency.
    iram_start = ldtext.find(".iram0.text")
    if iram_start < 0:
        raise RuntimeError("IRAM output section missing from the actual SDK linker script")
    iram_end = ldtext.find("} >", iram_start)
    if iram_end < 0:
        raise RuntimeError("IRAM output section terminator missing")
    iram = ldtext[iram_start:iram_end]
    placed = {name: (".text." + name) in iram for name in symbols}
    result = {
        "kind": "static-sdk-and-linked-isr-review",
        "hardwareTest": False,
        "sdkConfigSha256": digest(cfg), "sdkLinkerSha256": digest(linker),
        "config": {name: definitions.get(name, "disabled/not-defined") for name in names},
        "expectedIramLinkerPlacement": placed,
        "linkedVerificationExecuted": False,
    }
    if args.elf:
        sections = subprocess.check_output(["readelf", "-WS", str(args.elf)], text=True)
        symtable = subprocess.check_output(["readelf", "-Ws", str(args.elf)], text=True)
        index_names = {index: name for index, name in re.findall(r"\[\s*(\d+)\]\s+(\S+)", sections)}
        actual = {}
        for line in symtable.splitlines():
            parts = line.split()
            if len(parts) >= 8 and parts[-1] in symbols:
                actual[parts[-1]] = {"section": index_names.get(parts[6], parts[6]), "address": parts[1]}
        missing = [name for name in symbols if name not in actual]
        wrong = [name for name in symbols if name in actual and "iram" not in actual[name]["section"]]
        result.update(linkedVerificationExecuted=True, elfSha256=digest(args.elf),
                      symbols=actual, missingSymbols=missing, nonIramSymbols=wrong)
        result["passed"] = all(placed.values()) and not missing and not wrong
    else:
        result["passed"] = all(placed.values())
        result["limit"] = "Only SDK configuration and linker placement inspected; final ELF residency not yet verified."
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": result["passed"], "linked": result["linkedVerificationExecuted"], "report": str(args.out)}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
