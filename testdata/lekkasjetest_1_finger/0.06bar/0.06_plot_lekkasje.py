import os
import re
import matplotlib.pyplot as plt
import numpy as np

DATA_DIR = r"c:\Users\birge\OneDrive - NTNU\Skole\Master\VSCode\testdata\lekkasjetest_1_finger\0.06bar"
TARGET_PRESSURE = 0.06
STAT_WINDOW = 10
SMOOTH_WINDOW = 15
HOLD_DURATION = 55.0
COMMON_T = np.linspace(0, HOLD_DURATION, 1200)

files = sorted(
    (f for f in os.listdir(DATA_DIR) if f.endswith(".txt")),
    key=lambda f: int(re.match(r"(\d+)", f).group(1))
)


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


interp_curves = []
stats = []

for i, fname in enumerate(files):
    times, pressures = load(os.path.join(DATA_DIR, fname))

    above = np.where(pressures >= TARGET_PRESSURE)[0]
    if len(above) == 0:
        continue

    first_idx = above[0]
    t_norm = times - times[first_idx]
    p_smooth = smooth(pressures, SMOOTH_WINDOW)

    mask = (t_norm >= 0) & (t_norm <= HOLD_DURATION)
    t_clip = t_norm[mask]
    p_clip = p_smooth[mask]

    interp_curves.append(np.interp(COMMON_T, t_clip, p_clip))

    begin_p = float(np.mean(pressures[first_idx:first_idx + STAT_WINDOW]))
    begin_t = times[first_idx]
    end_t_target = begin_t + HOLD_DURATION
    nearest = int(np.argmin(np.abs(times - end_t_target)))
    half = STAT_WINDOW // 2
    sl = slice(max(0, nearest - half), min(len(times), nearest + half))
    end_p = float(np.mean(pressures[sl]))
    drop = begin_p - end_p
    duration = times[nearest] - begin_t
    rate = drop / duration if duration > 0 else 0.0
    rel = rate / begin_p * 100 if begin_p > 0 else 0.0
    stats.append((f"Test {i + 1}", begin_p, end_p, drop, rate, rel))

matrix = np.array(interp_curves)
mean_curve = matrix.mean(axis=0)
std_curve = matrix.std(axis=0)

fig, ax = plt.subplots(figsize=(9, 5))

for curve in interp_curves:
    ax.plot(COMMON_T, curve, color="gray", linewidth=0.8, alpha=0.5)

ax.fill_between(COMMON_T, mean_curve - std_curve, mean_curve + std_curve,
                alpha=0.3, color="steelblue", label=r"$\pm$1 std")
ax.plot(COMMON_T, mean_curve, color="steelblue", linewidth=2.0, label="Mean pressure")

y_lo = (mean_curve - std_curve).min()
y_hi = (mean_curve + std_curve).max()
margin = (y_hi - y_lo) * 0.15
ax.set_ylim(y_lo - margin, y_hi + margin)
ax.set_xlabel(f"Time after reaching {TARGET_PRESSURE} bar [s]")
ax.set_ylabel("Pressure [bar]")
ax.set_title(f"Leakage during hold phase at {TARGET_PRESSURE} bar")
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(os.path.join(DATA_DIR, "lekkasje_0.06bar_1finger.png"), dpi=150)

mean_vals = [float(np.mean([s[j] for s in stats])) for j in range(1, 6)]
stats.append(("Mean", *mean_vals))

rows = []
for s in stats:
    name, bp, ep, drop, rate, rel = s
    rows.append([name, f"{bp:.5f}", f"{ep:.5f}", f"{drop:.5f}", f"{rate:.2e}", f"{rel:.4f}"])

header = f"{'Test':<10} {'Begin [bar]':>14} {'End [bar]':>12} {'Drop [bar]':>12} {'Rate [bar/s]':>14} {'Rel [%/s]':>12}"
print(header)
print("-" * len(header))
for s in rows:
    name, bp, ep, drop, rate, rel = s
    print(f"{name:<10} {bp:>14} {ep:>12} {drop:>12} {rate:>14} {rel:>12}")

plt.show()
