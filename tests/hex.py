def sh(x):
    if x < 0:
        x += 1 << 32
    return hex(x), bin(x)
