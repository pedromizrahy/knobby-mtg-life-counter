import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools" / "pokemon"))
from upload_batch import plan, upload, verify_installed


def sprite():
    return (b"TPK2" + bytes([1, 1, 0]) + b"\x00\x00" +
            bytes([0, 2, 2, 1]) + b"\x64\x00" + bytes(4))


class BatchUploadTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.manifest = self.root / "pool.json"
        self.manifest.write_text(json.dumps({"schema": 1, "species": [
            {"id": 778, "asset": "p778.bin"},
            {"id": 25, "asset": "p25.bin"}]}), encoding="utf-8")
        (self.root / "p25.bin").write_bytes(sprite())

    def tearDown(self):
        self.tmp.cleanup()

    def test_dry_run_skips_mimikyu_and_never_writes(self):
        items = plan(self.manifest, self.root)
        self.assertEqual([i[0] for i in items], [25])
        self.assertEqual(items[0][1], sprite())
        self.assertFalse((self.root / "p778.bin").exists())

    def test_including_mimikyu_requires_valid_asset(self):
        with self.assertRaises(ValueError):
            plan(self.manifest, self.root, include_mimikyu=True)

    def test_rejects_spoofed_name(self):
        self.manifest.write_text(json.dumps({"schema":1, "species": [
            {"id":25, "asset":"../p25.bin"}]}))
        with self.assertRaises(ValueError):
            plan(self.manifest, self.root)

    def test_post_upload_verification_succeeds(self):
        port = MagicMock()
        port.readline.side_effect = [
            b"[Pokemon] PMD CHECK #25: /mons/p25.bin (read-only)\\n",
            b"[Pokemon] PMD VALID size=16 actions=1 palette=1 frames=1\\n",
            b"[Pokemon] PMD CHECK COMPLETE: card unmounted; no files modified.\\n",
        ]
        verify_installed(port, 25)
        port.write.assert_called_once_with(b"PK PMD CHECK 25\\n")

    def test_post_upload_verification_fails_closed(self):
        port = MagicMock()
        port.readline.side_effect = [b"[Pokemon] PMD INVALID size=16\\n"]
        with self.assertRaises(RuntimeError):
            verify_installed(port, 25)

    def test_protocol_uses_species_and_256_byte_acks(self):
        fake = MagicMock()
        with patch("upload_batch.receive") as read, patch("upload_batch.verify_installed") as verify, patch("upload_batch.time.sleep"):
            upload(fake, 25, sprite(), 0x1234abcd)
            self.assertIn(b"PK PMD PUT 25 ", fake.write.call_args_list[0].args[0])
            self.assertEqual(read.call_args_list[0].args[1], "PMD_READY")
            self.assertEqual(read.call_args_list[1].args[1], "#")
            self.assertIn("1234abcd", read.call_args_list[-1].args[1])
            verify.assert_called_once_with(fake, 25)


if __name__ == "__main__":
    unittest.main()
