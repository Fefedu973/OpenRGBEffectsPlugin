/*---------------------------------------------------------*\
| ScreenCapturer.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Screen Capturer Base Class       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QRect>

enum ScreenCapturerError {
    Cancelled,
    Other
};

class ScreenCapturer : public QObject
{
    Q_OBJECT

public:
    ScreenCapturer(QObject* parent = nullptr): QObject(parent){};
    ~ScreenCapturer() {};

    void SetFrameRate(unsigned int value) {framerate = value;};

    virtual void Init(const QString& restore_token = "", bool auto_start = false) { (void)restore_token; (void)auto_start; };
    virtual void SetToken(const QString& restore_token = "") { (void)restore_token; };
    virtual void Start() {};
    virtual void Stop() {};
    virtual void SetScreen(int) {};

protected:
    unsigned int framerate = 60;

signals:
    void OnStarted();
    void OnStopped();

    void OnRestoreTokenProvided(const QString& token);
    void OnImage(const QImage& img);
    void OnError(const ScreenCapturerError& err, const QString& message);

};
