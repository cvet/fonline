
#pragma once

#include <math.h>
#include <memory>
#include <string>
#include <vector>

#include "../OpenSoundMixer.h"

namespace osm {
/**
@brief	パニングを行うフィルタ
@note	基底クラスがあったほうがよい。
*/
class Panner {
private:
    double m_position;

public:
    Panner();

    /**
    @brief	パニングを行う。
    @return	実際に入力/出力したサンプル数
    @note	現状では、入力/出力サンプル数は同じとする。
    */
    std::pair<int32_t, int32_t> ProcessSamples(Sample inputSamples[], int32_t inputCount, Sample outputSamples[], int32_t outputCount);

    /**
    @brief	左右位置を指定する。0.0で中央, -1.0で左, 1.0で右。
    */
    void SetPosition(double position) { m_position = position; }

    /**
    @brief	左右位置を取得する。
    */
    double GetPosition() const { return m_position; }
};
}  // namespace osm
