#ifdef _ECLIPSE_OPENCL_HEADER
#   include "OpenCLKernel.hpp"
#endif

__constant static const uint IV512[] = {
	(0x8807a57e), (0xe616af75), (0xc5d3e4db),
	(0xac9ab027), (0xd915f117), (0xb6eecc54),
	(0x06e8020b), (0x4a92efd1), (0xaac6e2c9),
	(0xddb21398), (0xcae65838), (0x437f203f),
	(0x25ea78e7), (0x951fddd6), (0xda6ed11d),
	(0xe13e3567)
};

typedef struct {
	uint partial;
	uint partial_len;
	uint round_shift;
	uint S[36];
	ulong bit_count;
} metis_context;

void metis_init(metis_context* sc) {
	size_t u;

	for (u = 0; u < 20; u ++)
		sc->S[u] = 0;
	for (int i = 0; i < 16; i++) {
		sc->S[20+i] = IV512[i];
	}
	sc->partial = 0;
	sc->partial_len = 0;
	sc->round_shift = 0;
	sc->bit_count = 0;
}

void metis4_core(metis_context *sc, const void *data, size_t len)
{
	uint S00, S01, S02, S03, S04, S05, S06, S07, S08, S09;
	uint S10, S11, S12, S13, S14, S15, S16, S17, S18, S19;
	uint S20, S21, S22, S23, S24, S25, S26, S27, S28, S29;
	uint S30, S31, S32, S33, S34, S35;

	uint p;
	unsigned plen, rshift;
	do {
		sc->bit_count += (ulong)len << 3;
	} while (0);
	p = sc->partial;
	plen = sc->partial_len;
	if (plen < 4) {
		unsigned count = 4 - plen;
		if (len < count)
			count = len;
		plen += count;
		while (count -- > 0) {
			p = (p << 8) | *(const unsigned char *)data;
			data = (const unsigned char *)data + 1;
			len --;
		}
		if (len == 0) {
			sc->partial = p;
			sc->partial_len = plen;
			return;
		}
	}

	S00 = (sc)->S[ 0];
	S01 = (sc)->S[ 1];
	S02 = (sc)->S[ 2];
	S03 = (sc)->S[ 3];
	S04 = (sc)->S[ 4];
	S05 = (sc)->S[ 5];
	S06 = (sc)->S[ 6];
	S07 = (sc)->S[ 7];
	S08 = (sc)->S[ 8];
	S09 = (sc)->S[ 9];
	S10 = (sc)->S[10];
	S11 = (sc)->S[11];
	S12 = (sc)->S[12];
	S13 = (sc)->S[13];
	S14 = (sc)->S[14];
	S15 = (sc)->S[15];
	S16 = (sc)->S[16];
	S17 = (sc)->S[17];
	S18 = (sc)->S[18];
	S19 = (sc)->S[19];
	S20 = (sc)->S[20];
	S21 = (sc)->S[21];
	S22 = (sc)->S[22];
	S23 = (sc)->S[23];
	S24 = (sc)->S[24];
	S25 = (sc)->S[25];
	S26 = (sc)->S[26];
	S27 = (sc)->S[27];
	S28 = (sc)->S[28];
	S29 = (sc)->S[29];
	S30 = (sc)->S[30];
	S31 = (sc)->S[31];
	S32 = (sc)->S[32];
	S33 = (sc)->S[33];
	S34 = (sc)->S[34];
	S35 = (sc)->S[35];

	rshift = sc->round_shift;
	switch (rshift) {
		for (;;) {
			sph_u32 q;

		case 0:
			q = p;
			TIX4(q, S00, S01, S04, S07, S08, S22, S24, S27, S30);
			CMIX36(S33, S34, S35, S01, S02, S03, S15, S16, S17);
			SMIX(S33, S34, S35, S00);
			CMIX36(S30, S31, S32, S34, S35, S00, S12, S13, S14);
			SMIX(S30, S31, S32, S33);
			CMIX36(S27, S28, S29, S31, S32, S33, S09, S10, S11);
			SMIX(S27, S28, S29, S30);
			CMIX36(S24, S25, S26, S28, S29, S30, S06, S07, S08);
			SMIX(S24, S25, S26, S27);
			NEXT(1);
			/* fall through */
		case 1:
			q = p;
			TIX4(q, S24, S25, S28, S31, S32, S10, S12, S15, S18);
			CMIX36(S21, S22, S23, S25, S26, S27, S03, S04, S05);
			SMIX(S21, S22, S23, S24);
			CMIX36(S18, S19, S20, S22, S23, S24, S00, S01, S02);
			SMIX(S18, S19, S20, S21);
			CMIX36(S15, S16, S17, S19, S20, S21, S33, S34, S35);
			SMIX(S15, S16, S17, S18);
			CMIX36(S12, S13, S14, S16, S17, S18, S30, S31, S32);
			SMIX(S12, S13, S14, S15);
			NEXT(2);
			/* fall through */
		case 2:
			q = p;
			TIX4(q, S12, S13, S16, S19, S20, S34, S00, S03, S06);
			CMIX36(S09, S10, S11, S13, S14, S15, S27, S28, S29);
			SMIX(S09, S10, S11, S12);
			CMIX36(S06, S07, S08, S10, S11, S12, S24, S25, S26);
			SMIX(S06, S07, S08, S09);
			CMIX36(S03, S04, S05, S07, S08, S09, S21, S22, S23);
			SMIX(S03, S04, S05, S06);
			CMIX36(S00, S01, S02, S04, S05, S06, S18, S19, S20);
			SMIX(S00, S01, S02, S03);
			NEXT(0);
		}
	}

	p = 0;
	sc->partial_len = (unsigned)len;
	while (len -- > 0) {
		p = (p << 8) | *(const unsigned char *)data;
		data = (const unsigned char *)data + 1;
	}
	sc->partial = p;
	sc->round_shift = rshift;

	WRITE_STATE_BIG(sc);
}

kernel void metis512(global ulong * in, global ulong * out) {

	metis_context ctx;
	metis_init(&ctx);

	int id = get_global_id(0);
	for (int i = 0; i < 8; i++) {
		out[i] = in[i];
	}
}
