import cffi
import os, sys
import json

def run_cpu_test(emulator, cpu_state, test):
    success = True
    (a, f, b, c, d, e, h, l, pc, sp) = cpu_state
    a[0] = int(test["initial"]["cpu"]["a"], 16)
    f[0] = int(test["initial"]["cpu"]["f"], 16)
    b[0] = int(test["initial"]["cpu"]["b"], 16)
    c[0] = int(test["initial"]["cpu"]["c"], 16)
    d[0] = int(test["initial"]["cpu"]["d"], 16)
    e[0] = int(test["initial"]["cpu"]["e"], 16)
    h[0] = int(test["initial"]["cpu"]["h"], 16)
    l[0] = int(test["initial"]["cpu"]["l"], 16)
    pc[0] = int(test["initial"]["cpu"]["pc"], 16)
    sp[0] = int(test["initial"]["cpu"]["sp"], 16)

    # print(test)

    # init cpu state
    emulator.cpu_setup(a[0], f[0], b[0], c[0], d[0], e[0], h[0], l[0], pc[0], sp[0])

    # init cpu memory
    for ramData in test["initial"]["ram"]:
        [address, data] = [int(x, 16) for x in ramData]
        emulator.cpu_set_memory(address, data)
        # readBack = emulator.cpu_get_memory(address)
        # print("Set: {:04x}: {:02x} (readBack={:02x})".format(address, data, readBack))

    # run test !
    emulator.cpu_tick()

    # get results
    emulator.cpu_get_state(a, f, b, c, d, e, h, l, pc, sp)

    # check ram results
    ram_check = []
    for ramData in test["final"]["ram"]:
        [address, expected_data] = [int(x, 16) for x in ramData]
        cpu_data = emulator.cpu_get_memory(address)
        ram_check.append((address, expected_data, cpu_data))
        # print("Get: {:04x}: {:02x} (exp: {:02x})".format(address, cpu_data, expected_data))
    
    if (a[0] != int(test["final"]["cpu"]["a"], 16)) or \
       (f[0] != int(test["final"]["cpu"]["f"], 16)) or \
       (b[0] != int(test["final"]["cpu"]["b"], 16)) or \
       (c[0] != int(test["final"]["cpu"]["c"], 16)) or \
       (d[0] != int(test["final"]["cpu"]["d"], 16)) or \
       (e[0] != int(test["final"]["cpu"]["e"], 16)) or \
       (h[0] != int(test["final"]["cpu"]["h"], 16)) or \
       (l[0] != int(test["final"]["cpu"]["l"], 16)) or \
       (pc[0] != int(test["final"]["cpu"]["pc"], 16)) or \
       (sp[0] != int(test["final"]["cpu"]["sp"], 16)):
        success = False
    
    for address, expected_data, cpu_data in ram_check:
        if expected_data != cpu_data:
            success = False
            break
    
    if not success:
        print()
        print(" a={:02x}, expected={:02x}".format(a[0], int(test["final"]["cpu"]["a"], 16)))
        print(" f={:02x}, expected={:02x}".format(f[0], int(test["final"]["cpu"]["f"], 16)))
        print(" b={:02x}, expected={:02x}".format(b[0], int(test["final"]["cpu"]["b"], 16)))
        print(" c={:02x}, expected={:02x}".format(c[0], int(test["final"]["cpu"]["c"], 16)))
        print(" d={:02x}, expected={:02x}".format(d[0], int(test["final"]["cpu"]["d"], 16)))
        print(" e={:02x}, expected={:02x}".format(e[0], int(test["final"]["cpu"]["e"], 16)))
        print(" h={:02x}, expected={:02x}".format(h[0], int(test["final"]["cpu"]["h"], 16)))
        print(" l={:02x}, expected={:02x}".format(l[0], int(test["final"]["cpu"]["l"], 16)))
        print("pc={:04x}, expected={:04x}".format(pc[0], int(test["final"]["cpu"]["pc"], 16)))
        print("sp={:04x}, expected={:04x}".format(sp[0], int(test["final"]["cpu"]["sp"], 16)))
        for address, expected_data, cpu_data in ram_check:
            print("RAM @{:04x}={:02x}, expected={:02x}".format(address, cpu_data, expected_data))

    return success

def load_tests(path, file):
    d = {}
    with open(path + "/" + file) as f:
        d = json.load(f)
    return d

def main():
    cpu_test_path = "sm83-test-data-master/cpu_tests/v1"
    so_file = ".release/emulator.dll"

    ffi = cffi.FFI()
    ffi.cdef("""
    uint8_t cpu_get_memory(uint16_t addr);
    """)
    ffi.cdef("""
    void cpu_set_memory(uint16_t addr, uint8_t val);
    """)
    ffi.cdef("""
    void cpu_setup(uint8_t a, uint8_t f, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t h, uint8_t l, uint16_t pc, uint16_t sp);
    """)
    ffi.cdef("""
    void cpu_get_state(uint8_t *a, uint8_t *f, uint8_t *b, uint8_t *c, uint8_t *d, uint8_t *e, uint8_t *h, uint8_t *l, uint16_t *pc, uint16_t *sp);
    """)
    ffi.cdef("""
    void cpu_tick(void);
    """)
    emulator = ffi.dlopen(so_file)

    a = ffi.new("uint8_t[1]")
    f = ffi.new("uint8_t[1]")
    b = ffi.new("uint8_t[1]")
    c = ffi.new("uint8_t[1]")
    d = ffi.new("uint8_t[1]")
    e = ffi.new("uint8_t[1]")
    h = ffi.new("uint8_t[1]")
    l = ffi.new("uint8_t[1]")
    pc = ffi.new("uint16_t[1]")
    sp = ffi.new("uint16_t[1]")
    cpu_state = (a, f, b, c, d, e, h, l, pc, sp)
    
    cpu_tests = os.listdir(cpu_test_path)
    
    print("ALU-Tests:")
    for fileName in cpu_tests[:]:
        test_list = load_tests(cpu_test_path, fileName)
        print("File: {}".format(fileName))
        for test_num, test in enumerate(test_list):
            print("Test {:>4d}/{:>4d}\r".format(test_num+1, len(test_list)), end="")
            sys.stdout.flush()
            if not run_cpu_test(emulator, cpu_state, test):
                print("\nTest:\n{}".format(test))
                return
        print()
    
if __name__ == "__main__":
    main()
