#pragma once
#include <Storages/MergeTree/MergeTreeIndexGranularity.h>

namespace DB
{

class MergeTreeIndexGranularityConstant : public MergeTreeIndexGranularity
{
private:
    size_t constant_granularity;
    size_t last_mark_granularity;

    size_t num_marks_without_final = 0;
    bool has_final_mark = false;

public:
    MergeTreeIndexGranularityConstant() = default;
    explicit MergeTreeIndexGranularityConstant(size_t constant_granularity_);
    MergeTreeIndexGranularityConstant(size_t constant_granularity_, size_t last_mark_granularity_, size_t num_marks_without_final_, bool has_final_mark_);

    size_t getRowsCountInRange(size_t begin, size_t end) const override;
    size_t countMarksForRows(size_t from_mark, size_t number_of_rows) const override;
    size_t countRowsForRows(size_t from_mark, size_t number_of_rows, size_t offset_in_rows) const override;

    size_t getMarksCount() const override;
    size_t getTotalRows() const override;

    size_t getMarkRows(size_t mark_index) const override;
    size_t getMarkStartingRow(size_t mark_index) const override;
    bool hasFinalMark() const override { return has_final_mark; }

    void appendMark(size_t rows_count) override;
    void adjustLastMark(size_t rows_count) override;
    void shrinkToFitInMemory() override {}
    std::shared_ptr<MergeTreeIndexGranularity> optimize() const override { return nullptr; }

    std::string describe() const override;
};

}

