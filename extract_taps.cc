#include <gnuradio/filter/mmse_fir_interpolator_cc.h>
#include <iostream>
#include <iomanip>

int main() {
    gr::filter::mmse_fir_interpolator_cc interp;
    std::cout << "#ifndef MMSE_TAPS_H" << std::endl;
    std::cout << "#define MMSE_TAPS_H" << std::endl;
    std::cout << "// Extracted from gr::filter::mmse_fir_interpolator_cc" << std::endl;
    std::cout << "__constant__ float mmse_taps[129][8] = {" << std::endl;
    for(int i=0; i<=128; i++) {
        std::cout << "  {";
        for(int j=0; j<8; j++) {
            gr_complex in[8] = {0};
            in[j] = 1.0f;
            float mu = i / 128.0f;
            gr_complex out = interp.interpolate(in, mu);
            std::cout << std::fixed << std::setprecision(6) << out.real() << "f";
            if (j < 7) std::cout << ", ";
        }
        std::cout << "}";
        if (i < 128) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "};" << std::endl;
    std::cout << "#endif" << std::endl;
    return 0;
}
