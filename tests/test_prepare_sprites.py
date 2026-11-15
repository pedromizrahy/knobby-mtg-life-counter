import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "pokemon"))
from prepare_sprites import pack_tpk2, frames_from_image, SIDE
from audit_catalog import validate_tpk2


class SpritePreparationTest(unittest.TestCase):
    def test_png_to_tpk2(self):
        frame = Image.new("RGBA", (SIDE, SIDE), (0, 0, 0, 0))
        for x in range(8, 36):
            for y in range(12, 32):
                frame.putpixel((x, y), (200, 50, 80, 255))
        data = pack_tpk2([frame], [150])
        with tempfile.TemporaryDirectory() as dirname:
            path = Path(dirname) / "p25.bin"
            path.write_bytes(data)
            self.assertEqual(validate_tpk2(path), len(data))
        self.assertIn(bytes([255]), [data[-SIDE*SIDE:][-1:]])
        self.assertEqual(data[:4], b"TPK2")

    def test_six_frames_and_limits(self):
        frames = []
        for i in range(6):
            item = Image.new("RGBA", (SIDE, SIDE), (20*i, 120, 200, 255))
            item.putpixel((0, 0), (0, 0, 0, 0))
            frames.append(item)
        data = pack_tpk2(frames, [100] * 6)
        with tempfile.TemporaryDirectory() as dirname:
            path = Path(dirname) / "p149.bin"
            path.write_bytes(data)
            validate_tpk2(path)
        with self.assertRaises(ValueError):
            pack_tpk2(frames + frames, [100] * 12)

    def test_load_animation_gif(self):
        import io
        images = [
            Image.new("RGBA", (22, 30), (255, 0, 0, 255)),
            Image.new("RGBA", (22, 30), (0, 0, 255, 255)),
        ]
        bio = io.BytesIO()
        images[0].save(bio, format="GIF", append_images=images[1:],
                       save_all=True, duration=[120, 200], loop=0)
        frames, duration = frames_from_image(bio.getvalue())
        self.assertEqual(len(frames), 2)
        self.assertEqual(duration, [120, 200])
        self.assertEqual(frames[0].size, (SIDE, SIDE))


if __name__ == "__main__":
    unittest.main()
