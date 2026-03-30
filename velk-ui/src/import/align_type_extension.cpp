#include "align_type_extension.h"

#include <velk/api/any.h>
#include <velk-ui/types.h>

namespace velk_ui {

velk::array_view<velk::Uid> AlignTypeExtension::supported_types() const
{
    static const velk::Uid types[] = {
        velk::type_uid<HAlign>(),
        velk::type_uid<VAlign>()
    };
    return { types, 2 };
}

velk::IAny::Ptr AlignTypeExtension::deserialize(velk::Uid uid, const velk::IImportData& data) const
{
    if (uid == velk::type_uid<HAlign>()) {
        if (data.kind() == velk::IImportData::Kind::String) {
            auto s = data.as_string();
            if (s == "left")   return velk::Any<HAlign>(HAlign::Left);
            if (s == "center") return velk::Any<HAlign>(HAlign::Center);
            if (s == "right")  return velk::Any<HAlign>(HAlign::Right);
        }
        if (data.kind() == velk::IImportData::Kind::Number) {
            return velk::Any<HAlign>(static_cast<HAlign>(static_cast<uint8_t>(data.as_number())));
        }
    }

    if (uid == velk::type_uid<VAlign>()) {
        if (data.kind() == velk::IImportData::Kind::String) {
            auto s = data.as_string();
            if (s == "top")    return velk::Any<VAlign>(VAlign::Top);
            if (s == "center") return velk::Any<VAlign>(VAlign::Center);
            if (s == "bottom") return velk::Any<VAlign>(VAlign::Bottom);
        }
        if (data.kind() == velk::IImportData::Kind::Number) {
            return velk::Any<VAlign>(static_cast<VAlign>(static_cast<uint8_t>(data.as_number())));
        }
    }

    return {};
}

} // namespace velk_ui
