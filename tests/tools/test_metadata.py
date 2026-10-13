# SPDX-License-Identifier: MIT
"""Exercise metadata through the CLI and the firmware-package consumer."""

import io
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS / "flash"))

from firmware import create_firmware_blob


class MetadataTest(unittest.TestCase):
    def run_generator(self, platform, linker_map, *options):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            map_file = directory / "firmware.map"
            out_file = directory / "metadata.json"
            map_file.write_text(linker_map)
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOLS / "metadata.py"),
                    "test",
                    platform,
                    str(map_file),
                    str(out_file),
                    *options,
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            return result, out_file.read_text()

    def generate(self, platform, linker_map, *options):
        result, output = self.run_generator(platform, linker_map, *options)
        self.assertEqual(result.returncode, 0, result.stderr)
        return output

    def unpack(self, metadata, image, name=None):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr("firmware.metadata.json", metadata)
            package.writestr("firmware-base.bin", image)
        return create_firmware_blob(archive, name)[1]

    def test_unmodified_image_without_pybricks_sections(self):
        image = bytes(range(256))
        for platform in ("prime_hub_f4", "prime_hub_h5"):
            with self.subTest(platform=platform):
                metadata = self.generate(platform, "", "--no-custom-name")
                self.assertEqual(self.unpack(metadata, image)[platform], image)
                self.assertEqual(
                    self.unpack(metadata, image, "Ignored")[platform], image
                )

    def test_existing_name_patch(self):
        # Synthetic GNU linker-map input: no firmware/source fixture is required.
        metadata = self.generate(
            "prime_hub_f4",
            "FLASH_FIRMWARE 0x08010000 0x000f0000\n"
            ".name 0x08010010 0x10\n"
            ".user 0x08010020\n",
        )
        image = bytes(range(16)) + b"Original\0" + bytes(7)
        renamed = self.unpack(metadata, image, "Renamed")["prime_hub_f4"]
        self.assertEqual(renamed, image[:16] + b"Renamed\0" + image[24:])

    def test_checksum_is_preserved_without_custom_name(self):
        metadata = self.generate(
            "city_hub",
            "FLASH_FIRMWARE 0x00004000 0x00000020\n.user 0x00004010\n",
            "--no-custom-name",
        )
        image = bytes(range(28))
        blob = self.unpack(metadata, image)["city_hub"]
        self.assertEqual(blob[:28], image)
        self.assertEqual(len(blob), 32)
        words = [int.from_bytes(blob[i : i + 4], "little") for i in range(0, 32, 4)]
        self.assertEqual(sum(words) & 0xFFFFFFFF, 0)
        self.assertEqual(json.loads(metadata)["variants"][0]["hub-name-size"], 0)

    def test_explicit_sum_checksum_without_pybricks_map_sections(self):
        metadata = self.generate(
            "technic_hub",
            "Zephyr map with no Pybricks sections\n",
            "--no-custom-name",
            "--checksum-size",
            "0x20",
        )
        variant = json.loads(metadata)["variants"][0]
        self.assertEqual(variant["checksum-size"], 32)
        self.assertEqual(variant["hub-name-offset"], 0)
        self.assertEqual(variant["hub-name-size"], 0)

        image = bytes(range(12))
        blob = self.unpack(metadata, image)["technic_hub"]
        self.assertEqual(blob[:12], image)
        self.assertEqual(len(blob), 16)
        erased_tail = blob + b"\xff" * (32 - len(blob))
        words = [
            int.from_bytes(erased_tail[i : i + 4], "little") for i in range(0, 32, 4)
        ]
        self.assertEqual(sum(words) & 0xFFFFFFFF, 0)

    def test_explicit_sum_checksum_rejects_invalid_options(self):
        for platform, options, message in (
            ("technic_hub", ("--checksum-size", "32"), "--no-custom-name"),
            (
                "prime_hub_f4",
                ("--no-custom-name", "--checksum-size", "32"),
                "sum-checksummed",
            ),
            (
                "technic_hub",
                ("--no-custom-name", "--checksum-size", "0"),
                "multiple of four",
            ),
            (
                "technic_hub",
                ("--no-custom-name", "--checksum-size", "6"),
                "multiple of four",
            ),
            (
                "technic_hub",
                ("--no-custom-name", "--checksum-size=-4"),
                "multiple of four",
            ),
        ):
            with self.subTest(platform=platform, options=options):
                result, _ = self.run_generator(platform, "", *options)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)


if __name__ == "__main__":
    unittest.main()
