#include"global.h"
#include "OpenCLObjects.h"

OpenCLKernel* kernel;
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

	main.listDevices();

	in = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(80, CL_MEM_READ_WRITE, NULL);
	out = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint) * 255, CL_MEM_READ_WRITE, NULL);
	out_count = OpenCLMain::getInstance().getDevice(0)->getContext()->createBuffer(sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);
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

	// measure time
	for(uint32 n=0; n<0x1000; n++)
	{
		if( block->height != monitorCurrentBlockHeight )
			break;

		kernel->resetArgs();
		kernel->addGlobalArg(in);
		kernel->addGlobalArg(out);
		kernel->addGlobalArg(out_count);
		kernel->addScalarUInt(n*0x8000);
		kernel->addScalarUInt(target);

		cl_uint out_count_tmp = 0;

		uint32 begin = getTimeMilliseconds();
		q->enqueueWriteBuffer(in, &block->version, 80);
		q->enqueueWriteBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->enqueueKernel1D(kernel, 0x8000, kernel->getWorkGroupSize(OpenCLMain::getInstance().getDevice(0)));
		q->enqueueReadBuffer(out, out_tmp, sizeof(cl_uint) * 255);
		q->enqueueReadBuffer(out_count, &out_count_tmp, sizeof(cl_uint));
		q->finish();
		uint32 end = getTimeMilliseconds();
		printf("Elapsed time: %d ms\n", (end-begin));

		for (int i =0; i < out_count_tmp; i++) {
			block->nonce = out_tmp[i];
			xptMiner_submitShare(block);
		}

		totalCollisionCount += 0x8000;
	}

}
