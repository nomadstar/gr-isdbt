#include <cuda_runtime.h>
#include <stdio.h>
#include <cuComplex.h>
#include "mmse_taps.h"
#include <gnuradio/gr_complex.h>

extern "C" {

bool isdbt_gpu_detect() {
    int deviceCount = 0;
    cudaError_t error_id = cudaGetDeviceCount(&deviceCount);
    if (error_id != cudaSuccess) {
        printf("CUDA Error: %s\n", cudaGetErrorString(error_id));
        return false;
    }
    if (deviceCount == 0) {
        printf("No CUDA devices found.\n");
        return false;
    }
    
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, 0);
    printf("GPU Detectada: %s (Compute %d.%d)\n", deviceProp.name, deviceProp.major, deviceProp.minor);
    return true;
}

void isdbt_gpu_alloc_buffers(int max_in_size, int max_out_size, void** in_gpu, void** out_gpu) {
    cudaMalloc(in_gpu, max_in_size * sizeof(gr_complex));
    cudaMalloc(out_gpu, max_out_size * sizeof(gr_complex));
}

void isdbt_gpu_free_buffers(void* in_gpu, void* out_gpu) {
    if(in_gpu) cudaFree(in_gpu);
    if(out_gpu) cudaFree(out_gpu);
}

__global__ void mmse_interp_kernel(const cuFloatComplex* in, cuFloatComplex* out, 
                                   double d_samp_phase_0, double d_samp_inc, 
                                   int num_outputs) {
    int oo = blockIdx.x * blockDim.x + threadIdx.x;
    if (oo < num_outputs) {
        double s = d_samp_phase_0 + oo * d_samp_inc;
        double f = floor(s);
        int ii = (int)f;
        float mu = (float)(s - f);

        int imu = (int)rintf(mu * 128.0f);
        if (imu < 0) imu = 0;
        if (imu > 128) imu = 128;

        cuFloatComplex result = make_cuFloatComplex(0.0f, 0.0f);
        for(int j=0; j<8; j++) {
            float tap = mmse_taps[imu][j];
            cuFloatComplex val = in[ii + j];
            result.x += val.x * tap;
            result.y += val.y * tap;
        }
        out[oo] = result;
    }
}

int isdbt_gpu_interpolate_input(const gr_complex* in_cpu, gr_complex* out_cpu, 
                                void* in_gpu, void* out_gpu,
                                double d_samp_phase_0, double d_samp_inc, 
                                int num_outputs, int num_inputs) {
                                    
    // Copy data to GPU
    cudaMemcpy(in_gpu, in_cpu, num_inputs * sizeof(gr_complex), cudaMemcpyHostToDevice);
    
    int blockSize = 256;
    int numBlocks = (num_outputs + blockSize - 1) / blockSize;
    
    mmse_interp_kernel<<<numBlocks, blockSize>>>(
        (const cuFloatComplex*)in_gpu, 
        (cuFloatComplex*)out_gpu, 
        d_samp_phase_0, 
        d_samp_inc, 
        num_outputs
    );
    
    cudaDeviceSynchronize();
    
    // Copy data back
    cudaMemcpy(out_cpu, out_gpu, num_outputs * sizeof(gr_complex), cudaMemcpyDeviceToHost);
    
    // Calculate final index to return (equivalent to while loop exit state)
    double final_s = d_samp_phase_0 + num_outputs * d_samp_inc;
    return (int)floor(final_s);
}

}
