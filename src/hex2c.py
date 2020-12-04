from sys import stdin
ol = []
for ll in stdin.readlines():
    ll = ll.strip()
    if len(ll) > 3 and ll[0] == ":":
        xx = zip(ll[1::2], ll[2::2])
        hx = "      \"" + "".join(["\\x%s%s" % tuple(x) for x in xx]) + "\""
        # print(hx)
        ol += [hx]
print("   uint8_t *dd[] = {")
print(",\n".join(ol))
print("   };")
