import subprocess
import numpy as np
from PIL import Image

IMAGE = "images/example_picture.png"
BINARY = "./swe"
CHUNK_SIZE = 250
TARGET_ACCURACY = 0.97
MAX_COMPONENTS = 35

img = Image.open(IMAGE).convert("RGB")
w, h = img.size
img = img.resize((round(w), round(h )), Image.LANCZOS)
arr = np.asarray(img, dtype=np.float64)
h, w, c = arr.shape

recon = np.empty_like(arr)
encoded_bytes = 0
for ch in range(c):
    for row in range(h):
        for start in range(0, w, CHUNK_SIZE):
            end = min(start + CHUNK_SIZE, w)
            values = ",".join(f"{v:.10g}" for v in arr[row, start:end, ch])
            out = subprocess.run(
                [BINARY, str(TARGET_ACCURACY), str(MAX_COMPONENTS), "1.0", "0", values],
                capture_output=True, text=True,
            ).stdout
            for line in out.strip().splitlines():
                if line[0].isdigit():
                    idx, _, _, val = line.split(",")
                    recon[row, start + int(idx), ch] = float(val)
                elif line.startswith("# summary"):
                    encoded_bytes += int(line.split("encoded_bytes=")[1].split()[0])

recon = np.clip(recon, 0, 255).astype(np.uint8)
original_bytes = arr.size * 8
r2 = 1 - np.mean((arr - recon) ** 2) / np.var(arr)

Image.fromarray(recon).save("image_reconstructed.png")

print(f"chunk_size={CHUNK_SIZE}")
print(f"target_accuracy={TARGET_ACCURACY}")
print(f"max_components={MAX_COMPONENTS}")
print(f"achieved R^2={r2:.4f}")
print(f"{original_bytes} -> {encoded_bytes} bytes ({original_bytes / encoded_bytes:.2f}x)")
