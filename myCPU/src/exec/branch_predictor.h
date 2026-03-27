#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct PredictorQueryResult {
    bool valid{false};
    bool predicted_taken{false};
    uint64_t predicted_target{0};
    bool table_hit{false};
};

struct PredictorUpdate {
    uint64_t pc{0};
    uint32_t raw{0};
    bool taken{false};
    uint64_t target{0};
};

struct PredictorStats {
    uint64_t total_predictions{0};
    uint64_t correct_predictions{0};
    uint64_t mispredictions{0};
};

class BranchPredictor {
public:
    PredictorQueryResult query(uint64_t pc, uint32_t raw);
    void update(const PredictorUpdate& update);
    void reset();
    PredictorStats stats() const;

private:
    struct Entry {
        bool valid{false};
        uint64_t pc{0};
        uint8_t counter{1};
        uint64_t target{0};
    };

    static constexpr size_t kTableSize = 64;

    static size_t index_for(uint64_t pc);

    std::array<Entry, kTableSize> table_{};
    PredictorStats stats_{};
};
