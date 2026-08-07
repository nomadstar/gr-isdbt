#include <hip/hip_runtime.h>
#include <hip/hip_complex.h>
#include <stdio.h>
#include "mmse_taps.h"
#include <gnuradio/gr_complex.h>

extern "C" {

bool isdbt_gpu_detect() {
    int deviceCount = 0;
    hipError_t error_id = hipGetDeviceCount(&deviceCount);
    if (error_id != hipSuccess) {
        printf("HIP Error: %s\n", hipGetErrorString(error_id));
        return false;
    }
    if (deviceCount == 0) {
        printf("No ROCm/HIP devices found.\n");
        return false;
    }

    hipDeviceProp_t deviceProp;
    hipGetDeviceProperties(&deviceProp, 0);
    printf("GPU Detectada (HIP): %s (gcnArch %s)\n", deviceProp.name, deviceProp.gcnArchName);
    return true;
}

void isdbt_gpu_alloc_buffers(int max_in_size, int max_out_size, void** in_gpu, void** out_gpu) {
    hipMalloc(in_gpu, max_in_size * sizeof(gr_complex));
    hipMalloc(out_gpu, max_out_size * sizeof(gr_complex));
}

void isdbt_gpu_free_buffers(void* in_gpu, void* out_gpu) {
    if(in_gpu) hipFree(in_gpu);
    if(out_gpu) hipFree(out_gpu);
}

__global__ void mmse_interp_kernel_hip(const hipFloatComplex* in, hipFloatComplex* out,
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

        hipFloatComplex result = make_hipFloatComplex(0.0f, 0.0f);
        for(int j=0; j<8; j++) {
            float tap = mmse_taps[imu][j];
            hipFloatComplex val = in[ii + j];
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
    hipMemcpy(in_gpu, in_cpu, num_inputs * sizeof(gr_complex), hipMemcpyHostToDevice);

    int blockSize = 256;
    int numBlocks = (num_outputs + blockSize - 1) / blockSize;

    hipLaunchKernelGGL(mmse_interp_kernel_hip, dim3(numBlocks), dim3(blockSize), 0, 0,
        (const hipFloatComplex*)in_gpu,
        (hipFloatComplex*)out_gpu,
        d_samp_phase_0,
        d_samp_inc,
        num_outputs
    );

    hipDeviceSynchronize();

    // Copy data back
    hipMemcpy(out_cpu, out_gpu, num_outputs * sizeof(gr_complex), hipMemcpyDeviceToHost);

    // Calculate final index to return (equivalent to while loop exit state)
    double final_s = d_samp_phase_0 + num_outputs * d_samp_inc;
    return (int)floor(final_s);
}

}
