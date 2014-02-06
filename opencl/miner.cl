#ifdef _ECLIPSE_OPENCL_HEADER
#   include "OpenCLKernel.hpp"
#   include "keccak.cl"
#   include "shavite.cl"
#   include "metis.cl"
#   include "OpenCLKernel.hpp"
#endif

kernel void metiscoin_process(constant  const   ulong*  u,
                              constant  const   char*   buff,
                              global            uint*   out,
                              global            uint*   outcount,
                                                uint    begin_nonce,
                                                uint    hashes_per_worker,
                                                uint    target) {

	uint nonce = begin_nonce + (get_global_id(0) * hashes_per_worker);

    keccak_context  ctx_keccak;
	shavite_context ctx_shavite;
	metis_context   ctx_metis;
    ulong hash_temp[8];
    
    ctx_keccak.lim = 72;
    ctx_keccak.ptr = 8;
    
    for (uint pass = 0; pass < hashes_per_worker; ++pass) {
        // keccak (resume from passed state)
        #pragma unroll
        for (ushort i = 0; i < 4; i++) { ctx_keccak.buf[i] = buff[i]; }
        #pragma unroll
        for (int i = 0; i < 25; i++) { ctx_keccak.u.wide[i] = u[i]; }
        *((uint*)(ctx_keccak.buf+4)) = nonce + pass;
        keccak_close(&ctx_keccak, hash_temp);

        // shavite
        shavite_init(&ctx_shavite);
        // shavite_core_and_close(&ctx_shavite, hash_temp, hash_temp);

        // metis
        metis_init(&ctx_metis);
        // metis_core_and_close(&ctx_metis, hash_temp, hash_temp);

        if( *(uint*)((uchar*)hash_temp+28) <= target )
        {
            uint pos = atomic_inc(outcount); //saves first pos for counter
            out[pos] = nonce + pass;
        }
    }
}


kernel void keccak_step_noinit(constant const ulong* u, constant const char* buff, global ulong* out, uint begin_nonce) {

	size_t id = get_global_id(0);
	uint nonce = (uint)id + begin_nonce;
	uint hnonce = nonce / 0x8000;
	uint lnonce = nonce % 0x8000;
	nonce = hnonce * 0x10000 + lnonce;

	ulong hash[8];

	// inits context
	keccak_context	 ctx_keccak;
	ctx_keccak.lim = 72;
	ctx_keccak.ptr = 8;
#pragma unroll
	for (int i = 0; i < 4; i++) {
		ctx_keccak.buf[i] = buff[i];
	}
	*((uint*)(ctx_keccak.buf+4)) = nonce;
#pragma unroll
	for (int i = 0; i < 25; i++) {
		ctx_keccak.u.wide[i] = u[i];
	}

	// keccak
	keccak_close(&ctx_keccak, hash);

#pragma unroll
	for (int i = 0; i < 8; i++) {
		out[(id * 8)+i] = hash[i];
	}
}


kernel void shavite_step(global ulong* in_out,
                         global uint*  AES0,
                         global uint*  AES1,
                         global uint*  AES2,
                         global uint*  AES3
                         )
{
	size_t id = get_global_id(0);

	shavite_context	 ctx_shavite;
	ulong hash0[8];
	ulong hash1[8];

	// prepares data
	for (int i = 0; i < 8; i++) {
		hash0[i] = in_out[(id * 8)+i];
	}
    
    // Copy global lookup table into local memory
    size_t qty = 256;
    local uint SHAVITE_LOOKUP0[256];
    local uint SHAVITE_LOOKUP1[256];
    local uint SHAVITE_LOOKUP2[256];
    local uint SHAVITE_LOOKUP3[256];
    event_t e[4];
    e[0] = async_work_group_copy(SHAVITE_LOOKUP0, AES0, qty, 0);
    e[1] = async_work_group_copy(SHAVITE_LOOKUP1, AES1, qty, 0);
    e[2] = async_work_group_copy(SHAVITE_LOOKUP2, AES2, qty, 0);
    e[3] = async_work_group_copy(SHAVITE_LOOKUP3, AES3, qty, 0);
    wait_group_events(4, e);

	shavite_init(&ctx_shavite);
    shavite_core_64(&ctx_shavite, hash0);
	shavite_close(&ctx_shavite, hash1,
                  SHAVITE_LOOKUP0,
                  SHAVITE_LOOKUP1,
                  SHAVITE_LOOKUP2,
                  SHAVITE_LOOKUP3
                 );

	for (int i = 0; i < 8; i++) {
		in_out[(id * 8)+i] = hash1[i];
	}
}

kernel void metis_step(global ulong* in,
                       global uint*  out,
                       global uint*  outcount,
                              uint   begin_nonce,
                              uint   target,
                       global uint*  mixtab0,
                       global uint*  mixtab1,
                       global uint*  mixtab2,
                       global uint*  mixtab3
                       )
{
	size_t id = get_global_id(0);
	uint nonce = (uint)id + begin_nonce;
	uint hnonce = nonce / 0x8000;
	uint lnonce = nonce % 0x8000;
	nonce = hnonce * 0x10000 + lnonce;

	metis_context ctx_metis;
	ulong hash0[8];
	ulong hash1[8];

	// prepares data
	for (int i = 0; i < 8; i++) {
		hash0[i] = in[(id * 8)+i];
	}

    // Copy global lookup table into local memory
    size_t qty = 256;
    local uint METIS_LOOKUP0[256];
    local uint METIS_LOOKUP1[256];
    local uint METIS_LOOKUP2[256];
    local uint METIS_LOOKUP3[256];
    event_t e[4];
    e[0] = async_work_group_copy(METIS_LOOKUP0, mixtab0, qty, 0);
    e[1] = async_work_group_copy(METIS_LOOKUP1, mixtab1, qty, 0);
    e[2] = async_work_group_copy(METIS_LOOKUP2, mixtab2, qty, 0);
    e[3] = async_work_group_copy(METIS_LOOKUP3, mixtab3, qty, 0);
    wait_group_events(4, e);

    
    metis_init(&ctx_metis);
    metis_core_and_close(&ctx_metis, hash0, hash1,
                         METIS_LOOKUP0,
                         METIS_LOOKUP1,
                         METIS_LOOKUP2,
                         METIS_LOOKUP3);


	if( *(uint*)((uchar*)hash1+28) <= target )
	{
		uint pos = atomic_inc(outcount); //saves first pos for counter
		out[pos] = nonce;
	}

}
