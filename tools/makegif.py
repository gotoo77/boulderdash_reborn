#!/usr/bin/env python3

from PIL import Image
import sys
from pathlib import Path

def make_gif(output, frames, duration):
    images = [Image.open(f).convert("RGBA") for f in frames]

    # Vérification taille
    for img in images:
        if img.size != (32, 32):
            raise ValueError(f"Frame {img} n'est pas en 32x32")

    images[0].save(
        output,
        save_all=True,
        append_images=images[1:],
        duration=duration,
        loop=0,
        disposal=2,
        transparency=0,
    )

    print(f"[OK] GIF généré : {output}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage:")
        print("  python make_gif.py output.gif duration_ms frame1.png frame2.png ...")
        sys.exit(1)

    output = sys.argv[1]
    duration = int(sys.argv[2])
    frames = sys.argv[3:]

    make_gif(output, frames, duration)
