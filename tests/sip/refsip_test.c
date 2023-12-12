#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "refsip.h"


// Note that the following code, like the Verilog that it's intended
// to test and be compatible with, does _not_ include the padding and
// length encoding that's part of standard siphash.  Padding and length
// encoding _must_ be added elsewhere in the stack for the final system
// to have a chance of being secure.

static int sip_sign(const unsigned char *key)
{
	int c;
	unsigned int ix=0;
	unsigned char msg[1024], hash[8];
	while (EOF != (c=getchar())) {
		printf("%2.2x\n", c);
		if (ix<sizeof msg) msg[ix++] = c;
	}
	if (ix%8 != 0) {
		printf("error: byte count not a mult of 8: %d\n", ix);
		return 1;
	}
	core_siphash(hash, msg, ix, key);
	for (unsigned jx=0; jx<8; jx++) printf("%2.2x\n", hash[jx]);
	return 0;
}

static int sip_check(const unsigned char *key)
{
	char lbuff[4];
	unsigned char mbuff[100];
	unsigned char hash_in[8], hash_out[8];
	unsigned int mx=0;
	int fail;
	while (fgets(lbuff, 4, stdin)) {
		if (lbuff[2] == '\n') {
			unsigned int v;
			v = strtoul(lbuff, NULL, 16);
			// printf("found 0x%2.2x\n", v);
			mbuff[mx++] = v;
			if (mx >= sizeof mbuff) return 2;
		}
	}
	// Last 8 bytes of message are hash
	if (mx < 8) return 2;
	mx -= 8;
	memcpy(hash_in, mbuff+mx, 8);
	mbuff[mx] = '\0';
	printf("string (%s)\n", mbuff);
	core_siphash(hash_out, mbuff, mx, key);
	printf("Rx hash      ");
	for (unsigned jx=0; jx<8; jx++) printf("%2.2x", hash_in[jx]);
	printf("\n");
	fail = memcmp(hash_in, hash_out, 8) != 0;
	printf(fail ? "FAIL\n" : "PASS\n");
	return fail;
}

static void print64(const char *header, const uint8_t *data, unsigned len)
{
	printf(header);
	for (unsigned jx=0; jx<len; jx++) printf("%2.2x", data[jx]);
	printf("\r\n");
}

#define HASH_SIZE (8)
static int spot_check1(const unsigned char *local_nonce, const unsigned char *desired_mac)
{
	const unsigned char *tkey = (const unsigned char *) "super secret key";
	unsigned char result_mac[HASH_SIZE];
	int ok=1;
	print64("in   ", local_nonce, HASH_SIZE);
	print64("want ", desired_mac, HASH_SIZE);
	core_siphash(result_mac, local_nonce, HASH_SIZE, tkey);
	print64("got  ", result_mac, HASH_SIZE);
	for (unsigned int n = 0; n < HASH_SIZE; n++) {
		ok &= (result_mac[n] == desired_mac[n]);
	}
	printf("%s\n", ok ? "OK" : "BAD");
	return !ok;
}

#define HEX(x) (isdigit(x) ? (x)-'0' : islower(x) ? (x)-'a'+10 : isupper(x) ? (x)-'A'+10 : 0)

static int unhexlify(unsigned char *bytes, const char *hex, unsigned int len)
{
	unsigned int ix;
	if (!bytes || !hex) return 1;
	for (ix=0; ix<len; ix++) {
		if (hex[2*ix] == '\0' || hex[2*ix+1] == '\0') break;
		*bytes++ = HEX(hex[2*ix]) << 4 | HEX(hex[2*ix+1]);
	}
	return (ix<len);
}

static int spot_check(void)
{
	unsigned char local_nonce[HASH_SIZE], desired_mac[HASH_SIZE];
	int rc = 0;
	unhexlify(local_nonce, "0000000000000000", HASH_SIZE);
	unhexlify(desired_mac, "cda26e52dca34794", HASH_SIZE);
	if (spot_check1(local_nonce, desired_mac)) rc = 1;
	unhexlify(local_nonce, "fd51f6ad6cf3f85b", HASH_SIZE);
	unhexlify(desired_mac, "3a1f3a48fb45800f", HASH_SIZE);
	if (spot_check1(local_nonce, desired_mac)) rc = 1;
	unhexlify(local_nonce, "8b8ec08cfce55e6f", HASH_SIZE);
	unhexlify(desired_mac, "8e763fc0499f581c", HASH_SIZE);
	if (spot_check1(local_nonce, desired_mac)) rc = 1;
	return rc;
}

static void usage(void)
{
	printf("Usage: refsip {sign, check, spot}\n  I/O through stdin and stdout\n");
}

int main(int argc, char *argv[])
{
	int rc=0;
	unsigned char key[16];
	for (unsigned u=0; u<16; u++) key[u] = u;
	if (argc > 1 && !strcmp(argv[1], "sign")) rc = sip_sign(key);
	else if (argc > 1 && !strcmp(argv[1], "check")) rc = sip_check(key);
	else if (argc > 1 && !strcmp(argv[1], "spot")) rc = spot_check();
	else usage();
	return rc;
}
