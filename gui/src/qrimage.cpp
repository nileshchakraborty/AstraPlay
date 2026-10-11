#include "qrimage.h"

#include <qrcodegen/qrcodegen.hpp>

#include <QBuffer>
#include <QByteArray>
#include <QImage>

QString renderQrCodeDataUrl(const QString &text, int size)
{
	if(text.isEmpty() || size <= 0)
		return QString();

	const int px = qBound(64, size, 1024);
	const QByteArray utf8 = text.toUtf8();

	try
	{
		const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(utf8.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
		const int modules = qr.getSize();
		const int border = 4;
		const int scale = qMax(1, px / (modules + border * 2));
		const int imgSize = (modules + border * 2) * scale;

		QImage img(imgSize, imgSize, QImage::Format_RGB888);
		img.fill(Qt::white);
		for(int y = -border; y < modules + border; y++)
		{
			for(int x = -border; x < modules + border; x++)
			{
				if(!qr.getModule(x, y))
					continue;
				for(int dy = 0; dy < scale; dy++)
				{
					for(int dx = 0; dx < scale; dx++)
						img.setPixel((x + border) * scale + dx, (y + border) * scale + dy, qRgb(0, 0, 0));
				}
			}
		}

		QByteArray png;
		QBuffer buf(&png);
		if(!buf.open(QIODevice::WriteOnly) || !img.save(&buf, "PNG"))
			return QString();
		return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
	}
	catch(...)
	{
		return QString();
	}
}
