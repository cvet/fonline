
#include "osm.Resampler.h"
#include <algorithm>
#include "../OpenSoundMixerInternal.h"

namespace osm {
static const double pi = 3.14159265358979323846;

// 補間関数
double lanczos3(double x) {
    if (x < -3) return 0.0;
    if (x > 3) return 0.0;
    if (x == 0) return 1.0;
    return 3 * sin(pi * x) * sin(pi * x / 3) / (pi * pi * x * x);
}

// 補間関数のテーブル引き
double lanczos3_table(double x) {
    static bool init = false;
    static const size_t len = 512 * 3;
    static double table[len + 1];
    if (!init) {
        for (int32_t i = 0; i <= len; ++i) {
            table[i] = lanczos3((3.0 * i) / len);
        }
        init = true;
    }
    x = fabs(x) * len / 3.0;
    int32_t ix = (int32_t)floor(x + 0.5);
    if (ix >= len) return 0.0;
    if (ix <= 0) return 1.0;
    return table[ix];
}

Resampler::Resampler() : m_resampleRatio(1.0), m_isInit(false), m_inCur(0), m_outCur(0), m_outResidure(0.0) {}

void Resampler::Init() {
    memset(m_inBuf, 0, sizeof(m_inBuf));
    m_isInit = true;
}

std::pair<int32_t, int32_t> Resampler::ProcessSamples(
        Sample inputSamples[], int32_t inputCount, Sample outputSamples[], int32_t outputCount) {
    if (!m_isInit) {
        Init();
    }
    if (m_resampleRatio < 1.0)
        return DownResample(inputSamples, inputCount, outputSamples, outputCount);
    else
        return UpResample(inputSamples, inputCount, outputSamples, outputCount);
}

std::pair<int32_t, int32_t> Resampler::UpResample(Sample in[], int32_t inCount, Sample out[], int32_t outCount) {
    double invRatio = 1.0 / m_resampleRatio;
    int32_t actualOut = 0;
    for (int32_t i = 0; i < inCount; ++i) {
        m_inBuf[(m_inCur + BUFFER_LEN) % BUFFER_LEN] = in[i];
        for (;;) {
            if (actualOut >= outCount) break;
            double x = m_outResidure;
            int32_t ix = floor(x);
            double sum[] = {0.0, 0.0};
            int32_t start = ix - 3 + 1;
            int32_t stop = ix + 3;
            for (int32_t k = start; k <= stop; ++k) {
                int32_t pos = (m_outCur + k - 3 + BUFFER_LEN) % BUFFER_LEN;
                double weight = lanczos3_table(x - k);
                sum[0] += m_inBuf[pos].Left * weight;
                sum[1] += m_inBuf[pos].Right * weight;
            }
            out[actualOut].Left = Clamp((int32_t)sum[0], 32767, -32768);
            out[actualOut].Right = Clamp((int32_t)sum[1], 32767, -32768);
            ++actualOut;
            m_outResidure += invRatio;
            int32_t n = floor(m_outResidure);
            m_outCur += n;
            m_outResidure -= n;
            if (m_outCur > m_inCur) break;
        }
        ++m_inCur;
        if (m_inCur >= BUFFER_LEN && m_outCur >= BUFFER_LEN) {
            m_inCur %= BUFFER_LEN;
            m_outCur %= BUFFER_LEN;
        }
    }
    return std::make_pair(inCount, actualOut);
}

std::pair<int32_t, int32_t> Resampler::DownResample(Sample in[], int32_t inCount, Sample out[], int32_t outCount) {
    double invRatio = 1.0 / m_resampleRatio;
    int32_t actualOut = 0;
    int32_t delay = ceil(3 * invRatio);
    for (size_t i = 0; i < inCount; ++i) {
        m_inBuf[(m_inCur + BUFFER_LEN) % BUFFER_LEN] = in[i];
        if (m_inCur >= m_outCur && actualOut < outCount) {
            double x = m_outResidure;
            double sum[] = {0.0, 0.0};
            int32_t start = floor(x - 3 * invRatio) + 1;
            int32_t stop = floor(x + 3 * invRatio);
            for (int32_t k = start; k <= stop; ++k) {
                int32_t pos = (m_outCur + k - delay + BUFFER_LEN) % BUFFER_LEN;
                double weight = lanczos3_table((x - k) * m_resampleRatio) * m_resampleRatio;
                sum[0] += m_inBuf[pos].Left * weight;
                sum[1] += m_inBuf[pos].Right * weight;
            }
            out[actualOut].Left = Clamp((int32_t)sum[0], 32767, -32768);
            out[actualOut].Right = Clamp((int32_t)sum[1], 32767, -32768);
            ++actualOut;
            m_outResidure += invRatio;
            int32_t n = floor(m_outResidure);
            m_outCur += n;
            m_outResidure -= n;
        }
        ++m_inCur;
        if (m_inCur >= BUFFER_LEN && m_outCur >= BUFFER_LEN) {
            m_inCur %= BUFFER_LEN;
            m_outCur %= BUFFER_LEN;
        }
    }
    return std::make_pair(inCount, actualOut);
}

int32_t Resampler::GetInputExceedance() const { return std::max(0, m_inCur - m_outCur); }

}  // namespace osm
