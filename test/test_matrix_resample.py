#!/usr/bin/env python3
"""
Host-side regression test for the internal LED matrix resampler
(INTERNAL_LED_MATRIX in include/base.h).

The resampler is an area-weighted box filter: every source pixel
contributes to each destination cell it overlaps, weighted by the overlap
area. This script mirrors that math and checks it, so the algorithm can be
validated without a device:

  python3 test/test_matrix_resample.py

Exit code is non-zero if any check fails.
"""

import sys

MODE = "rgb"


def resample(sw, sh, mw, mh, pixel_fn):
    """Area-weighted box resample of an sw x sh source into an mw x mh matrix.

    Returns (sums, weights) where sums[c] is the [r, g, b] accumulator and
    weights[c] the total overlap weight of cell c (row-major).
    """
    n = mw * mh
    sums = [[0, 0, 0] for _ in range(n)]
    weights = [0] * n

    for row in range(sh):
        for col in range(sw):
            r, g, b = pixel_fn(col, row)
            # source pixel span in cell-scaled units (1 pixel = mw x mh)
            px0, px1 = col * mw, (col + 1) * mw
            py0, py1 = row * mh, (row + 1) * mh

            cx0, cx1 = px0 // sw, (px1 - 1) // sw
            cy0, cy1 = py0 // sh, (py1 - 1) // sh

            for cy in range(cy0, cy1 + 1):
                y0, y1 = cy * sh, (cy + 1) * sh
                oy = min(py1, y1) - max(py0, y0)
                if oy <= 0:
                    continue
                for cx in range(cx0, cx1 + 1):
                    x0, x1 = cx * sw, (cx + 1) * sw
                    ox = min(px1, x1) - max(px0, x0)
                    if ox <= 0:
                        continue
                    cell = cy * mw + cx
                    if cell >= n:
                        continue
                    w = ox * oy
                    sums[cell][0] += r * w
                    sums[cell][1] += g * w
                    sums[cell][2] += b * w
                    weights[cell] += w

    return sums, weights


def old_cell(col, row, mw, mh, sw, sh):
    """Previous (buggy) direct pixel->cell mapping, kept for contrast."""
    return ((row * mw) // sh) * mw + (col * mw) // sw


def avg(sums, weights, cell):
    if weights[cell] == 0:
        return (0, 0, 0)
    return tuple(s // weights[cell] for s in sums[cell])


failures = 0


def check(name, cond, detail=""):
    global failures
    if cond:
        print(f"  PASS  {name}")
    else:
        failures += 1
        print(f"  FAIL  {name} {detail}")


def solid(_c, _r):
    return (1, 1, 1)


def partition_of_unity(sw, sh, mw, mh):
    """Every cell must receive exactly sw*sh of overlap weight."""
    sums, weights = resample(sw, sh, mw, mh, solid)
    return all(w == sw * sh for w in weights), weights


print("area-weighted box resampler")
print("downscale 8x32 -> 5x5")
ok, weights = partition_of_unity(8, 32, 5, 5)
check("partition of unity (all cells == 256)", ok, weights)

print("non-square 8x32 -> 3x2")
ok, weights = partition_of_unity(8, 32, 3, 2)
check("partition of unity (all cells == 256)", ok, weights)
# contrast: the old mapping could address cells past the matrix and drop them
old_cells = {old_cell(c, r, 3, 2, 8, 32) for r in range(32) for c in range(8)}
check("old mapping overflows (the fixed bug)", max(old_cells) >= 6,
      f"old cells only reach {max(old_cells)}")

print("non-integer ratio 3x3 -> 2x2")
ok, weights = partition_of_unity(3, 3, 2, 2)
check("partition of unity (all cells == 9)", ok, weights)

print("upscale 2x2 -> 4x4")
ok, weights = partition_of_unity(2, 2, 4, 4)
check("partition of unity (all cells == 4)", ok, weights)

print("8x32 top-quarter pattern -> 5x5")
sums, weights = resample(8, 32, 5, 5, lambda _c, r: (0, 255, 0) if r < 8 else (0, 0, 0))
row0 = [avg(sums, weights, 0 * 5 + cx)[1] for cx in range(5)]
row1 = [avg(sums, weights, 1 * 5 + cx)[1] for cx in range(5)]
check("top matrix row fully lit (all == 255)", all(v == 255 for v in row0), row0)
check("second row ~25% (all == 63)", all(v == 63 for v in row1), row1)

print()
if failures:
    print(f"{failures} check(s) failed")
    sys.exit(1)
print("all checks passed")
