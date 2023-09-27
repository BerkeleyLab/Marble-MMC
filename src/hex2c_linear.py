from sys import stdin
ol = []
olda = None
for ll in stdin.readlines():
    ll = ll.strip()
    if len(ll) > 3 and ll[0:3] == ":10":
        xlen = int(ll[1:3], 16)
        xadd = int(ll[3:7], 16)
        xtyp = ll[7:9]
        ll = ll[9:-2]
        slen = int(len(ll)/2)
        # print("len %d add 0x%4.4x typ %s data len %d" % (xlen, xadd, xtyp, slen))
        if xlen != slen:
            print("length mismatch %d != %d: aborting!" % (xlen, slen))
            exit(1)
        if olda is None:
            # print("a set 0x%4.4x" % xadd)
            olda = xadd
            firsta = xadd
        else:
            if olda != xadd:
                print("check 0x%4.4x == 0x%4.4x" % (olda, xadd))
                exit(1)
        xx = zip(ll[0::2], ll[1::2])
        hx = "      \"" + "".join(["\\x%s%s" % tuple(x) for x in xx]) + "\""
        # print(hx)
        ol += [hx]
        olda += slen
print("// addresses 0x%4.4x to 0x%4.4x" % (firsta, olda-1))
print("   uint8_t dd[] = {")
print("\n".join(ol))
print("   };")
