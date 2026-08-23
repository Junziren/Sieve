#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace SortSynth {

struct Grain {
    int  id;
    float startMs;
    float durationMs;
    int  originalIndex;
    juce::Colour color;
};

struct SortStep {
    enum Type : uint8_t { Compare, Swap, Overwrite };
    Type  type;
    int   indexA;
    int   indexB;    // for Overwrite: the new value being written at indexA
};

enum class Algorithm : int {
    Bubble=0, Insertion=1, Selection=2, Quick=3, Merge=4,
    Shell=5, Heap=6, Shaker=7, Bogo=8
};

inline juce::String algorithmToString(Algorithm a) {
    switch (a) {
        case Algorithm::Bubble: return "Bubble"; case Algorithm::Insertion: return "Insertion";
        case Algorithm::Selection: return "Selection"; case Algorithm::Quick: return "Quick";
        case Algorithm::Merge: return "Merge"; case Algorithm::Shell: return "Shell";
        case Algorithm::Heap: return "Heap"; case Algorithm::Shaker: return "Shaker";
        case Algorithm::Bogo: return "Bogo";
    }
    return "Unknown";
}

} // namespace SortSynth
