#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

#define HASH_SIZE (2)
static int spot_check1(uint32_t local_hash[], uint32_t desired_mac[])
{
	const unsigned char *tkey = (const unsigned char *) "super secret key";
	uint32_t result_mac[HASH_SIZE];
	int ok;
	core_siphash((unsigned char *) result_mac, (unsigned char *) local_hash, 8, tkey);
	ok = desired_mac[0] == result_mac[0] && desired_mac[1] == result_mac[1];
	printf("desired_mac = %8.8lx %8.8lx %s\r\n", desired_mac[0], desired_mac[1], ok ? "OK" : "BAD");
	return !ok;
}

static int spot_check(void)
{
	uint32_t local_hash[HASH_SIZE], desired_mac[HASH_SIZE];
	int rc = 0;
	local_hash[0] = 0x00000000;  local_hash[1] = 0x00000000;
	desired_mac[0] = 0x526ea2cd;  desired_mac[1] = 0x9447a3dc;
	if (spot_check1(local_hash, desired_mac)) rc = 1;
	local_hash[0] = 0xd8612c21;  local_hash[1] = 0xbd33c82e;
	desired_mac[0] = 0xd40e5c3d; desired_mac[1] = 0x0056a781;
	if (spot_check1(local_hash, desired_mac)) rc = 1;
	local_hash[0] = 0x6cf3f85b;  local_hash[1] = 0xfd51f6ad;
	desired_mac[0] = 0x483a1f3a; desired_mac[1] = 0x0f8045fb;
	if (spot_check1(local_hash, desired_mac)) rc = 1;
	return rc;
}

static void usage(void)
{
	printf("Usage: refsip {sign, check}\n  I/O through stdin and stdout\n");
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
