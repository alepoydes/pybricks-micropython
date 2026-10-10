# SPDX-License-Identifier: MIT
"""Check standard F4 installation paths without USB hardware."""

import contextlib
import io
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/flash"))

import dfu
from lwp3.bytecodes import HubKind


class InstallTest(unittest.TestCase):
    def test_factory_loader_preserved_and_main_header_written_last(self):
        image = b"\x00\x00\x05\x20" + bytes(0x300 - 4)
        operations = []
        for bcd in (0x0100, 0x0300):
            with self.subTest(bcd=bcd):
                operations.clear()
                device = SimpleNamespace(
                    _dev=SimpleNamespace(
                        idVendor=0x0694, idProduct=0x0011, bcdDevice=bcd
                    ),
                    get_flash_layout_str=lambda: "064Kg,128Kg",
                    get_serial=lambda: "synthetic",
                    set_mcu_family=lambda family: None,
                    erase_firmware=lambda address, size: operations.append(
                        ("erase", address, size)
                    ),
                    write_firmware=lambda address, data: operations.append(
                        ("write", address, bytes(data))
                    ),
                    exit=lambda: operations.append(("exit",)),
                )
                with (
                    patch.object(dfu, "get_dfu_devices", return_value=[object()]),
                    patch.object(dfu, "DfuDevice", return_value=device),
                    contextlib.redirect_stdout(io.StringIO()),
                    self.assertRaises(SystemExit) as result,
                ):
                    dfu.flash_dfu({"prime_hub_f4": image}, HubKind.TECHNIC_LARGE)
                self.assertEqual(result.exception.code, 0)
                prefix = dfu.MBOOT_PATH.read_bytes() if bcd == 0x0100 else b""
                address = 0x08008000 if bcd == 0x0100 else 0x08010000
                self.assertEqual(
                    operations,
                    [
                        ("erase", address, len(prefix) + len(image)),
                        ("write", address, prefix + b"\xff" * 4 + image[4:]),
                        ("write", 0x08010000, image[:4]),
                        ("exit",),
                    ],
                )


if __name__ == "__main__":
    unittest.main()
