from PIL import Image

data = [0x3C, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x5E, 0x7E, 0x0A, 0x7C, 0x56, 0x38, 0x7C]

palette = [(255,255,255), (170,170,170), (85,85,85), (0,0,0)]
# palette = [(0x26,0x34,0x23), (0x53,0x70,0x2f), (0xa6,0xb6,0x3d), (0xf1,0xf3,0xc0)]

def main():
    tile = []
    for i in range(0, len(data), 2):
        lsb = data[i + 0]
        msb = data[i + 1]
        for j in range(7,-1,-1):
            t = 0
            t += 1 if (0 != lsb & (1<<j)) else 0
            t += 2 if (0 != msb & (1<<j)) else 0
            tile.append(t)
        #     print(t, end="")
        # print()
    
    img = Image.new("RGB", (8, 8), color="red")
    pixels = img.load()

# Set the pixel colors from top-left to bottom-right
    for y in range(8):
        for x in range(8):
            pixels[x, y] = palette[tile[y*8+x]]

    img = img.resize((800, 800), Image.NEAREST)
    img.show()

if __name__ == "__main__":
    main()