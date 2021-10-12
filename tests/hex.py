def sh(x):
    if x < 0:
        x += 1 << 32
    if x < 0:
        raise ValueError
    return hex(x), bin(x)
