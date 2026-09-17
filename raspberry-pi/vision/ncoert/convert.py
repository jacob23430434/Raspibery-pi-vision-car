# ===============================
# A 格式 -> B 格式（无行首逗号）
# ===============================

INPUT_FILE  = "A.txt"
OUTPUT_FILE = "B.txt"

values = []
rows = []

with open(INPUT_FILE, "r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        if "-" in line:      # 跳过分隔线
            continue
        try:
            values.append(int(line))
        except:
            pass

# 每 8 个数一行
for i in range(0, len(values), 8):
    chunk = values[i:i+8]
    if len(chunk) == 8:
        # ❌ 不要行首逗号
        row = ",".join(str(x) for x in chunk) + ","
        rows.append(row)

with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
    for r in rows:
        f.write(r + "\n")

print(f"完成：{len(rows)} 行")
