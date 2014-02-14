#include "common.cl"

kernel void metiscoin_process(constant ulong* u,
                              constant char*  buff,
                              global   uint*  out,
                              global   uint*  outcount,
                                       uint   begin_nonce,
                                       uint   target,
                              global   uint*  restrict AES0,
                              global   uint*  restrict AES1,
                              global   uint*  restrict AES2,
                              global   uint*  restrict AES3,
                              global   uint*  restrict mixtab0,
                              global   uint*  restrict mixtab1,
                              global   uint*  restrict mixtab2,
                              global   uint*  restrict mixtab3)
{
    uint nonce = begin_nonce + get_global_id(0);

    keccak_context  ctx_keccak;
    shavite_context ctx_shavite;
    metis_context   ctx_metis;
    ulong hash_temp[8];

    // Copy all lookup tables to local memory
    // Requires at least (8 * 256 * 4) bytes = 8 kb
    size_t qty = 256;
    local uint SHAVITE_LOOKUP0[256];
    local uint SHAVITE_LOOKUP1[256];
    local uint SHAVITE_LOOKUP2[256];
    local uint SHAVITE_LOOKUP3[256];
    local uint METIS_LOOKUP0[256];
    local uint METIS_LOOKUP1[256];
    local uint METIS_LOOKUP2[256];
    local uint METIS_LOOKUP3[256];
    event_t e[8];
    e[0] = async_work_group_copy(SHAVITE_LOOKUP0, AES0,    qty, 0);
    e[1] = async_work_group_copy(SHAVITE_LOOKUP1, AES1,    qty, 0);
    e[2] = async_work_group_copy(SHAVITE_LOOKUP2, AES2,    qty, 0);
    e[3] = async_work_group_copy(SHAVITE_LOOKUP3, AES3,    qty, 0);
    e[4] = async_work_group_copy(METIS_LOOKUP0,   mixtab0, qty, 0);
    e[5] = async_work_group_copy(METIS_LOOKUP1,   mixtab1, qty, 0);
    e[6] = async_work_group_copy(METIS_LOOKUP2,   mixtab2, qty, 0);
    e[7] = async_work_group_copy(METIS_LOOKUP3,   mixtab3, qty, 0);
    wait_group_events(8, e);


    // keccak (resume from passed state)
    #pragma unroll
    for (ushort i = 0; i < 4; i++) { ctx_keccak.buf[i] = buff[i]; }
    #pragma unroll
    for (int i = 0; i < 25; i++) { ctx_keccak.wide[i] = u[i]; }
    *((uint*)(ctx_keccak.buf+4)) = nonce;
    keccak_close(&ctx_keccak, hash_temp);

    // shavite
    shavite_init(&ctx_shavite);
    shavite_core_64(&ctx_shavite, hash_temp);
    shavite_close(&ctx_shavite, hash_temp,
                  SHAVITE_LOOKUP0,
                  SHAVITE_LOOKUP1,
                  SHAVITE_LOOKUP2,
                  SHAVITE_LOOKUP3);

    // metis
    metis_init(&ctx_metis);
    metis_core_and_close(&ctx_metis, hash_temp, hash_temp,
                         METIS_LOOKUP0,
                         METIS_LOOKUP1,
                         METIS_LOOKUP2,
                         METIS_LOOKUP3);

    if( *(uint*)((uchar*)hash_temp+28) <= target )
    {
        out[atomic_inc(outcount)] = nonce;
    }
}


kernel void keccak_step_noinit(constant const ulong* u, constant const char* buff, global ulong* restrict out, uint begin_nonce) {

    size_t id = get_global_id(0);
    uint nonce = (uint)id + begin_nonce;
    uint hnonce = nonce / 0x8000;
    uint lnonce = nonce % 0x8000;
    nonce = hnonce * 0x10000 + lnonce;

    ulong hash[8];

    // inits context
    keccak_context	 ctx_keccak;
#pragma unroll
    for (int i = 0; i < 4; i++) {
        ctx_keccak.buf[i] = buff[i];
    }
    *((uint*)(ctx_keccak.buf+4)) = nonce;
#pragma unroll
    for (int i = 0; i < 25; i++) {
        ctx_keccak.wide[i] = u[i];
    }

    // keccak
    keccak_close(&ctx_keccak, hash);

#pragma unroll
    for (int i = 0; i < 8; i++) {
        out[(id * 8)+i] = hash[i];
    }
}


kernel void shavite_step(global ulong* in_out,
                         global uint*  restrict AES0,
                         global uint*  restrict AES1,
                         global uint*  restrict AES2,
                         global uint*  restrict AES3)
{
    size_t id = get_global_id(0);

    shavite_context	 ctx_shavite;
    ulong hash[8];

    // prepares data
    for (int i = 0; i < 8; i++) {
        hash[i] = in_out[(id * 8)+i];
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
    shavite_core_64(&ctx_shavite, hash);
    shavite_close(&ctx_shavite, hash,
                  SHAVITE_LOOKUP0,
                  SHAVITE_LOOKUP1,
                  SHAVITE_LOOKUP2,
                  SHAVITE_LOOKUP3);

    for (int i = 0; i < 8; i++) {
        in_out[(id * 8)+i] = hash[i];
    }
}

kernel void metis_step(global ulong* in,
                       global uint*  out,
                       global uint*  outcount,
                              uint   begin_nonce,
                              uint   target,
                       global uint*  restrict mixtab0,
                       global uint*  restrict mixtab1,
                       global uint*  restrict mixtab2,
                       global uint*  restrict mixtab3)
{
    size_t id = get_global_id(0);
    uint nonce = (uint)id + begin_nonce;
    uint hnonce = nonce / 0x8000;
    uint lnonce = nonce % 0x8000;
    nonce = hnonce * 0x10000 + lnonce;

    metis_context ctx_metis;
    ulong hash[8];

    // prepares data
    for (int i = 0; i < 8; i++) {
        hash[i] = in[(id * 8)+i];
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
    metis_core_and_close(&ctx_metis, hash, hash,
                         METIS_LOOKUP0,
                         METIS_LOOKUP1,
                         METIS_LOOKUP2,
                         METIS_LOOKUP3);


    if( *(uint*)((uchar*)hash+28) <= target )
    {
        out[atomic_inc(outcount)] = nonce;
    }

}
