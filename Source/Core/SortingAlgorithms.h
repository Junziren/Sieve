#pragma once
#include "Core/Common.h"
#include <vector>
#include <string>

namespace SortSynth {

class SortingAlgorithms {
public:
    /** Precompute all sort steps for a given algorithm and array size. */
    static std::vector<SortStep> generateSteps(Algorithm algo, std::vector<int>& arr);

    /** Get algorithm name string. */
    static std::string name(Algorithm algo);

private:
    static std::vector<SortStep> bubbleSteps(std::vector<int>& arr);
    static std::vector<SortStep> insertionSteps(std::vector<int>& arr);
    static std::vector<SortStep> selectionSteps(std::vector<int>& arr);
    static std::vector<SortStep> quickSteps(std::vector<int>& arr);
    static std::vector<SortStep> mergeSteps(std::vector<int>& arr);
    static std::vector<SortStep> shellSteps(std::vector<int>& arr);
    static std::vector<SortStep> heapSteps(std::vector<int>& arr);
    static std::vector<SortStep> shakerSteps(std::vector<int>& arr);
    static std::vector<SortStep> bogoSteps(std::vector<int>& arr);
};

} // namespace SortSynth
