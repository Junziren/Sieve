#pragma once
#include <juce_core/juce_core.h>
#include "../Core/SortingAlgorithms.h"

namespace SortSynth {

class SortingAlgorithmTests : public juce::UnitTest {
public:
    SortingAlgorithmTests() : juce::UnitTest("Sorting Algorithms") {}

    void runTest() override {
        auto testAlgo = [this](Algorithm algo, const juce::String& name) {
            beginTest(name);
            for (int n : {4, 8, 16, 32, 64}) {
                std::vector<int> arr(n);
                for (int i = 0; i < n; ++i) arr[i] = n - i; // reverse order

                auto steps = SortingAlgorithms::generateSteps(algo, arr);

                // Verify sorted
                for (int i = 1; i < n; ++i)
                    expect(arr[i - 1] <= arr[i],
                           name + " size " + juce::String(n) + " not sorted at " + juce::String(i));

                // Verify steps are non-empty
                expect(!steps.empty(), name + " produced no steps");
            }
        };

        testAlgo(Algorithm::Bubble, "Bubble");
        testAlgo(Algorithm::Insertion, "Insertion");
        testAlgo(Algorithm::Selection, "Selection");
        testAlgo(Algorithm::Quick, "Quick");
        testAlgo(Algorithm::Merge, "Merge");
        testAlgo(Algorithm::Shell, "Shell");
        testAlgo(Algorithm::Heap, "Heap");
        testAlgo(Algorithm::Shaker, "Shaker");
        testAlgo(Algorithm::Bogo, "Bogo");
    }
};

static SortingAlgorithmTests sortingTests;

} // namespace SortSynth
