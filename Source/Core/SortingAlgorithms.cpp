#include "SortingAlgorithms.h"
#include <algorithm>
#include <random>
#include <stack>
#include <cmath>

namespace SortSynth {

std::string SortingAlgorithms::name(Algorithm algo) {
    return algorithmToString(algo).toStdString();
}

std::vector<SortStep> SortingAlgorithms::generateSteps(Algorithm algo, std::vector<int>& arr) {
    switch (algo) {
        case Algorithm::Bubble:    return bubbleSteps(arr);
        case Algorithm::Insertion: return insertionSteps(arr);
        case Algorithm::Selection: return selectionSteps(arr);
        case Algorithm::Quick:     return quickSteps(arr);
        case Algorithm::Merge:     return mergeSteps(arr);
        case Algorithm::Shell:     return shellSteps(arr);
        case Algorithm::Heap:      return heapSteps(arr);
        case Algorithm::Shaker:    return shakerSteps(arr);
        case Algorithm::Bogo:      return bogoSteps(arr);
    }
    return {};
}

// ── Bubble Sort ──
std::vector<SortStep> SortingAlgorithms::bubbleSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n - 1; ++i) {
        int lastSwap = -1;
        for (int j = 0; j < n - i - 1; ++j) {
            steps.push_back({SortStep::Compare, j, j + 1});
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                steps.push_back({SortStep::Swap, j, j + 1});
                lastSwap = j + 1;
            }
        }
        if (lastSwap == -1) break; // early exit
    }
    return steps;
}

// ── Insertion Sort ──
std::vector<SortStep> SortingAlgorithms::insertionSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0) {
            steps.push_back({SortStep::Compare, j, j + 1});
            if (arr[j] > key) {
                arr[j + 1] = arr[j]; steps.push_back({SortStep::Overwrite, j + 1, arr[j]});
                --j;
            } else {
                break;
            }
        }
        if (j + 1 != i) {
            arr[j + 1] = key; steps.push_back({SortStep::Overwrite, j + 1, key});
        }
    }
    return steps;
}

// ── Selection Sort ──
std::vector<SortStep> SortingAlgorithms::selectionSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n; ++j) {
            steps.push_back({SortStep::Compare, j, minIdx});
            if (arr[j] < arr[minIdx])
                minIdx = j;
        }
        if (minIdx != i) {
            std::swap(arr[i], arr[minIdx]);
            steps.push_back({SortStep::Swap, i, minIdx});
        }
    }
    return steps;
}

// ── Quick Sort (iterative) ──
static void quickSortIter(std::vector<int>& arr, std::vector<SortStep>& steps,
                           int low, int high) {
    std::stack<std::pair<int,int>> stack;
    stack.push({low, high});
    
    while (!stack.empty()) {
        auto [lo, hi] = stack.top(); stack.pop();
        if (lo >= hi) continue;
        
        int pivot = arr[hi];
        int i = lo - 1;
        for (int j = lo; j < hi; ++j) {
            steps.push_back({SortStep::Compare, j, hi});
            if (arr[j] < pivot) {
                ++i;
                if (i != j) {
                    std::swap(arr[i], arr[j]);
                    steps.push_back({SortStep::Swap, i, j});
                }
            }
        }
        ++i;
        if (i != hi) {
            std::swap(arr[i], arr[hi]);
            steps.push_back({SortStep::Swap, i, hi});
        }
        
        stack.push({lo, i - 1});
        stack.push({i + 1, hi});
    }
}

std::vector<SortStep> SortingAlgorithms::quickSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    if (n > 0) quickSortIter(arr, steps, 0, n - 1);
    return steps;
}

// ── Merge Sort ──
static void merge(std::vector<int>& arr, std::vector<SortStep>& steps,
                  int left, int mid, int right) {
    std::vector<int> temp(right - left + 1);
    int i = left, j = mid + 1, k = 0;
    
    while (i <= mid && j <= right) {
        steps.push_back({SortStep::Compare, i, j});
        if (arr[i] <= arr[j]) {
            temp[k++] = arr[i++];
        } else {
            temp[k++] = arr[j++];
        }
    }
    while (i <= mid) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];
    
    for (int t = 0; t < k; ++t) {
        if (arr[left + t] != temp[t]) {
            arr[left + t] = temp[t]; steps.push_back({SortStep::Overwrite, left + t, temp[t]});
        }
    }
}

