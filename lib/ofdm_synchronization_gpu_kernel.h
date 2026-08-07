#ifndef OFDM_SYNCHRONIZATION_GPU_KERNEL_H
#define OFDM_SYNCHRONIZATION_GPU_KERNEL_H

#include <gnuradio/gr_complex.h>

extern "C" {
    bool isdbt_gpu_detect();
    
    // Allocates memory for input and output buffers
    void isdbt_gpu_alloc_buffers(int max_in_size, int max_out_size, void** in_gpu, void** out_gpu);
    
    // Frees memory
    void isdbt_gpu_free_buffers(void* in_gpu, void* out_gpu);
    
    // Interpolate using MMSE FIR
    int isdbt_gpu_interpolate_input(const gr_complex* in_cpu, gr_complex* out_cpu, 
                                    void* in_gpu, void* out_gpu,
                                    double d_samp_phase_0, double d_samp_inc, 
                                    int num_outputs, int num_inputs);
}

#endif
