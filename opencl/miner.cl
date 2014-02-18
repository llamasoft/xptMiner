#include "common.cl"

kernel void
metiscoin_process(constant ulong* u,
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
/*
    uint nonce = begin_nonce + get_global_id(0);

    keccak_context  ctx_keccak;
    metis_context   ctx_metis;
    ulong hash_temp[8];

    // Copy all lookup tables to local memory
    // Requires at least (8 * 256 * 4) bytes = 8 kb
    local uint SHAVITE_LOOKUP0[256];
    local uint SHAVITE_LOOKUP1[256];
    local uint SHAVITE_LOOKUP2[256];
    local uint SHAVITE_LOOKUP3[256];
    local uint METIS_LOOKUP0[256];
    local uint METIS_LOOKUP1[256];
    local uint METIS_LOOKUP2[256];
    local uint METIS_LOOKUP3[256];
    event_t e;
    e = async_work_group_copy(SHAVITE_LOOKUP0, AES0,    256, 0);
    e = async_work_group_copy(SHAVITE_LOOKUP1, AES1,    256, e);
    e = async_work_group_copy(SHAVITE_LOOKUP2, AES2,    256, e);
    e = async_work_group_copy(SHAVITE_LOOKUP3, AES3,    256, e);
    e = async_work_group_copy(METIS_LOOKUP0,   mixtab0, 256, e);
    e = async_work_group_copy(METIS_LOOKUP1,   mixtab1, 256, e);
    e = async_work_group_copy(METIS_LOOKUP2,   mixtab2, 256, e);
    e = async_work_group_copy(METIS_LOOKUP3,   mixtab3, 256, e);
    wait_group_events(1, e);


    // keccak (resume from passed state)
    #pragma unroll
    for (ushort i = 0; i < 4; i++) { ctx_keccak.buf[i] = buff[i]; }
    #pragma unroll
    for (int i = 0; i < 25; i++) { ctx_keccak.wide[i] = u[i]; }
    *((uint*)(ctx_keccak.buf+4)) = nonce;
    keccak_close(&ctx_keccak, hash_temp);

    // shavite
    //shavite_init(&ctx_shavite);
    //shavite_core_64(&ctx_shavite, hash_temp);
    shavite(hash_temp,
            SHAVITE_LOOKUP0,
            SHAVITE_LOOKUP1,
            SHAVITE_LOOKUP2,
            SHAVITE_LOOKUP3);

    // metis
    metis_init(&ctx_metis);
    metis_core_and_close(&ctx_metis, (uchar *)hash_temp, hash_temp,
                         METIS_LOOKUP0,
                         METIS_LOOKUP1,
                         METIS_LOOKUP2,
                         METIS_LOOKUP3);

    if( *(uint*)((uchar*)hash_temp+28) <= target )
    {
        out[atomic_inc(outcount)] = nonce;
    }
*/
}


kernel void 
keccak_step_noinit(constant const ulong* u,
                   constant const char* buff,
                   global ulong* restrict out,
                   uint begin_nonce)
{
    size_t id = get_global_id(0);
    uint nonce = (uint)id + begin_nonce;
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


kernel __attribute__(( vec_type_hint(uchar4) )) void 
shavite_step(global ulong* in_out,
             global uint*  restrict AES0,
             global uint*  restrict AES1,
             global uint*  restrict AES2,
             global uint*  restrict AES3)
{
    size_t id = get_global_id(0);
    ulong hash[8];

    // prepares data
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        hash[i] = in_out[(id * 8)+i];
    }

    // Copy global lookup table into local memory
    local uint SHAVITE_LOOKUP0[256];
    local uint SHAVITE_LOOKUP1[256];
    local uint SHAVITE_LOOKUP2[256];
    local uint SHAVITE_LOOKUP3[256];
    event_t e;
    e = async_work_group_copy(SHAVITE_LOOKUP0, AES0, 256, 0);
    e = async_work_group_copy(SHAVITE_LOOKUP1, AES1, 256, e);
    e = async_work_group_copy(SHAVITE_LOOKUP2, AES2, 256, e);
    e = async_work_group_copy(SHAVITE_LOOKUP3, AES3, 256, e);
    wait_group_events(1, &e);

    //shavite_init(&ctx_shavite);
    //shavite_core_64(&ctx_shavite, hash);
    shavite((uint *)hash,
            SHAVITE_LOOKUP0,
            SHAVITE_LOOKUP1,
            SHAVITE_LOOKUP2,
            SHAVITE_LOOKUP3);

    #pragma unroll
    for (int i = 0; i < 8; i++) {
        in_out[(id * 8)+i] = hash[i];
    }
}

kernel  __attribute__(( vec_type_hint(uchar4) )) void 
metis_step(global ulong* in,
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

    ulong hash[8];

    // prepares data
    for (int i = 0; i < 8; i++) {
        hash[i] = in[(id * 8)+i];
    }

    // Copy global lookup table into local memory
    local uint local_mixtab0[256];
    local uint local_mixtab1[256];
    local uint local_mixtab2[256];
    local uint local_mixtab3[256];
    event_t e;
    e = async_work_group_copy(local_mixtab0, mixtab0, 256, 0);
    e = async_work_group_copy(local_mixtab1, mixtab1, 256, e);
    e = async_work_group_copy(local_mixtab2, mixtab2, 256, e);
    e = async_work_group_copy(local_mixtab3, mixtab3, 256, e);
    wait_group_events(1, &e);


    metis((uint *)hash,
          local_mixtab0,
          local_mixtab1,
          local_mixtab2,
          local_mixtab3);

    if( *(uint*)((uchar*)hash + 28) <= target )
    {
        out[atomic_inc(outcount)] = nonce;
    }

}
