import numpy as np
import matplotlib.pyplot as plt

# ======================
# 1. 读取数据
# ======================
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
    return np.array(rows)

data = load_data("W900.txt")   # ← 改成你的文件名
N, CH = data.shape
print(f"Loaded data: {N} samples, {CH} channels")

# ======================
# 2. 前 50 条算校准系数 K
# ======================
CAL_N = 30
cal_data = data[:CAL_N]

ch_mean = cal_data.mean(axis=0)
global_mean = ch_mean.mean()
K = global_mean / ch_mean

print("\n=== Calibration result ===")
for i in range(CH):
    print(f"CH{i}: mean={ch_mean[i]:.2f}, K={K[i]:.5f}")

# ======================
# 3. 应用到全部 1000 条
# ======================
data_cal = data * K
eps = 1e-9
p_low  = np.percentile(data_cal, 5, axis=0)
p_high = np.percentile(data_cal, 95, axis=0)

data_norm = (data_cal - p_low) / (p_high - p_low + eps)
data_norm = np.clip(data_norm, 0, 1)

print("P5 :", np.round(p_low, 2))
print("P95:", np.round(p_high, 2))
print("Norm range check:", data_norm.min(), data_norm.max())

# ======================
# 4. 数值检验
# ======================
print("\n=== Statistics (RAW, all data) ===")
print("mean:", np.round(data.mean(axis=0), 2))
print("std :", np.round(data.std(axis=0), 2))

print("\n=== Statistics (CALIBRATED, all data) ===")
print("mean:", np.round(data_cal.mean(axis=0), 2))
print("std :", np.round(data_cal.std(axis=0), 2))

print("\n=== First 50 after calibration (mean) ===")
print(np.round(data_cal[:50].mean(axis=0), 2))

# ======================
# 5. 画图验证
# ======================
plt.figure(figsize=(12, 8))

# 原始数据（前 1000 条）
plt.subplot(2, 1, 1)
plt.title("RAW data (first 1000 samples)")
plt.plot(data[:1000])
plt.ylabel("Value")

# 校准后数据（前 1000 条）
plt.subplot(2, 1, 2)
plt.title("CALIBRATED data (first 1000 samples)")
plt.plot(data_cal[:1000])
plt.xlabel("Sample")
plt.ylabel("Value")

plt.tight_layout()
plt.show()
