import numpy as np
import matplotlib.pyplot as plt

def load_data(path):
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip().strip(",")
            if not line:
                continue
            parts = [p for p in line.split(",") if p != ""]
            if len(parts) >= 8:
                rows.append([float(x) for x in parts[:8]])
    return np.array(rows, dtype=float)

X = load_data("W900.txt")
T, CH = X.shape
assert CH == 8
eps = 1e-9

# 每帧的“共同亮度”目标（白底但会漂移）
t = X.mean(axis=1)  # shape (T,)

# ========= 评分：通道一致性 + 保形 + 防塌缩 =========
def score_model(X, Y):
    # (1) 同帧通道一致性：越小越好
    s_cons = np.mean(np.std(Y, axis=1))

    # (2) 保形：每个通道与原始的相关（越高越好）
    corr = []
    for i in range(CH):
        c = np.corrcoef(X[:, i], Y[:, i])[0, 1]
        corr.append(0.0 if np.isnan(c) else c)
    s_shape = np.mean(1 - np.clip(corr, -1, 1))

    # (3) 防塌缩：如果输出随时间的波动被压没了，惩罚
    # 用整体时间方差比例来约束（1 附近最好）
    var_ratio = (np.std(Y) + eps) / (np.std(X) + eps)
    s_collapse = abs(var_ratio - 1.0)

    return s_cons + 0.15 * s_shape + 0.20 * s_collapse

results = []

# ========= M1: per-channel gain to match frame_mean =========
# 目标：a_i * x_i ≈ t(t)
a1 = np.zeros(CH)
for i in range(CH):
    xi = X[:, i]
    a1[i] = (np.dot(t, xi)) / (np.dot(xi, xi) + eps)
Y1 = X * a1
results.append(("gain_to_frame_mean", Y1, a1, None, score_model(X, Y1)))

# ========= M2: per-channel affine to match frame_mean =========
# 目标：a_i * x_i + b_i ≈ t(t)
a2 = np.zeros(CH)
b2 = np.zeros(CH)
for i in range(CH):
    xi = X[:, i]
    A = np.vstack([xi, np.ones_like(xi)]).T
    a2[i], b2[i] = np.linalg.lstsq(A, t, rcond=None)[0]
Y2 = X * a2 + b2
results.append(("affine_to_frame_mean", Y2, a2, b2, score_model(X, Y2)))

# ========= M3: gain then divide by frame_mean (强力去光照漂移) =========
# 先做通道增益，再除掉帧均值，把每帧统一到同尺度
a3 = X.mean() / (X.mean(axis=0) + eps)
frame_mean = X.mean(axis=1, keepdims=True)
Y3 = (X * a3) / (frame_mean + eps)
results.append(("gain_then_frame_norm", Y3, a3, None, score_model(X, Y3)))

# ========= M4: affine then divide by frame_mean（更强，但可能过拟合） =========
a4 = np.zeros(CH)
b4 = np.zeros(CH)
for i in range(CH):
    xi = X[:, i]
    A = np.vstack([xi, np.ones_like(xi)]).T
    a4[i], b4[i] = np.linalg.lstsq(A, t, rcond=None)[0]
Y4_raw = X * a4 + b4
Y4 = Y4_raw / (Y4_raw.mean(axis=1, keepdims=True) + eps)
results.append(("affine_then_frame_norm", Y4, a4, b4, score_model(X, Y4)))

# ========= 选最优 =========
best = min(results, key=lambda r: r[4])
name, Ybest, a, b, sc = best

print("\n=== BEST MODEL ===")
print("Model:", name)
print("Score:", sc)
print("a:", np.array(a))
if b is not None:
    print("b:", np.array(b))

# ========= 可视化 =========
plt.figure(figsize=(12, 8))
plt.subplot(2, 1, 1)
plt.title("RAW (first 300)")
plt.plot(X[:1000])
plt.subplot(2, 1, 2)
plt.title(f"BEST: {name} (first 300)")
plt.plot(Ybest[:1000])
plt.tight_layout()
plt.show()
