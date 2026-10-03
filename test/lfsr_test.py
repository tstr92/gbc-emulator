import sys

def get_freq(shift, div):
    div1 = div if div!=0 else 0.5
    f1 = 262144/(div1*2**shift)
    if 0 == div:
        f2 = (4*1024*1024)/((16>>1) << shift)
    else:
        f2 = (4*1024*1024)/((16 * div) << shift)
    return (f1, f2)

def main():
    for shift in range(16):
        for div in range(8):
            f1, f2 = get_freq(shift, div)
            if f1 != f2:
                print(f"problem with shift={shift} and div={div} (f1={f1}, f2={f2})")
            else:
                print(f"shift={shift:3d}, div={div:3d}, f={f1}Hz")
    return 0

if __name__ == "__main__":
    sys.exit(main())