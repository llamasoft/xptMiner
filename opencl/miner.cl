#ifdef _ECLIPSE_OPENCL_HEADER
#   include "OpenCLKernel.hpp"
#   include "keccak.cl"
#   include "shavite.cl"
#   include "metis.cl"
#   include "OpenCLKernel.hpp"
#endif

kernel void metiscoin_process(global char* in, global ulong* out, uint begin_nonce) {

	size_t id = get_global_id(0);
	uint nonce = (uint)id + begin_nonce;

	keccak_context	 ctx_keccak;
	shavite_context ctx_shavite;
	metis_context ctx_metis;
	char data[80];
	ulong hash0[8];
	ulong hash1[8];
	ulong hash2[8];

	// prepares data
	for (int i = 0; i < 80; i++) {
		data[i] = in[i];
	}
	char * p = (char*)&nonce;
	for (int i = 0; i < 4; i++) {
		data[76+i] = p[i];
	}

	// keccak
	keccak_init(&ctx_keccak);
	keccak_core(&ctx_keccak, data, 80);
	keccak_close(&ctx_keccak, hash0);

	// shavite
	shavite_init(&ctx_shavite);
	shavite_core(&ctx_shavite, hash0, 64);
	shavite_close(&ctx_shavite, hash1);

	// metis
	metis_init(&ctx_metis);
	metis_core(&ctx_metis, hash1, 64);
	metis_close(&ctx_metis, hash2);

	// copys out
	for (int i = 0; i < 8; i++) {
		out[i+(8*id)] = hash2[i];
	}

}
