#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include "osm.FastFourierTransform.h"

namespace osm {

// Implemention of Fast Fourier Transform
int FastFourierTransform(const std::vector<Sample> &samples, std::vector<float> &spectrums, FFTWindow window)
{
    int32_t sampleNum = spectrums.size();

    // "sampleNum" must be the power of 2.
    if(sampleNum & (sampleNum - 1)) return -1;

    // Allocate & initialize memroy
    std::vector<float> spectrumReal(sampleNum);
    std::vector<float> spectrumImag(sampleNum, 0.0f);
    for(int i = 0; i < sampleNum; ++i)
    {
        int iRev = BitReverse(i, sampleNum);

        // Move wave data
        Sample sample = samples[iRev];
        spectrumReal[i] = (sample.Left + sample.Right) / 32768.0f;

        // Apply window function
        switch(window)
        {
            case FFTWindow::Rectangular:
                break;

            case FFTWindow::Triangle:
                spectrumReal[i] *= Triangle((float)iRev / (float)sampleNum);
                break;

            case FFTWindow::Hamming:
                spectrumReal[i] *= Hamming((float)iRev / (float)sampleNum);
                break;

            case FFTWindow::Hanning:
                spectrumReal[i] *= Hanning((float)iRev / (float)sampleNum);
                break;

            case FFTWindow::Blackman:
                spectrumReal[i] *= Blackman((float)iRev / (float)sampleNum);
                break;

            case FFTWindow::BlackmanHarris:
                spectrumReal[i] *= BlackmanHarris((float)iRev / (float)sampleNum);
                break;
        }
    }

    // Fast Fourier Transform
    for(int i = 1; i < sampleNum; i *= 2)
        for(int j = 0; j < sampleNum; j += i * 2)
            for(int k = 0; k < i; ++k)
            {
                int m = j + k;
                int n = m + i;
                double tr = cos(-M_PI / (double)i * (double)k);
                double ti = sin(-M_PI / (double)i * (double)k);
                double mr = spectrumReal[m] + spectrumReal[n] * tr - spectrumImag[n] * ti;
                double mi = spectrumImag[m] + spectrumImag[n] * tr + spectrumReal[n] * ti;
                double nr = spectrumReal[m] - spectrumReal[n] * tr + spectrumImag[n] * ti;
                double ni = spectrumImag[m] - spectrumImag[n] * tr - spectrumReal[n] * ti;
                spectrumReal[m] = mr; spectrumImag[m] = static_cast<float>(mi);
                spectrumReal[n] = nr; spectrumImag[n] = static_cast<float>(ni);
            }

    // Return spectrum data as real value
    for(int i = 0; i < sampleNum; ++i)
        spectrums[i] = sqrt(pow(spectrumReal[i], 2) + pow(spectrumImag[i], 2));

    return 0;
}

// Bit Reverse
static int BitReverse(int32_t x, int32_t x_max)
{
    int bit_rev = 0;
    for(int i = x_max / 2; i >= 1; i /= 2)
    {
        bit_rev += i * (x % 2);
        x /= 2;
    }
    return bit_rev;
}

// Window Functions
static float Triangle(float x) { return 1 - abs(2 * x - 1); }

static float Hamming(float x) { return 0.54f - 0.46f * cosf(2 * M_PI * x); }

static float Hanning(float x) { return 0.5f - 0.5f * cosf(2 * M_PI * x); }

static float Blackman(float x) { return 0.42f - 0.5f * cosf(2 * M_PI * x) + 0.08f * cosf(4 * M_PI * x); }

static float BlackmanHarris(float x) { return 0.35875f - 0.48829f * cosf(2 * M_PI * x) + 0.14128f * cosf(4 * M_PI * x) - 0.012604f * cosf(6 * M_PI * x); }

}