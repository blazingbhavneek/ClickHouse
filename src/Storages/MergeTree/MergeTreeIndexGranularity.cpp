#include <memory>
#include <Storages/MergeTree/MergeTreeIndexGranularity.h>
#include <Storages/MergeTree/MergeTreeIndexGranularityAdaptive.h>
#include <Storages/MergeTree/MergeTreeIndexGranularityConstant.h>
#include <Storages/MergeTree/MergeTreeIndexGranularityInfo.h>
#include <IO/WriteHelpers.h>
#include "Common/Exception.h"
#include "Storages/MergeTree/MergeTreeDataPartType.h"

namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

namespace MergeTreeSetting
{
    extern const MergeTreeSettingsUInt64 index_granularity;
    extern const MergeTreeSettingsUInt64 index_granularity_bytes;
    extern const MergeTreeSettingsBool use_const_adaptive_granularity;
}

size_t MergeTreeIndexGranularity::getRowsCountInRange(const MarkRange & range) const
{
    return getRowsCountInRange(range.begin, range.end);
}

size_t MergeTreeIndexGranularity::getRowsCountInRanges(const MarkRanges & ranges) const
{
    size_t total = 0;
    for (const auto & range : ranges)
        total += getRowsCountInRange(range);
    return total;
}

size_t MergeTreeIndexGranularity::getMarksCountWithoutFinal() const
{
    size_t total = getMarksCount();
    if (total == 0)
        return total;
    return total - hasFinalMark();
}

size_t MergeTreeIndexGranularity::getLastMarkRows() const
{
    return getMarkRows(getMarksCount() - 1);
}

size_t MergeTreeIndexGranularity::getLastNonFinalMarkRows() const
{
    size_t last_mark_rows = getLastMarkRows();
    if (last_mark_rows != 0)
        return last_mark_rows;
    return getMarkRows(getMarksCount() - 2);
}

void MergeTreeIndexGranularity::addRowsToLastMark(size_t rows_count)
{
    if (hasFinalMark())
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Cannot add rows to final mark");
    }
    else if (empty())
    {
        appendMark(rows_count);
    }
    else
    {
        adjustLastMark(getLastMarkRows() + rows_count);
    }
}

size_t computeIndexGranularityForBlock(
    size_t rows_in_block,
    size_t bytes_in_block,
    size_t index_granularity_bytes,
    size_t fixed_index_granularity_rows,
    bool blocks_are_granules,
    bool can_use_adaptive_index_granularity)
{
    size_t index_granularity_for_block;

    if (!can_use_adaptive_index_granularity)
    {
        index_granularity_for_block = fixed_index_granularity_rows;
    }
    else
    {
        if (blocks_are_granules)
        {
            index_granularity_for_block = rows_in_block;
        }
        else if (bytes_in_block >= index_granularity_bytes)
        {
            size_t granules_in_block = bytes_in_block / index_granularity_bytes;
            index_granularity_for_block = rows_in_block / granules_in_block;
        }
        else
        {
            size_t size_of_row_in_bytes = std::max(bytes_in_block / rows_in_block, 1UL);
            index_granularity_for_block = index_granularity_bytes / size_of_row_in_bytes;
        }
    }

    /// We should be less or equal than fixed index granularity.
    /// But if block size is a granule size then do not adjust it.
    /// Granularity greater than fixed granularity might come from compact part.
    if (!blocks_are_granules)
        index_granularity_for_block = std::min(fixed_index_granularity_rows, index_granularity_for_block);

    /// Very rare case when index granularity bytes less than single row.
    if (index_granularity_for_block == 0)
        index_granularity_for_block = 1;

    return index_granularity_for_block;
}

MergeTreeIndexGranularityPtr createMergeTreeIndexGranularity(
    size_t rows_in_block,
    size_t bytes_in_block,
    const MergeTreeSettings & settings,
    const MergeTreeIndexGranularityInfo & info,
    bool blocks_are_granules)
{
    bool use_adaptive_granularity = info.mark_type.adaptive;
    bool use_const_adaptive_granularity = settings[MergeTreeSetting::use_const_adaptive_granularity];
    bool is_compact_part = info.mark_type.part_type == MergeTreeDataPartType::Compact;

    if (blocks_are_granules || is_compact_part || (use_adaptive_granularity && !use_const_adaptive_granularity))
        return std::make_shared<MergeTreeIndexGranularityAdaptive>();

    size_t computed_granularity = computeIndexGranularityForBlock(
        rows_in_block,
        bytes_in_block,
        settings[MergeTreeSetting::index_granularity_bytes],
        settings[MergeTreeSetting::index_granularity],
        blocks_are_granules,
        use_adaptive_granularity);

    return std::make_shared<MergeTreeIndexGranularityConstant>(computed_granularity);
}

}
