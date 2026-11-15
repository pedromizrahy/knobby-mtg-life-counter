import json
import struct
import tempfile
import unittest
from pathlib import Path
from tools.pokemon.audit_catalog import audit, validate_tpk2


def fake_tpk2():
    # Minimal structurally valid TPK2 file for tests; no copyrighted art.
    return (b"TPK2" + struct.pack("<BH", 1, 1) +
            b"\x00\x00" + struct.pack("<BBBB", 0, 2, 2, 1) +
            struct.pack("<H", 100) + bytes([0, 0, 0, 0]))


class AuditTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.mons = self.root / "mons"
        self.mons.mkdir()
        self.catalog = self.root / "catalog.json"

    def tearDown(self):
        self.tmp.cleanup()

    def write_catalog(self, rows):
        self.catalog.write_text(json.dumps({"schema": 1, "species": [
            {"id": number, "tier": tier, "asset": f"p{number}.bin"}
            for number, tier in rows]}), encoding="utf-8")

    def test_valid_asset_and_missing_tiers(self):
        self.write_catalog([(778, "rare")])
        (self.mons / "p778.bin").write_bytes(fake_tpk2())
        self.assertGreater(validate_tpk2(self.mons / "p778.bin"), 0)
        rows, ready = audit(self.catalog, self.mons)
        self.assertEqual(rows[0][2][:5], "VALID")
        self.assertFalse(any(ready.values()))

    def test_master_only_needs_legendary(self):
        self.write_catalog([(150, "legendary")])
        (self.mons / "p150.bin").write_bytes(fake_tpk2())
        _, ready = audit(self.catalog, self.mons)
        self.assertTrue(ready["master"])
        self.assertFalse(ready["poke"])
        self.assertFalse(ready["great"])
        self.assertFalse(ready["ultra"])

    def test_all_four_tiers_allow_every_ball(self):
        tiers = [(25, "common"), (778, "rare"), (149, "epic"), (150, "legendary")]
        self.write_catalog(tiers)
        for number, _ in tiers:
            (self.mons / f"p{number}.bin").write_bytes(fake_tpk2())
        _, ready = audit(self.catalog, self.mons)
        self.assertTrue(all(ready.values()))

    def test_reject_corrupt_asset(self):
        self.write_catalog([(150, "legendary")])
        (self.mons / "p150.bin").write_bytes(b"TPK2" + bytes(20))
        rows, ready = audit(self.catalog, self.mons)
        self.assertIn("INVALID", rows[0][2])
        self.assertFalse(ready["master"])

    def test_duplicate_and_path_spoof(self):
        self.write_catalog([(778, "rare"), (778, "epic")])
        with self.assertRaises(ValueError):
            audit(self.catalog, self.mons)
        self.catalog.write_text(json.dumps({"schema": 1, "species": [
            {"id": 778, "tier": "rare", "asset": "../outside.bin"}]}))
        with self.assertRaises(ValueError):
            audit(self.catalog, self.mons)

    def test_repository_candidate_pool_has_all_four_tiers(self):
        # Candidate pool is a PROPOSAL, not a list of files installed on DIAL.
        candidate = Path(__file__).resolve().parent.parent / "tools" / "pokemon" / "candidate_pool.json"
        manifest = json.loads(candidate.read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema"], 1)
        self.assertIn("NOT_INSTALLED", manifest["status"])
        rows = manifest["species"]
        self.assertGreaterEqual(len(rows), 4)
        ids = [row["id"] for row in rows]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual({row["tier"] for row in rows},
                         {"common", "rare", "epic", "legendary"})
        for row in rows:
            self.assertEqual(row["asset"], f"p{row['id']}.bin")
            self.assertTrue(1 <= row["id"] <= 1025)

    def test_no_idle_and_trailing_bytes(self):
        good = fake_tpk2()
        (self.mons / "p778.bin").write_bytes(good + b"X")
        with self.assertRaises(ValueError):
            validate_tpk2(self.mons / "p778.bin")
        (self.mons / "p778.bin").write_bytes(
            good[:9] + bytes([1]) + good[10:])
        with self.assertRaises(ValueError):
            validate_tpk2(self.mons / "p778.bin")


if __name__ == "__main__":
    unittest.main()
