# 引擎算法交叉验证（host_test 降级：本机无 gcc 时用 Python 等价复现核心断言）

def euclid(pulses, rotate):
    if pulses == 0:
        return 0
    if pulses >= 16:
        return 0xFFFF
    bits = 0
    acc = 0
    for s in range(16):
        acc += pulses
        if acc >= 16:
            bits |= 1 << s
            acc -= 16
    rotate &= 15
    if rotate:
        bits = ((bits << rotate) | (bits >> (16 - rotate))) & 0xFFFF
    return bits


def bitset(b):
    return [i for i in range(16) if (b >> i) & 1]


assert bitset(euclid(4, 1)) == [0, 4, 8, 12], "euclid(4,1)"
assert euclid(0, 7) == 0, "euclid(0,x) all off"
assert euclid(16, 3) == 0xFFFF, "euclid(>=16,x) all on"

# 间距差 <= 1
for p in range(1, 17):
    for r in range(16):
        b = bitset(euclid(p, r))
        if len(b) < 2:
            continue
        gaps = []
        for i in range(len(b)):
            g = (b[(i + 1) % len(b)] - b[i]) % 16
            gaps.append(g if g else 16)
        if max(gaps) - min(gaps) > 1:
            raise AssertionError(f"gap diff >1 for p={p} r={r}: {gaps}")
print("euclid vectors OK")

# swing 触发点：偶数步 sub_tick=0；奇数步 sub_tick=swing*8/100
def trigger_tick(step, swing):
    return (swing * 8 // 100) if (step & 1) else 0


assert trigger_tick(0, 50) == 0 and trigger_tick(1, 50) == 4 and trigger_tick(2, 50) == 0
assert trigger_tick(1, 25) == 2 and trigger_tick(1, 100) == 8
print("swing trigger OK")

# 每轨 base_pulses + 强制 bit0（temp<5）：Kick temp0 -> pulses4 rotate1 -> [0,4,8,12]
print("all checks passed")
