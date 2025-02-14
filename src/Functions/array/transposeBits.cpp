#include <Functions/IFunction.h>
#include <Functions/FunctionFactory.h>
#include <Functions/FunctionHelpers.h>
#include <DataTypes/DataTypeArray.h>
#include <Columns/ColumnArray.h>
#include <Columns/ColumnNullable.h>
#include <Columns/ColumnString.h>
#include <Columns/ColumnFixedString.h>
#include <Common/typeid_cast.h>
#include <Common/assert_cast.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int ILLEGAL_COLUMN;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}


class FunctionTransposeBits : public IFunction
{
public:
    static constexpr auto name = "transposeBits";
    static FunctionPtr create(ContextPtr) { return std::make_shared<FunctionTransposeBits>(); }

    String getName() const override { return name; }

    size_t getNumberOfArguments() const override { return 1; }
    bool useDefaultImplementationForConstants() const override { return true; }

    bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return true; }

    DataTypePtr getReturnTypeImpl(const DataTypes & arguments) const override
    {
        const DataTypeArray * array_type = checkAndGetDataType<DataTypeArray>(arguments[0].get());
        if (!array_type)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument for function {} must be array.", getName());

        return arguments[0];
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr & result_type, size_t) const override;

private:
    template <typename T>
    static bool executeNumber(const IColumn & src_data, const ColumnArray::Offsets & src_offsets, IColumn & res_data);

    static bool executeFixedString(const IColumn & src_data, const ColumnArray::Offsets & src_offsets, IColumn & res_data);
    static bool executeString(const IColumn & src_data, const ColumnArray::Offsets & src_array_offsets, IColumn & res_data);
    static bool executeGeneric(const IColumn & src_data, const ColumnArray::Offsets & src_array_offsets, IColumn & res_data);
};


ColumnPtr FunctionTransposeBits::executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t) const
{
    const ColumnArray * array = checkAndGetColumn<ColumnArray>(arguments[0].column.get());
    if (!array)
        throw Exception(ErrorCodes::ILLEGAL_COLUMN, "Illegal column {} of first argument of function {}",
            arguments[0].column->getName(), getName());

    auto res_ptr = array->cloneEmpty();
    ColumnArray & res = assert_cast<ColumnArray &>(*res_ptr);
    res.getOffsetsPtr() = array->getOffsetsPtr();

    const IColumn & src_data = array->getData();
    const ColumnArray::Offsets & offsets = array->getOffsets();

    IColumn & res_data = res.getData();

    const ColumnNullable * src_nullable_col = typeid_cast<const ColumnNullable *>(&src_data);
    ColumnNullable * res_nullable_col = typeid_cast<ColumnNullable *>(&res_data);

    const IColumn * src_inner_col = src_nullable_col ? &src_nullable_col->getNestedColumn() : &src_data;
    IColumn * res_inner_col = res_nullable_col ? &res_nullable_col->getNestedColumn() : &res_data;

    false // NOLINT
        || executeNumber<Float32>(*src_inner_col, offsets, *res_inner_col)
        || executeNumber<Float64>(*src_inner_col, offsets, *res_inner_col);

    chassert(bool(src_nullable_col) == bool(res_nullable_col));

    if (src_nullable_col)
        throw Exception(ErrorCodes::ILLEGAL_COLUMN, "Illegal column {} of null map of the first argument of function {}",
            src_nullable_col->getNullMapColumn().getName(), getName());

    return res_ptr;
}


template <typename T>
bool FunctionTransposeBits::executeNumber(const IColumn & src_data, const ColumnArray::Offsets & src_offsets, IColumn & res_data)
{
    int num_bits = 0;

    if (std::is_same_v<T, float>) {
        num_bits = 32;
    } else num_bits = 64;

    if (const ColumnVector<T> * src_data_concrete = checkAndGetColumn<ColumnVector<T>>(&src_data))
    {
        const PaddedPODArray<T> & src_vec = src_data_concrete->getData();
        PaddedPODArray<T> & res_vec = typeid_cast<ColumnVector<T> &>(res_data).getData();
        res_vec.resize(src_data.size());

        size_t size = src_offsets.size();
        ColumnArray::Offset src_prev_offset = 0;

        for (size_t i = 0; i < size; ++i)
        {

            const auto * src = &src_vec[src_prev_offset];
            const auto * curr = src;
            const auto * src_end = &src_vec[src_offsets[i]];

            int array_size = static_cast<int>(src_end - src);

            if (src == src_end or array_size==0)
                continue;

            while (curr < src_end)
            {
                for(int j=0; j<num_bits; j++){
                    int idx = ((static_cast<int>(curr - src)) + j * array_size) / num_bits;
                    int nthbit = ((static_cast<int>(curr - src)) + j * array_size) % num_bits;
                    auto * tgt = &res_vec[idx];
                    if(num_bits==32){
                        uint32_t bits32=0;
                        memcpy(&bits32, curr, sizeof(bits32));
                        bool bit_to_set = ( bits32 >> j) & 1;
                        bits32=0;
                        memcpy(&bits32, tgt, sizeof(bits32));  
                        bits32 |= (static_cast<uint32_t>(bit_to_set) << (31-nthbit));
                        memcpy(tgt, &bits32, sizeof(bits32));
                    }
                    else{
                        uint64_t bits=0;
                        memcpy(&bits, curr, sizeof(bits));
                        bool bit_to_set = ( bits >> j) & 1;
                        bits=0;
                        memcpy(&bits, tgt, sizeof(bits));  
                        bits |= (static_cast<uint64_t>(bit_to_set) << (63-nthbit));
                        memcpy(tgt, &bits, sizeof(bits));
                    }
                }
                curr++;
            }

            src_prev_offset = src_offsets[i];
        }

        return true;
    }
    return false;
}


REGISTER_FUNCTION(TransposeBits)
{
    factory.registerFunction<FunctionTransposeBits>();
}

}
