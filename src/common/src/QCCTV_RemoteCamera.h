/*
 * Copyright (c) 2016 Alex Spataru
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE
 */

#ifndef _QCCTV_CAMERA_H
#define _QCCTV_CAMERA_H

#include <QObject>
#include <QHostAddress>

#include "QCCTV.h"

class QCCTV_RemoteCamera : public QObject {
    Q_OBJECT

signals:
    void connected (const QCCTV_RemoteCamera& camera);
    void disconnected (const QCCTV_RemoteCamera& camera);

public:
    QCCTV_RemoteCamera();
    ~QCCTV_RemoteCamera();

    Q_INVOKABLE int fps() const;
    Q_INVOKABLE QString group() const;
    Q_INVOKABLE QSize imageSize() const;
    Q_INVOKABLE QString cameraName() const;
    Q_INVOKABLE QImage currentImage() const;
    Q_INVOKABLE QHostAddress address() const;
    Q_INVOKABLE QCCTV_LightStatus lightStatus() const;
    Q_INVOKABLE QCCTV_CameraStatus cameraStatus() const;

public slots:
    void attemptConnection();
    void setFPS (const int fps);
    void setGroup (const QString& group);
    void setImageSize (const QSize size);
    void setAddress (const QHostAddress& address);
    void setLightStatus (const QCCTV_LightStatus status);
    void setCameraStatus (const QCCTV_CameraStatus status);

private:
    int m_fps;
    QSize m_size;
    QString m_name;
    QImage m_image;
    QString m_group;
    QHostAddress m_address;
    QCCTV_LightStatus m_light_status;
    QCCTV_CameraStatus m_camera_status;
};

#endif