static void mergeSortRec(std::vector<int>& arr, std::vector<SortStep>& steps,
                          int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    mergeSortRec(arr, steps, left, mid);
    mergeSortRec(arr, steps, mid + 1, right);
    merge(arr, steps, left, mid, right);
}

std::vector<SortStep> SortingAlgorithms::mergeSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    if (n > 0) mergeSortRec(arr, steps, 0, n - 1);
    return steps;
}

// ── Shell Sort (Ciura gap sequence) ──
std::vector<SortStep> SortingAlgorithms::shellSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    
    // Ciura gap sequence (extended)
    static const int gaps[] = {701, 301, 132, 57, 23, 10, 4, 1};
    
    for (int gap : gaps) {
        if (gap >= n) continue;
        for (int i = gap; i < n; ++i) {
            int temp = arr[i];
            int j = i;
            while (j >= gap) {
                steps.push_back({SortStep::Compare, j - gap, j});
                if (arr[j - gap] > temp) {
                    arr[j] = arr[j - gap]; steps.push_back({SortStep::Overwrite, j, arr[j - gap]});
                    j -= gap;
                } else {
                    break;
                }
            }
            if (j != i) {
                arr[j] = temp; steps.push_back({SortStep::Overwrite, j, temp});
            }
        }
    }
    return steps;
}

// ── Heap Sort ──
static void heapify(std::vector<int>& arr, std::vector<SortStep>& steps,
                     int n, int i) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    
    if (left < n) {
        steps.push_back({SortStep::Compare, left, largest});
        if (arr[left] > arr[largest])
            largest = left;
    }
    if (right < n) {
        steps.push_back({SortStep::Compare, right, largest});
        if (arr[right] > arr[largest])
            largest = right;
    }
    
    if (largest != i) {
        std::swap(arr[i], arr[largest]);
        steps.push_back({SortStep::Swap, i, largest});
        heapify(arr, steps, n, largest);
    }
}

std::vector<SortStep> SortingAlgorithms::heapSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    
    // Phase 1: heapify
    for (int i = n / 2 - 1; i >= 0; --i)
        heapify(arr, steps, n, i);
    
    // Phase 2: extract
    for (int i = n - 1; i > 0; --i) {
        std::swap(arr[0], arr[i]);
        steps.push_back({SortStep::Swap, 0, i});
        heapify(arr, steps, i, 0);
    }
    
    return steps;
}

// ── Cocktail Shaker Sort ──
std::vector<SortStep> SortingAlgorithms::shakerSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    int left = 0, right = n - 1;
    bool swapped = true;
    
    while (swapped) {
        swapped = false;
        
        // Forward pass (left → right)
        for (int i = left; i < right; ++i) {
            steps.push_back({SortStep::Compare, i, i + 1});
            if (arr[i] > arr[i + 1]) {
                std::swap(arr[i], arr[i + 1]);
                steps.push_back({SortStep::Swap, i, i + 1});
                swapped = true;
            }
        }
        --right;
        if (!swapped) break;
        
        swapped = false;
        // Backward pass (right → left)
        for (int i = right; i > left; --i) {
            steps.push_back({SortStep::Compare, i - 1, i});
            if (arr[i - 1] > arr[i]) {
                std::swap(arr[i - 1], arr[i]);
                steps.push_back({SortStep::Swap, i - 1, i});
                swapped = true;
            }
        }
        ++left;
    }
    
    return steps;
}

// ── Bogo Sort (fixed iterations) ──
std::vector<SortStep> SortingAlgorithms::bogoSteps(std::vector<int>& arr) {
    std::vector<SortStep> steps;
    int n = static_cast<int>(arr.size());
    std::mt19937 rng(42); // fixed seed for reproducibility
    
    auto isSorted = [](const std::vector<int>& a) {
        for (size_t i = 1; i < a.size(); ++i)
            if (a[i - 1] > a[i]) return false;
        return true;
    };
    
    const int maxIterations = std::min(256, n * 2);
    
    for (int iter = 0; iter < maxIterations; ++iter) {
        // Shuffle
        for (int i = n - 1; i > 0; --i) {
            std::uniform_int_distribution<int> dist(0, i);
            int j = dist(rng);
            if (i != j) {
                std::swap(arr[i], arr[j]);
                steps.push_back({SortStep::Swap, i, j});
            }
        }
        
        // Check sorted (generate compare steps)
        for (int i = 0; i < n - 1; ++i) {
            steps.push_back({SortStep::Compare, i, i + 1});
        }
        
        if (isSorted(arr)) break;
    }
    
    return steps;
}

} // namespace SortSynth


