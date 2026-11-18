// about: what the app does, versions, creators, licenses, repositories.
#pragma once

#include "ui/Tab.hpp"

namespace dp::ui {

class AboutTab : public Tab {
public:
    AboutTab();
    std::string key() const override { return "about"; }
    std::string title() const override;
};

}  // namespace dp::ui
