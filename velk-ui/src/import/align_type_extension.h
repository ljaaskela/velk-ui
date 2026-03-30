#ifndef VELK_UI_ALIGN_TYPE_EXTENSION_H
#define VELK_UI_ALIGN_TYPE_EXTENSION_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>

namespace velk_ui {

class AlignTypeExtension
    : public velk::ext::ObjectCore<AlignTypeExtension, velk::IImporterTypeExtension>
{
public:
    VELK_CLASS_UID("d7e8f9a0-b1c2-3d4e-5f60-718293a4b5c6", "AlignTypeExtension");

    velk::array_view<velk::Uid> supported_types() const override;
    velk::IAny::Ptr deserialize(velk::Uid type_uid, const velk::IImportData& data) const override;
};

} // namespace velk_ui

#endif // VELK_UI_ALIGN_TYPE_EXTENSION_H
