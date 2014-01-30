#include"global.h"
#include "OpenCLObjects.h"

void metiscoin_init_opencl(int device_num) {
	printf("Initializing GPU %d\n", device_num);
	OpenCLMain &main = OpenCLMain::getInstance();

	std::vector<std::string> files_metis;
	files_metis.push_back("opencl/metis.cl");
	OpenCLProgram* program_metis = main.getDevice(0)->getContext()->loadProgramFromFiles(files_metis);
	OpenCLKernel* kernel_metis = program_metis->getKernel("metis512");

	std::vector<std::string> files_shavite;
	files_shavite.push_back("opencl/shavite.cl");
	OpenCLProgram* program_shavite = main.getDevice(0)->getContext()->loadProgramFromFiles(files_shavite);
	OpenCLKernel* kernel_shavite = program_shavite->getKernel("shavite512");

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
	OpenCLBuffer* in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64, CL_MEM_WRITE_ONLY, NULL);
	OpenCLBuffer* out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64, CL_MEM_WRITE_ONLY, NULL);
	OpenCLCommandQueue * q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));

	kernel->resetArgs();
	kernel->addGlobalArg(in);
	kernel->addGlobalArg(out);

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

			sph_metis512_init(&ctx_metis);
			sph_metis512(&ctx_metis, hash1, 64);
			sph_metis512_close(&ctx_metis, hash2);

//			q->enqueueWriteBuffer(in, hash1, 64);
//			q->enqueueKernel1D(kernel, 1, 1);
//			q->enqueueReadBuffer(out, hash2_2, 64);
//			q->finish();
//
//			for (int i = 0; i < 8; i++) {
//				if (hash2[i] != hash2_2[i]) {
//					printf ("hashes are different %d %lX %lX\n", i, hash2[i], hash2_2[i]);
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
