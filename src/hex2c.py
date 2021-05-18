from sys import stdin
ol = []
fl = []
flash = True
for ll in stdin.readlines():
    ll = ll.strip()
    if len(ll) > 3 and ll[0] == ":":
        xx = zip(ll[1::2], ll[2::2])
        h1 = "".join(["\\x%s%s" % tuple(x) for x in xx])
        hx = "      \"" + h1 + "\""
        # print(hx)
        ol += [hx]
        # simple heuristics for now, could be made more precese
        if len(h1) == 84 and h1[:7] == "\\x10\\x0" and h1[12:16] == "\\x00":
            hx = "      \"" + h1[16:-4] + "\""
            fl += [hx]
        elif h1 != "\\x00\\x00\\x00\\x01\\xFF":
            flash = False
if flash:
    # destined for xrp_flash() in i2c_pm.c
    print("   const uint8_t dd[] = {")
    print("\n".join(fl))
    print("   };")
else:
    # destined for xrp_program_static() in hexrec.c
    print("   const char *dd[] = {")
    print(",\n".join(ol))
    print("   };")
