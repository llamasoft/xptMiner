#include"global.h"
#include "OpenCLObjects.h"

void metiscoin_init_opencl(int device_num) {
	printf("Initializing GPU %d\n", device_num);
	OpenCLMain &main = OpenCLMain::getInstance();
	std::vector<std::string> files;
	files.push_back("opencl/metis.cl");
	OpenCLProgram* program = main.getDevice(0)->getContext()->loadProgramFromFiles(files);
	OpenCLKernel* kernel = program->getKernel("metis512");
	main.listDevices();
}

void metiscoin_process(minerMetiscoinBlock_t* block)
{
	sph_keccak512_context	 ctx_keccak;
	sph_shavite512_context	 ctx_shavite;
	sph_metis512_context	 ctx_metis;
	static unsigned char pblank[1];
	block->nonce = 0;

	uint32 target = *(uint32*)(block->targetShare+28);
	uint64 hash0[8];
	uint64 hash1[8];
	uint64 hash2[8];
	uint64 hash2_2[8];

	OpenCLKernel* kernel = OpenCLMain::getInstance().getDevice(0)->getContext()->getProgram(0)->getKernel("metis512");
	OpenCLKernel* kernel_init = OpenCLMain::getInstance().getDevice(0)->getContext()->getProgram(0)->getKernel("metis_init_g");
	OpenCLBuffer* in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64, CL_MEM_WRITE_ONLY, NULL);
	OpenCLBuffer* out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64, CL_MEM_WRITE_ONLY, NULL);
	OpenCLBuffer* ctx = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(sph_metis512_context), CL_MEM_WRITE_ONLY, NULL);
	OpenCLCommandQueue * q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));

	kernel->resetArgs();
	kernel->addGlobalArg(in);
	kernel->addGlobalArg(out);

	kernel_init->resetArgs();
	kernel_init->addGlobalArg(ctx);

	for(uint32 n=0; n<0x1000; n++)
	{
		if( block->height != monitorCurrentBlockHeight )
			break;
		for(uint32 f=0; f<0x8000; f++)
		{
			sph_keccak512_init(&ctx_keccak);
			sph_keccak512(&ctx_keccak, &block->version, 80);
			sph_keccak512_close(&ctx_keccak, hash0);

			sph_shavite512_init(&ctx_shavite);
			sph_shavite512(&ctx_shavite, hash0, 64);
			sph_shavite512_close(&ctx_shavite, hash1);

			struct {
				cl_uint partial;
				cl_uint partial_len;
				cl_uint round_shift;
				cl_uint S[36];
				cl_ulong bit_count;
			} ctx_metis2;

			sph_metis512_init(&ctx_metis);

			q->enqueueKernel1D(kernel_init, 1, 1);
			q->enqueueReadBuffer(ctx, &ctx_metis2, 164);
			q->finish();
			printf ("ctx size = %d\n", sizeof(sph_metis512_context));
			for (int i = 0; i < 36; i++) {
				if (ctx_metis.S[i] != ctx_metis2.S[i]) {
					printf("init is different s[%d] = %X - %X\n", i, ctx_metis.S[i], ctx_metis2.S[i]);
				}
			}
			if (ctx_metis.partial != ctx_metis2.partial) {
				printf("init partial is different %X %X\n", ctx_metis.partial, ctx_metis2.partial);
			}
			if (ctx_metis.partial_len != ctx_metis2.partial_len) {
				printf("init partial_len is different %X %X\n", ctx_metis.partial_len, ctx_metis2.partial_len);
			}
			if (ctx_metis.round_shift != ctx_metis2.round_shift) {
				printf("init round_shift is different %X %X\n", ctx_metis.round_shift, ctx_metis2.round_shift);
			}
			if (ctx_metis.bit_count != ctx_metis2.bit_count) {
				printf("init bit_count is different %X %X\n", ctx_metis.bit_count != ctx_metis2.bit_count);
			}

			sph_metis512(&ctx_metis, hash1, 64);
			sph_metis512_close(&ctx_metis, hash2);

//			q->enqueueReadBuffer(in, hash1, 64);
//			q->enqueueKernel1D(kernel, 1, 1);
//			q->enqueueWriteBuffer(out, hash2_2, 64);
//
//			q->finish();
//
//			for (int i = 0; i < 64; i++) {
//				if (hash2[i] != hash2_2[i]) {
//					printf ("hashes are different\n");
//				}
//			}

			if( *(uint32*)((uint8*)hash2+28) <= target )
			{
				totalShareCount++;
				xptMiner_submitShare(block);
			}
			block->nonce++;
		}
		totalCollisionCount += 0x8000;
	}

	delete q;
	delete out;
	delete in;
	delete kernel;

}
