#pragma once

#include <Windows.h>

class ITabBarHostOwner {
public:
    virtual ~ITabBarHostOwner() = default;
    virtual HWND GetHostWindow() const noexcept = 0;
    virtual HWND GetHostRebarWindow() const noexcept = 0;
    virtual void NotifyTabHostFocusChange(BOOL hasFocus) = 0;
};
