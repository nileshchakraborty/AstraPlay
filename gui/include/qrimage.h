#pragma once

#include <QString>

// Renders text as a QR code PNG data URL ("data:image/png;base64,...").
// Returns an empty string on failure (empty input, invalid size, data too long).
QString renderQrCodeDataUrl(const QString &text, int size);
