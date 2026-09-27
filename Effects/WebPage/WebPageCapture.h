/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QImage>
#include <QUrl>
#include <memory>

// All browser operations and callbacks belong to the Qt GUI STA. Consumers get
// an owned immutable QImage, never a browser buffer or a HWND screenshot.
class WebPageCapture : public QObject
{
    Q_OBJECT
public:
    explicit WebPageCapture(QObject* parent = nullptr);
    ~WebPageCapture() override;
    static bool ValidUrl(const QUrl& url);
    static bool ValidSize(int width, int height);
    static QImage DecodePng(const QByteArray& bytes, int width, int height);
    void Start(const QUrl& url, int width, int height, int fps);
    void Stop();
signals:
    void FrameReady(const QImage& image);
    void Status(const QString& text);
private:
    struct State;
    std::shared_ptr<State> state;
};
