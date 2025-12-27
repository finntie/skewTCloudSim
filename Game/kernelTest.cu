
#include <sstream>
#include <iostream>
////

#include <cassert>
#include <memory>

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>
#include "kernelTest.cuh"



__global__ void addOne(uint64_t* data, int size) {
	int idx = threadIdx.x + blockIdx.x * blockDim.x;  // Unique index for each thread
	if (idx < size) {
		data[idx] += + 10;  // Modify the data at the idx position
	}
}

void kerneltest()
{

	int threads = 256;  // Number of threads per block
	//int numBlocks = (256 * 256 + threads - 1) / threads;  // Calculate number of blocks
	int numBlocks = cuda::ceil_div(256 * 256, threads); //Using CUDA function to do the same calculation

	// Allocate and copy data to device
	//uint64_t data[256 * 256][64];

	//uint64_t data[256 * 256];

	uint64_t* h_idata = (uint64_t*)malloc(256 * 256 * sizeof(uint64_t));
	uint64_t* h_odata = (uint64_t*)malloc(256 * 256 * sizeof(uint64_t));

	if (h_idata == 0 || h_odata == 0) {
		fprintf(stderr, "Not enough memory avaialable on host to run test!\n");
		exit(EXIT_FAILURE);
	}

	for (unsigned int i = 0; i < 256 * 256; i++) {
		h_idata[i] = (uint64_t)(i & 0xff);
	}

	printf("Parts of data we put in: [0]: %lli, [1]: %lli, [2]: %lli, [3]: %lli\n", h_idata[0], h_idata[1], h_idata[2], h_idata[3]);


	uint64_t* d_input;
	cudaMalloc((void**)&d_input, 256 * 256 * sizeof(uint64_t));
	cudaMemcpy(d_input, h_idata, 256 * 256 * sizeof(uint64_t), cudaMemcpyHostToDevice);
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl;
		exit(-1);
	}

	printf("Succesfully send %i data\n", static_cast<int>(256 * 256 * sizeof(uint64_t)));

	printf("Adding 10 to each item on the GPU...\n");
	addOne << <numBlocks, threads >> > (d_input, 256 * 256 * sizeof(uint64_t));

	for (unsigned int i = 0; i < 64; i++) {
		cudaMemcpyAsync(h_odata, d_input, 256 * 256 * sizeof(uint64_t),
			cudaMemcpyDeviceToHost, 0);
	}
	cudaDeviceSynchronize();
	
	err = cudaGetLastError();
	if (err != cudaSuccess) {
	
		std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl;
		exit(-1);
	}
	printf("Succesfully Received %i data\n", static_cast<int>(256 * 256 * sizeof(uint64_t)));

	printf("Now our data is: [0]: %lli, [1]: %lli, [2]: %lli, [3]: %lli\n", h_odata[0], h_odata[1], h_odata[2], h_odata[3]);

}