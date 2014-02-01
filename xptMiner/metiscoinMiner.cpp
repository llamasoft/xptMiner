#include"global.h"
#include "OpenCLObjects.h"
#include "ticker.h"

OpenCLKernel* kernel;
OpenCLKernel* kernel_metis;
OpenCLBuffer* in;
OpenCLBuffer* out;
OpenCLBuffer* out_count;
OpenCLCommandQueue * q;
uint32_t *out_tmp = new uint32_t[255];

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
#ifdef VALIDATE_ALGORITHMS
	kernel_metis = program->getKernel("metis512");
#endif

	main.listDevices();

	in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(80, CL_MEM_READ_WRITE, NULL);
	out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint) * 255, CL_MEM_READ_WRITE, NULL);
	out_count = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);
	q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));

	uint64_t *out_tmp = new uint64_t[0x8000*8];


}

void metiscoin_process(minerMetiscoinBlock_t* block)
{

	block->nonce = 0;
	uint32 target = *(uint32*)(block->targetShare+28);

	// measure time
	for(uint32 n=0; n<0x1000; n++)
	{
#ifdef MEASURE_TIME
		uint32 begin = getTimeMilliseconds();
#endif
		if( block->height != monitorCurrentBlockHeight )
			break;

		kernel->resetArgs();
		kernel->addGlobalArg(in);
		kernel->addGlobalArg(out);
		kernel->addGlobalArg(out_count);
		kernel->addScalarUInt(n*0x8000);
		kernel->addScalarUInt(target);

		cl_uint out_count_tmp = 0;

		q->enqueueWriteBuffer(in, &block->version, 80);
		q->enqueueWriteBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->enqueueKernel1D(kernel, 0x8000, kernel->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));
		q->enqueueReadBuffer(out, out_tmp, sizeof(cl_uint) * 255);
		q->enqueueReadBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->finish();

		for (int i =0; i < out_count_tmp; i++) {
			block->nonce = out_tmp[i];
			xptMiner_submitShare(block);
		}

		totalCollisionCount += 0x8000;
#ifdef MEASURE_TIME
		uint32 end = getTimeMilliseconds();
		printf("Elapsed time: %d ms\n", (end-begin));
#endif

#ifdef VALIDATE_ALGORITHMS
		// validator
		block->nonce = n * 0x8000;
		for (int f = 0; f < 0x8000; f++) {
			sph_keccak512_context	 ctx_keccak;
			sph_shavite512_context	 ctx_shavite;
			sph_metis512_context	 ctx_metis;
			uint64 hash0[8];
			uint64 hash1[8];
			uint64 hash2[8];
			uint64 hash2_2[8];

			sph_keccak512_init(&ctx_keccak);
			sph_shavite512_init(&ctx_shavite);
			sph_metis512_init(&ctx_metis);
			sph_keccak512(&ctx_keccak, &block->version, 80);
			sph_keccak512_close(&ctx_keccak, hash0);
			sph_shavite512(&ctx_shavite, hash0, 64);
			sph_shavite512_close(&ctx_shavite, hash1);

//			printf ("bit_count = %d partial = %d partial_len = %d round_shift = %d\n", ctx_metis.bit_count, ctx_metis.partial, ctx_metis.partial_len, ctx_metis.round_shift);

			sph_metis512(&ctx_metis, hash1, 64);

			// printfs metis contxt
			//printf ("bit_count = %d partial = %d partial_len = %d round_shift = %d\n", ctx_metis.bit_count, ctx_metis.partial, ctx_metis.partial_len, ctx_metis.round_shift);


			sph_metis512_close(&ctx_metis, hash2);

			kernel_metis->resetArgs();
			kernel_metis->addGlobalArg(in);
			kernel_metis->addGlobalArg(out);
			q->enqueueWriteBuffer(in, hash1, 64);
			q->enqueueKernel1D(kernel_metis, 1, 1);
			q->enqueueReadBuffer(out, hash2_2, 64);
			q->finish();

			for (int i = 0; i < 8; i++) {
				if (hash2[i] != hash2_2[i]) {
					printf ("**** Hashes do not match %i %lx %lx\n", i, hash2[i], hash2_2[i]);
				}
			}

			block->nonce++;
		}
		block->nonce = 0;
#endif
	}

}
