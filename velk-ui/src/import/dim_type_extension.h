#ifndef VELK_UI_DIM_TYPE_EXTENSION_H
#define VELK_UI_DIM_TYPE_EXTENSION_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/string.h>

#include <velk-ui/plugin.h>
#include <velk-ui/types.h>

namespace velk_ui {

dim parse_dim(velk::string_view str);

class DimTypeExtension
    : public velk::ext::ObjectCore<DimTypeExtension, velk::IImporterTypeExtension>
{
public:
    VELK_CLASS_UID(ClassId::Import::DimTypeExtension, "DimTypeExtension");

    velk::array_view<velk::Uid> supported_types() const override;
    velk::IAny::Ptr deserialize(velk::Uid type_uid, const velk::IImportData& data) const override;
};

} // namespace velk_ui

#endif // VELK_UI_DIM_TYPE_EXTENSION_H
