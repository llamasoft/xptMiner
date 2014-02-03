#include"global.h"
#include "OpenCLObjects.h"
#include "ticker.h"

#define STEP_SIZE 0x80000
#define NUM_STEPS (0x8000000/STEP_SIZE)

OpenCLKernel* kernel_all;
OpenCLKernel* kernel_keccak_noinit;
OpenCLKernel* kernel_shavite;
OpenCLKernel* kernel_metis;
#ifdef VALIDATE_ALGORITHMS
OpenCLKernel* kernel_validate;
#endif
OpenCLBuffer* u;
OpenCLBuffer* buff;
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
	kernel_keccak_noinit = program->getKernel("keccak_step_noinit");
	kernel_shavite = program->getKernel("shavite_step");
	kernel_metis = program->getKernel("metis_step");
#ifdef VALIDATE_ALGORITHMS
	kernel_validate = program->getKernel("metis512");
#endif

	main.listDevices();

	u = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(25*sizeof(cl_ulong), CL_MEM_READ_WRITE, NULL);
	buff = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(4, CL_MEM_READ_WRITE, NULL);

	hashes = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(
			64 * STEP_SIZE, CL_MEM_READ_WRITE, NULL);
	out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint) * 255, CL_MEM_READ_WRITE, NULL);
	out_count = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);
	q = OpenCLMain::getInstance().getDevice(0)->getContext()->createCommandQueue(OpenCLMain::getInstance().getDevice(0));
}

static inline cl_uint my_swap32(cl_uint x)
{
  return ((((x)&    0xFF)<<24) | (((x)&    0xFF00)<<8) | \
	  (((x)&0xFF0000)>> 8) | (((x)&0xFF000000)>>24));
}

void metiscoin_process(minerMetiscoinBlock_t* block)
{

	block->nonce = 0;
	uint32 target = *(uint32*)(block->targetShare+28);

	// measure time
	for (uint32 n = 0; n < NUM_STEPS; n++)
	{
#ifdef MEASURE_TIME
		uint32 begin = getTimeMilliseconds();
#endif
		if( block->height != monitorCurrentBlockHeight )
			break;

		//keccak
		//kernel void keccak_step_noinit(constant const ulong* u, constant const char* buff, global ulong* out, uint begin_nonce)
		kernel_keccak_noinit->resetArgs();
		kernel_keccak_noinit->addGlobalArg(u);
		kernel_keccak_noinit->addGlobalArg(buff);
		kernel_keccak_noinit->addGlobalArg(hashes);
		kernel_keccak_noinit->addScalarUInt(n * STEP_SIZE);

		sph_keccak512_context	 ctx_keccak;
		sph_keccak512_init(&ctx_keccak);
		sph_keccak512(&ctx_keccak, &block->version, 80);

		q->enqueueWriteBuffer(u, ctx_keccak.u.wide, 25*sizeof(cl_ulong));
		q->enqueueWriteBuffer(buff, ctx_keccak.buf, 4);
		q->enqueueKernel1D(kernel_keccak_noinit, STEP_SIZE,
				kernel_keccak_noinit->getWorkGroupSize(
						OpenCLMain::getInstance().getDevice(0)));

#ifdef MEASURE_TIME
		printf("keccak work group size = %d\n", kernel_keccak_noinit->getWorkGroupSize(
						OpenCLMain::getInstance().getDevice(0)));
		q->finish();
		uint32 end_keccak = getTimeMilliseconds();
#endif

		// shavite
		kernel_shavite->resetArgs();
		kernel_shavite->addGlobalArg(hashes);

		q->enqueueKernel1D(kernel_shavite, STEP_SIZE,
				kernel_shavite->getWorkGroupSize(
						OpenCLMain::getInstance().getDevice(0)));

#ifdef MEASURE_TIME
		printf("shavite work group size = %d\n", kernel_shavite->getWorkGroupSize(
						OpenCLMain::getInstance().getDevice(0)));
		q->finish();
		uint32 end_shavite = getTimeMilliseconds();
#endif
		// metis
		// metis_step(global ulong* in, global uint* out, global uint* outcount, uint begin_nonce, uint target) {
		kernel_metis->resetArgs();
		kernel_metis->addGlobalArg(hashes);
		kernel_metis->addGlobalArg(out);
		kernel_metis->addGlobalArg(out_count);
		kernel_metis->addScalarUInt(n*0x8000);
		kernel_metis->addScalarUInt(target);

		cl_uint out_count_tmp = 0;
		q->enqueueWriteBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->enqueueKernel1D(kernel_metis, STEP_SIZE,
				kernel_metis->getWorkGroupSize(
						OpenCLMain::getInstance().getDevice(0)));
		q->enqueueReadBuffer(out, out_tmp, sizeof(cl_uint) * 255);
		q->enqueueReadBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->finish();

		for (int i = 0; i < out_count_tmp; i++) {
			block->nonce = out_tmp[i];
			xptMiner_submitShare(block);
		}

		totalCollisionCount += STEP_SIZE;
#ifdef MEASURE_TIME
		uint32 end = getTimeMilliseconds();
		printf("Elapsed time: %d (k = %d, s = %d, m = %d) ms\n", (end-begin), (end_keccak-begin), (end_shavite-end_keccak), (end-end_shavite));
#endif

#ifdef VALIDATE_ALGORITHMS
		uint32 begin_validation = getTimeMilliseconds();

		cl_ulong *tmp_hashes = new cl_ulong[8 * STEP_SIZE];
		q->enqueueReadBuffer(hashes, tmp_hashes,
				sizeof(cl_ulong) * 8 * STEP_SIZE);
		q->finish();

		// validator
		block->nonce = n * STEP_SIZE;
		for (int f = 0; f < STEP_SIZE; f++) {
			sph_keccak512_context	 ctx_keccak;
			sph_shavite512_context	 ctx_shavite;
			sph_metis512_context	 ctx_metis;
			cl_ulong hash0[8];
			cl_ulong hash1[8];
			cl_ulong hash2[8];
			cl_ulong *hash1_2;
			cl_ulong *hash2_2;

			sph_keccak512_init(&ctx_keccak);
			sph_shavite512_init(&ctx_shavite);
			sph_metis512_init(&ctx_metis);
			sph_keccak512(&ctx_keccak, &block->version, 80);

//			// printf contents of ctx
//			printf ("lim = %d\n",ctx_keccak.lim);
//			printf ("ptr = %d\n",ctx_keccak.ptr);
//			for (int i = 0; i < ctx_keccak.ptr; i++) {
//				printf ("buff[%d] = %X\n",i,ctx_keccak.buf[i]);
//			}
//			printf ("nonce = %X\n", block->nonce);
//			for (int i = 0; i < 25; i++) {
//				printf ("u[%d] = %lX\n",i,ctx_keccak.u.wide[i]);
//			}

			sph_keccak512_close(&ctx_keccak, hash0);
			sph_shavite512(&ctx_shavite, hash0, 64);
			sph_shavite512_close(&ctx_shavite, hash1);

			hash2_2 = tmp_hashes+(f*8);

			for (int i = 0; i < 8; i++) {
				if (hash1[i] != hash2_2[i]) {
					printf ("**** Hashes do not match %i %x %x\n", i, hash0[i], hash2_2[i]);
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
