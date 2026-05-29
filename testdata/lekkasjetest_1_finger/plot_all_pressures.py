import os
import re
import matplotlib.pyplot as plt
import numpy as np

BASE_DIR = r"c:\Users\birge\OneDrive - NTNU\Skole\Master\VSCode\testdata\lekkasjetest_1_finger"

LEVELS = [
    {"dir": "0.02bar", "target": 0.02, "color": "#4878d0", "label": "0.02 bar"},
    {"dir": "0.04bar", "target": 0.04, "color": "#ee854a", "label": "0.04 bar"},
    {"dir": "0.06bar", "target": 0.06, "color": "#6acc65", "label": "0.06 bar"},
]

STAT_WINDOW   = 10
SMOOTH_WINDOW = 15
HOLD_DURATION = 55.0
COMMON_T      = np.linspace(0, HOLD_DURATION, 1200)


def load(path):
    times, pressures = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.lower().startswith("sensor"):
                continue
            parts = line.split()
            if len(parts) == 2:
                times.append(int(parts[0]) / 1000.0)
                pressures.append(float(parts[1]))
    return np.array(times), np.array(pressures)


def smooth(arr, w):
    out = np.convolve(arr, np.ones(w) / w, mode="same")
    half = w // 2
    out[:half] = arr[:half]
    out[-half:] = arr[-half:]
    return out


fig, ax = plt.subplots(figsize=(10, 5))

for lvl in LEVELS:
    data_dir = os.path.join(BASE_DIR, lvl["dir"])
    target   = lvl["target"]
    color    = lvl["color"]

    files = sorted(
        (f for f in os.listdir(data_dir) if f.endswith(".txt")),
        key=lambda f: int(re.match(r"(\d+)", f).group(1))
    )

    interp_curves = []
    for fname in files:
        times, pressures = load(os.path.join(data_dir, fname))
        above = np.where(pressures >= target)[0]
        if len(above) == 0:
            continue
        first_idx = above[0]
        t_norm  = times - times[first_idx]
        p_smooth = smooth(pressures, SMOOTH_WINDOW)
        mask    = (t_norm >= 0) & (t_norm <= HOLD_DURATION)
        t_clip  = t_norm[mask]
        p_clip  = p_smooth[mask]
        interp_curves.append(np.interp(COMMON_T, t_clip, p_clip))

    matrix     = np.array(interp_curves)
    mean_curve = matrix.mean(axis=0)

    for curve in interp_curves:
        ax.plot(COMMON_T, curve, color=color, linewidth=0.8, alpha=0.3)

    ax.plot(COMMON_T, mean_curve, color=color, linewidth=2.0, label=lvl["label"])

ax.set_xlabel("Time after reaching target pressure [s]")
ax.set_ylabel("Pressure [bar]")
ax.set_title("Leakage during hold phase — all pressure levels")
ax.legend(title="Target pressure", loc="center right", bbox_to_anchor=(1.0, 0.35))
ax.grid(True, alpha=0.3)
plt.tight_layout()

out_path = os.path.join(BASE_DIR, "lekkasje_all_pressures.png")
plt.savefig(out_path, dpi=150)
print(f"Saved to {out_path}")
plt.show()
