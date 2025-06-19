
def main():
    
    lfsr = 0

    for i in range(100000):
        #do xnor
        a = (0 != (lfsr & (1<<0)))
        b = (0 != (lfsr & (1<<1)))
        xnor_result = not(a ^ b)
        
        # copy
        msk = (1 << 15)# | ((!!lfsr_length_7Bit) << 7)
        lfsr = (lfsr | msk) if xnor_result else (lfsr & ~msk)
        lfsr >>= 1

        print(lfsr&1, end="")

if __name__ == "__main__":
    main()