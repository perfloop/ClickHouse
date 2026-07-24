// This focused benchmark uses exact block-column names only. The production
// lookup first resolves that exact-name path; these definitions keep its unused
// subcolumn fallback out of the benchmark's deliberately small build slice.
#include <DataTypes/IDataType.h>

namespace DB
{

DataTypePtr IDataType::tryGetSubcolumnType(std::string_view) const
{
    return {};
}

ColumnPtr IDataType::tryGetSubcolumn(std::string_view, const ColumnPtr &) const
{
    return {};
}

}
