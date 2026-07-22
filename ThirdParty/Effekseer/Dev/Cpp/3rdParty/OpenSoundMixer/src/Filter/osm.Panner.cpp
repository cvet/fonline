#define NOMINMAX
#include "osm.Panner.h"
#include <algorithm>
#include "../OpenSoundMixerInternal.h"

namespace osm {
Panner::Panner() : m_position(0.0) {}

std::pair<int32_t, int32_t> Panner::ProcessSamples(Sample inputSamples[], int32_t inputCount, Sample outputSamples[], int32_t outputCount) {
    int32_t count = std::min(inputCount, outputCount);
    if (m_position == 0.0) {
        memcpy(outputSamples, inputSamples, count * sizeof(Sample));
        return std::make_pair(count, count);
    }
    for (int32_t i = 0; i < count; ++i) {
        const Sample& in = inputSamples[i];
        Sample& out = outputSamples[i];
        double mix = in.Left + in.Right;
        double dif = in.Left - in.Right;
        double left = mix * (m_position * -0.5 + 0.5) + dif * 0.5;
        double right = mix * (m_position * 0.5 + 0.5) - dif * 0.5;
        out.Left = Clamp((int32_t)left, 32767, -32768);
        out.Right = Clamp((int32_t)right, 32767, -32768);
    }
    return std::make_pair(count, count);
}

}  // namespace osm
