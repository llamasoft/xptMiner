#include"global.h"
#include "OpenCLObjects.h"
#include "ticker.h"

OpenCLKernel* kernel_all;
OpenCLKernel* kernel_keccak;
OpenCLKernel* kernel_shavite;
OpenCLKernel* kernel_metis;
#ifdef VALIDATE_ALGORITHMS
OpenCLKernel* kernel_validate;
#endif
OpenCLBuffer* in;
OpenCLBuffer* hashes;
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
	kernel_all = program->getKernel("metiscoin_process");
	kernel_keccak = program->getKernel("keccak_step");
	kernel_shavite = program->getKernel("shavite_step");
	kernel_metis = program->getKernel("metis_step");
#ifdef VALIDATE_ALGORITHMS
	kernel_validate = program->getKernel("metis512");
#endif

	main.listDevices();

	in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(80, CL_MEM_READ_WRITE, NULL);
	hashes = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(64*0x8000, CL_MEM_READ_WRITE, NULL);
	out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint) * 255, CL_MEM_READ_WRITE, NULL);
	out_count = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);
	q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));
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

		//keccak
		//kernel void keccak_step(constant const char* in, global ulong* out, uint begin_nonce)
		kernel_keccak->resetArgs();
		kernel_keccak->addGlobalArg(in);
		kernel_keccak->addGlobalArg(hashes);
		kernel_keccak->addScalarUInt(n*0x8000);

		q->enqueueWriteBuffer(in, &block->version, 80);
		q->enqueueKernel1D(kernel_keccak, 0x8000, kernel_keccak->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));

#ifdef MEASURE_TIME
		q->finish();
		uint32 end_keccak = getTimeMilliseconds();
#endif
		// shavite
		kernel_shavite->resetArgs();
		kernel_shavite->addGlobalArg(hashes);

		q->enqueueKernel1D(kernel_shavite, 0x8000, kernel_shavite->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));

#ifdef MEASURE_TIME
		q->finish();
		uint32 end_shavite = getTimeMilliseconds();
#endif
		// metis
		// metis_step(global ulong* in, global uint* out, global uint* outcount, uint begin_nonce, uint target) {
//		kernel_metis->resetArgs();
//		kernel_metis->addGlobalArg(hashes);
//		kernel_metis->addGlobalArg(out);
//		kernel_metis->addGlobalArg(out_count);
//		kernel_metis->addScalarUInt(n*0x8000);
//		kernel_metis->addScalarUInt(target);
//
//		cl_uint out_count_tmp = 0;
//		q->enqueueWriteBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
//		q->enqueueKernel1D(kernel_metis, 0x8000, kernel_metis->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));
//		q->enqueueReadBuffer(out, out_tmp, sizeof(cl_uint) * 255);
//		q->enqueueReadBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->finish();
//
//		for (int i = 0; i < out_count_tmp; i++) {
//			block->nonce = out_tmp[i];
//			xptMiner_submitShare(block);
//		}

		totalCollisionCount += 0x8000;
#ifdef MEASURE_TIME
		uint32 end = getTimeMilliseconds();
		printf("Elapsed time: %d (k = %d, s = %d, m = %d) ms\n", (end-begin), (end_keccak-begin), (end_shavite-end_keccak), (end-end_shavite));
#endif

#ifdef VALIDATE_ALGORITHMS
		uint32 begin_validation = getTimeMilliseconds();
		// reads
		cl_ulong *tmp_hashes = new cl_ulong[8*0x8000];
		q->enqueueReadBuffer(hashes, tmp_hashes, sizeof(cl_ulong)*8*0x8000);
		q->finish();

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

			for (int i = 0; i < 8; i++) {
				if (hash1[i] != tmp_hashes[(8*f)+i]) {
					printf ("**** Hashes do not match %i %lx %lx\n", i, hash2[i], tmp_hashes[(8*f)+i]);
				}
			}

			block->nonce++;
		}
		delete tmp_hashes;
		block->nonce = 0;
		uint32 end_validation = getTimeMilliseconds();
		printf("Validation time: %d ms\n", (end_validation-begin_validation));
#endif
	}

}
