#ifndef PHYSICS_LAB_CONTROL_BRIDGE_H
#define PHYSICS_LAB_CONTROL_BRIDGE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// Helpers shared by the control bridge of every simulation. Each adapter owns
// its own parameter and state tables; this header only carries the mechanics of
// validating writes and publishing buffers the page reads from the Wasm heap.
namespace physicslab::bridge
{
    // The page reads at most this many points per plot, so a long run costs the
    // same per frame as a short one.
    inline constexpr std::size_t MAX_ANALYSIS_POINTS = 512;

    inline bool sanitize(float value, float minimum, float maximum, float &sanitized)
    {
        if (!std::isfinite(value))
        {
            return false;
        }

        sanitized = std::clamp(value, minimum, maximum);
        return true;
    }

    inline bool toBoolean(float value)
    {
        return value != 0.0f;
    }

    inline float fromBoolean(bool value)
    {
        return value ? 1.0f : 0.0f;
    }

    inline std::size_t renderedPointCount(std::size_t sampleCount)
    {
        return std::min(sampleCount, MAX_ANALYSIS_POINTS);
    }

    inline std::size_t mapRenderedSampleIndex(std::size_t sampleCount,
                                              std::size_t pointCount,
                                              std::size_t renderedIndex)
    {
        if (sampleCount <= 1 || pointCount <= 1)
        {
            return 0;
        }

        return (renderedIndex * (sampleCount - 1)) / (pointCount - 1);
    }

    // Fixed-capacity buffer of interleaved point pairs, refreshed once per frame.
    class SampleBuffer
    {
    public:
        void clear() { pointCount = 0; }

        void append(float horizontal, float vertical)
        {
            if (pointCount >= MAX_ANALYSIS_POINTS || !std::isfinite(horizontal) || !std::isfinite(vertical))
            {
                return;
            }

            values[pointCount * 2] = horizontal;
            values[(pointCount * 2) + 1] = vertical;
            ++pointCount;
        }

        const float *data() const { return values.data(); }
        int count() const { return static_cast<int>(pointCount); }

    private:
        std::array<float, MAX_ANALYSIS_POINTS * 2> values{};
        std::size_t pointCount{0};
    };

    // Downsamples a retained history into interleaved pairs.
    template <typename Sample, typename Horizontal, typename Vertical>
    void publishSamples(SampleBuffer &buffer,
                        const std::vector<Sample> &history,
                        Horizontal horizontal,
                        Vertical vertical)
    {
        buffer.clear();

        const std::size_t pointCount = renderedPointCount(history.size());
        for (std::size_t index = 0; index < pointCount; ++index)
        {
            const Sample &sample = history[mapRenderedSampleIndex(history.size(), pointCount, index)];
            buffer.append(horizontal(sample), vertical(sample));
        }
    }
} // namespace physicslab::bridge

#endif // PHYSICS_LAB_CONTROL_BRIDGE_H
