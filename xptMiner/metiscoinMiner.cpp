#include"global.h"
#include "OpenCLObjects.h"

OpenCLKernel* kernel;
OpenCLBuffer* in;
OpenCLBuffer* out;
OpenCLCommandQueue * q;
uint64_t *out_tmp = new uint64_t[0x8000*8];

void metiscoin_init_opencl(int device_num) {
	printf("Initializing GPU %d\n", device_num);
	OpenCLMain &main = OpenCLMain::getInstance();

//	std::vector<std::string> files_metis;
//	files_metis.push_back("opencl/metis.cl");
//	OpenCLProgram* program_metis = main.getDevice(0)->getContext()->loadProgramFromFiles(files_metis);
//	OpenCLKernel* kernel_metis = program_metis->getKernel("metis512");
//
//	std::vector<std::string> files_shavite;
//	files_shavite.push_back("opencl/shavite.cl");
//	OpenCLProgram* program_shavite = main.getDevice(0)->getContext()->loadProgramFromFiles(files_shavite);
//	OpenCLKernel* kernel_shavite = program_shavite->getKernel("shavite512");

	std::vector<std::string> files_keccak;
	files_keccak.push_back("opencl/keccak.cl");
	files_keccak.push_back("opencl/shavite.cl");
	files_keccak.push_back("opencl/metis.cl");
	files_keccak.push_back("opencl/miner.cl");
	OpenCLProgram* program = main.getDevice(0)->getContext()->loadProgramFromFiles(files_keccak);
	kernel = program->getKernel("metiscoin_process");

	main.listDevices();

	in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(80, CL_MEM_READ_WRITE, NULL);
	out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64 * 0x8000, CL_MEM_READ_WRITE, NULL);
	q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));

	uint64_t *out_tmp = new uint64_t[0x8000*8];


}

void metiscoin_process(minerMetiscoinBlock_t* block)
{
	sph_keccak512_context	 ctx_keccak;
	sph_shavite512_context	 ctx_shavite;
	sph_metis512_context	 ctx_metis;
	static unsigned char pblank[1];
	block->nonce = 0;

	uint32 target = *(uint32*)(block->targetShare+28);
//	uint64 hash0[8];
//	uint64 hash1[8];
//	uint64 hash2[8];
//	uint64 hash2_2[8];


	for(uint32 n=0; n<0x1000; n++)
	{
		if( block->height != monitorCurrentBlockHeight )
			break;

		kernel->resetArgs();
		kernel->addGlobalArg(in);
		kernel->addGlobalArg(out);
		kernel->addScalarUInt(n*0x8000);

		q->enqueueWriteBuffer(in, &block->version, 80);
		q->enqueueKernel1D(kernel, 0x8000, kernel->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));
		q->enqueueReadBuffer(out, out_tmp, 0x8000*64);
		q->finish();

		for(uint32 f=0; f<0x8000; f++)
		{
//			sph_keccak512_init(&ctx_keccak);
//			sph_keccak512(&ctx_keccak, &block->version, 80);
//			sph_keccak512_close(&ctx_keccak, hash0);
//
//			sph_shavite512_init(&ctx_shavite);
//			sph_shavite512(&ctx_shavite, hash0, 64);
//			sph_shavite512_close(&ctx_shavite, hash1);
//
//			sph_metis512_init(&ctx_metis);
//			sph_metis512(&ctx_metis, hash1, 64);
//			sph_metis512_close(&ctx_metis, hash2);

			uint64_t * hash2 = out_tmp + (f * 8);

			if( *(uint32*)((uint8*)hash2+28) <= target )
			{
				totalShareCount++;
				xptMiner_submitShare(block);
			}

			block->nonce++;
		}
		totalCollisionCount += 0x8000;
	}

}
