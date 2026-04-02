#ifndef VELK_UI_ALIGN_TYPE_EXTENSION_H
#define VELK_UI_ALIGN_TYPE_EXTENSION_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>

#include <velk-ui/plugin.h>

namespace velk_ui {

class AlignTypeExtension
    : public velk::ext::ObjectCore<AlignTypeExtension, velk::IImporterTypeExtension>
{
public:
    VELK_CLASS_UID(ClassId::Import::AlignTypeExtension, "AlignTypeExtension");

    velk::array_view<velk::Uid> supported_types() const override;
    velk::IAny::Ptr deserialize(velk::Uid type_uid, const velk::IImportData& data) const override;
};

} // namespace velk_ui

#endif // VELK_UI_ALIGN_TYPE_EXTENSION_H
