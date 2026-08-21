#include "WindowsAppearance.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

bool WindowsAppearance::isHighContrast()
{
#ifdef Q_OS_WIN
    HIGHCONTRASTW highContrast{sizeof(HIGHCONTRASTW)};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast),
                                 &highContrast, 0)
        && (highContrast.dwFlags & HCF_HIGHCONTRASTON);
#else
    return false;
#endif
}

void WindowsAppearance::applyToWindow(QWidget *window, bool dark, bool followSystem,
                                      const QColor &caption, const QColor &text,
                                      const QColor &border)
{
#ifdef Q_OS_WIN
    if (!window || !window->isWindow())
        return;

    const HWND handle = reinterpret_cast<HWND>(window->winId());
    if (followSystem) {
        const COLORREF defaultColor = DWMWA_COLOR_DEFAULT;
        DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR,
                              &defaultColor, sizeof(defaultColor));
        DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR,
                              &defaultColor, sizeof(defaultColor));
        DwmSetWindowAttribute(handle, DWMWA_BORDER_COLOR,
                              &defaultColor, sizeof(defaultColor));

        if (isHighContrast()) {
            const BOOL useDarkMode = FALSE;
            DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                  &useDarkMode, sizeof(useDarkMode));
            return;
        }

        const BOOL useDarkMode = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE,
                              &useDarkMode, sizeof(useDarkMode));
        return;
    }

    const BOOL useDarkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &useDarkMode, sizeof(useDarkMode));

    const COLORREF captionColor = RGB(caption.red(), caption.green(), caption.blue());
    const COLORREF textColor = RGB(text.red(), text.green(), text.blue());
    const COLORREF borderColor = RGB(border.red(), border.green(), border.blue());
    DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR,
                          &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR,
                          &textColor, sizeof(textColor));
    DwmSetWindowAttribute(handle, DWMWA_BORDER_COLOR,
                          &borderColor, sizeof(borderColor));
#else
    Q_UNUSED(window)
    Q_UNUSED(dark)
    Q_UNUSED(followSystem)
    Q_UNUSED(caption)
    Q_UNUSED(text)
    Q_UNUSED(border)
#endif
}
