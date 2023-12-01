# Magnificently crude PoC watchdog server
# Delete me the first chance you get!
# while true; do python3 -m mboxexchange -i $IP MB8_WD_HASH_L $(python3 -m decodembox -i $IP | python3 -m nonce_xfer); sleep 8; done
from sys import stdin
for ll in stdin.readlines():
    if "WD_NONCE" in ll:
        nn = ll.rstrip().split()[-1]
        if nn.startswith("0x"):
            nn = nn[2:]
        nn = "0"*(8-len(nn)) + nn
        rev = nn[6:8] + nn[4:6] + nn[2:4] + nn[0:2]
        print("0x" + rev)
