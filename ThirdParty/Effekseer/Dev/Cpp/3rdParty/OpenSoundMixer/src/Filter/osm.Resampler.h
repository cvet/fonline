
#pragma once

#define NOMINMAX
#include <math.h>
#include <memory>
#include <string>
#include <vector>

#include "../OpenSoundMixer.h"

namespace osm {
/**
@brief	再サンプリングを行うフィルタ
@note	他にもフィルタが増えたら基底クラスがあったほうがよい。
*/
class Resampler {
private:
    enum {
        MAX_LATENCY = 128,
        BUFFER_LEN = MAX_LATENCY * 2,
    };
    double m_resampleRatio = 0;
    Sample m_inBuf[BUFFER_LEN];
    bool m_isInit = false;
    int32_t m_inCur = 0;
    int32_t m_outCur = 0;
    double m_outResidure = 0;

    /**
    @brief	初期化を行う。
    */
    void Init();

    /**
    @brief	元よりも細かくサンプリングする。再生速度は遅くなる。
    */
    std::pair<int32_t, int32_t> UpResample(Sample in[], int32_t inCount, Sample out[], int32_t outCount);
    /**
    @brief	元よりも粗くサンプリングする。再生速度は速くなる。
    */
    std::pair<int32_t, int32_t> DownResample(Sample in[], int32_t inCount, Sample out[], int32_t outCount);

public:
    Resampler();

    /**
    @brief	再サンプリングを行う。
    @return	実際に入力/出力したサンプル数
    */
    std::pair<int32_t, int32_t> ProcessSamples(Sample inputSamples[], int32_t inputCount, Sample outputSamples[], int32_t outputCount);

    /**
    @brief	再サンプリング比を指定する。1.0で等速, 値の大小は再生時間に比例。
    */
    void SetResampleRatio(double resampleRatio) { m_resampleRatio = resampleRatio; }

    /**
    @brief	再サンプリング比を取得する。
    */
    double GetResampleRatio() const { return m_resampleRatio; }

    /**
    @brief	入力サンプルの超過数を取得する。
    */
    int32_t GetInputExceedance() const;
};
}  // namespace osm
