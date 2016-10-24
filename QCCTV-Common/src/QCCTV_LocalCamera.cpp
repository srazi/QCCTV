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

#include "QCCTV.h"
#include "QCCTV_LocalCamera.h"
#include "QCCTV_FrameGrabber.h"

#include <QFont>
#include <QThread>
#include <QBuffer>
#include <QPainter>
#include <QCameraInfo>
#include <QCameraExposure>
#include <QCameraImageCapture>

/**
 * Initializes the class by:
 *     - Bind receiver sockets
 *     - Starting the broadcast service
 *     - Generating the default image
 *     - Configuring the TCP server
 *     - Configuring the frame grabber
 */
QCCTV_LocalCamera::QCCTV_LocalCamera()
{
    /* Initialize pointers */
    m_camera = NULL;
    m_capture = NULL;

    /* Configure TCP listener socket */
    connect (&m_server, SIGNAL (newConnection()),
             this,        SLOT (acceptConnection()));
    m_server.listen (QHostAddress::Any, QCCTV_STREAM_PORT);

    /* Setup the frame grabber */
    connect (&m_frameGrabber, SIGNAL (newFrame (QImage)),
             this,              SLOT (changeImage (QImage)));

    /* Set default values */
    setFPS (QCCTV_DEFAULT_FPS);
    setName ("Unknown Camera");
    setCameraStatus (QCCTV_CAMSTATUS_DEFAULT);
    setFlashlightStatus (QCCTV_FLASHLIGHT_OFF);

    /* Set default image */
    QPixmap pixmap = QPixmap (320, 240);
    pixmap.fill (QColor ("#000").rgb());
    QPainter painter (&pixmap);

    /* Set default image text */
    painter.setPen (Qt::white);
    painter.setFont (QFont ("Arial"));
    painter.drawText (QRectF (0, 0, 320, 240),
                      Qt::AlignCenter, "NO CAMERA IMAGE");

    /* Get generated image buffer */
    m_image = pixmap.toImage();

    /* Start the event loops */
    QTimer::singleShot (1000, Qt::CoarseTimer, this, SLOT (update()));
    QTimer::singleShot (1000, Qt::CoarseTimer, this, SLOT (broadcastInfo()));
}

/**
 * Closes all the sockets and the frontal camera device
 */
QCCTV_LocalCamera::~QCCTV_LocalCamera()
{
    foreach (QTcpSocket* socket, m_sockets) {
        socket->abort();
        socket->deleteLater();
    }

    m_server.close();
    m_sockets.clear();
    m_broadcastSocket.close();
}

/**
 * Returns the current FPS of the camera
 */
int QCCTV_LocalCamera::fps() const
{
    return m_fps;
}

/**
 * Returns the current status of the flashlight (on or off)
 */
QCCTV_LightStatus QCCTV_LocalCamera::lightStatus() const
{
    return (QCCTV_LightStatus) m_flashlightStatus;
}

/**
 * Returns \c true if we should send a grayscale image to the QCCTV Station
 */
bool QCCTV_LocalCamera::isGrayscale() const
{
    return m_frameGrabber.isGrayscale();
}

/**
 * Returns the shrink ratio used to resize the image to send it to the
 * local network
 */
qreal QCCTV_LocalCamera::shrinkRatio() const
{
    return m_frameGrabber.shrinkRatio();
}

/**
 * Returns \c true if the flash light is on
 */
bool QCCTV_LocalCamera::flashlightOn() const
{
    return lightStatus() == QCCTV_FLASHLIGHT_ON;
}

/**
 * Returns \c true if the flash light is off
 */
bool QCCTV_LocalCamera::flashlightOff() const
{
    return lightStatus() == QCCTV_FLASHLIGHT_OFF;
}

/**
 * Returns the user-assigned name of the camera
 */
QString QCCTV_LocalCamera::cameraName() const
{
    return m_name;
}

/**
 * Returns the image that is currently being sent to the CCTV stations
 */
QImage QCCTV_LocalCamera::currentImage() const
{
    return m_image;
}

/**
 * Returns the current status of QCCTV in a string
 */
QString QCCTV_LocalCamera::statusString() const
{
    return QCCTV_STATUS_STRING (cameraStatus());
}

/**
 * Returns \c true if the camera is ready for saving photos
 */
bool QCCTV_LocalCamera::readyForCapture() const
{
    if (m_camera && m_capture)
        return m_capture->isReadyForCapture();

    return false;
}

/**
 * Returns \c true if the camera's flashlight is ready for use
 */
bool QCCTV_LocalCamera::flashlightAvailable() const
{
    if (m_camera)
        return m_camera->exposure()->isFlashReady();

    return false;
}

/**
 * Returns a list with all the connected QCCTV stations to this camera
 */
QStringList QCCTV_LocalCamera::connectedHosts() const
{
    QStringList list;

    foreach (QTcpSocket* socket, m_sockets)
        list.append (socket->peerAddress().toString());

    return list;
}

/**
 * Returns the current status of the camera itself
 */
int QCCTV_LocalCamera::cameraStatus() const
{
    return m_cameraStatus;
}

/**
 * Attempts to take a photo using the current camera
 */
void QCCTV_LocalCamera::takePhoto()
{
    if (readyForCapture())
        m_capture->capture();
}

/**
 * Forces the camera to re-focus the image
 */
void QCCTV_LocalCamera::focusCamera()
{
    if (m_camera) {
        m_camera->searchAndLock (QCamera::LockFocus);
        emit focusStatusChanged();
    }
}

/**
 * Attempts to turn on the camera flashlight/torch
 */
void QCCTV_LocalCamera::turnOnFlashlight()
{
    setFlashlightStatus (QCCTV_FLASHLIGHT_ON);
}

/**
 * Attempts to turn off the camera flashlight/torch
 */
void QCCTV_LocalCamera::turnOffFlashlight()
{
    setFlashlightStatus (QCCTV_FLASHLIGHT_OFF);
}

/**
 * Changes the FPS of the camera
 */
void QCCTV_LocalCamera::setFPS (const int fps)
{
    if (m_fps != QCCTV_GET_VALID_FPS (fps)) {
        m_fps = QCCTV_GET_VALID_FPS (fps);
        emit fpsChanged();
    }
}

/**
 * Changes the camera used to capture images to send to the QCCTV network
 */
void QCCTV_LocalCamera::setCamera (QCamera* camera)
{
    if (camera) {
        m_camera = camera;
        m_camera->setViewfinder (&m_frameGrabber);
        m_camera->setCaptureMode (QCamera::CaptureStillImage);
        m_camera->start();

        if (m_capture)
            delete m_capture;

        m_capture = new QCameraImageCapture (m_camera);
    }
}

/**
 * Changes the name assigned to this camera
 */
void QCCTV_LocalCamera::setName (const QString& name)
{
    if (m_name != name) {
        m_name = name;
        emit cameraNameChanged();
    }
}

/**
 * Enables or disables sending a grayscale image to the QCCTV Stations
 * in the local network
 */
void QCCTV_LocalCamera::setGrayscale (const bool gray)
{
    m_frameGrabber.setGrayscale (gray);
}

/**
 * Changes the shrink factor used to resize the image before sending it to
 * the QCCTV Stations in the local network
 */
void QCCTV_LocalCamera::setShrinkRatio (const qreal ratio)
{
    m_frameGrabber.setShrinkRatio (ratio);
}

/**
 * Obtains a new image from the camera and updates the camera status
 */
void QCCTV_LocalCamera::update()
{
    /* Generate a new camera status */
    updateStatus();

    /* Construct a new stream of data to send */
    generateDataStream();
    sendCameraData();

    /* Call this function again in several milliseconds */
    QTimer::singleShot (1000 / fps(), Qt::PreciseTimer, this, SLOT (update()));
}

/**
 * Updates the status code of the camera
 */
void QCCTV_LocalCamera::updateStatus()
{
    /* No camera present */
    if (!m_camera)
        addStatusFlag (QCCTV_CAMSTATUS_VIDEO_FAILURE);

    /* Check if camera is available */
    else if (m_camera->status() != QCamera::ActiveStatus)
        addStatusFlag (QCCTV_CAMSTATUS_VIDEO_FAILURE);

    /* Video is OK, ensure that VIDEO_FAILURE is removed */
    else
        removeStatusFlag (QCCTV_CAMSTATUS_VIDEO_FAILURE);

    /* Check if flash light is available */
    if (!flashlightAvailable())
        addStatusFlag (QCCTV_CAMSTATUS_LIGHT_FAILURE);
    else
        removeStatusFlag (QCCTV_CAMSTATUS_LIGHT_FAILURE);
}

/**
 * Sends the generated data packet to all connected QCCTV Stations.
 * The data is sent 'chunk by chunk' to avoid errors and ensure that the
 * generated data is interpreted correctly by the station(s).
 */
void QCCTV_LocalCamera::sendCameraData()
{
    foreach (QTcpSocket* socket, m_sockets)
        socket->write (m_dataStream);

    m_dataStream.clear();
}

/**
 * Creates and sends a new packet that announces the existence of this
 * camera to the local network
 */
void QCCTV_LocalCamera::broadcastInfo()
{
    QString str = "QCCTV_DISCOVERY_SERVICE";

    m_broadcastSocket.writeDatagram (str.toUtf8(),
                                     QHostAddress::Broadcast,
                                     QCCTV_DISCOVERY_PORT);

    QTimer::singleShot (QCCTV_DISCVRY_PKT_TIMING, Qt::PreciseTimer,
                        this, SLOT (broadcastInfo()));
}

/**
 * Closes and un-registers a station when the TCP connection is aborted
 */
void QCCTV_LocalCamera::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*> (sender());

    if (socket) {
        socket->close();
        socket->deleteLater();
        m_sockets.removeAt (m_sockets.indexOf (socket));
    }
}

/**
 * Called when a CCTV station wants to receive images from this camera
 * This function shall configure the TCP socket used for streaming the
 * images and receiving commands from the station
 */
void QCCTV_LocalCamera::acceptConnection()
{
    while (m_server.hasPendingConnections()) {
        m_sockets.append (m_server.nextPendingConnection());

        m_sockets.last()->setSocketOption (QTcpSocket::LowDelayOption, true);
        connect (m_sockets.last(), SIGNAL (disconnected()),
                 this,               SLOT (onDisconnected()));
        connect (m_sockets.last(), SIGNAL (readyRead()),
                 this,               SLOT (readCommandPacket()));
    }
}

/**
 * Interprets a command packet issued by the QCCTV station in the LAN.
 *
 * This packet contains the following data/instructions:
 *
 * - A new FPS to use
 * - The new light status
 * - A force focus request
 */
void QCCTV_LocalCamera::readCommandPacket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*> (sender());

    if (socket) {
        /* Read packet */
        QByteArray data = socket->readAll();
        if (data.size() != 3)
            return;

        /* Change the FPS */
        setFPS ((int) data.at (0));

        /* Change the light status */
        setFlashlightStatus ((QCCTV_LightStatus) data.at (1));

        /* Focus the camera */
        if (data.at (2) == QCCTV_FORCE_FOCUS)
            focusCamera();
    }
}

/**
 * Generates a byte array with the following information:
 *
 * - The camera name
 * - The FPS of the camera
 * - The light status of the camera
 * - The camera status
 * - The latest camera image
 *
 * This byte array will be sent to all connected QCCTV Stations in the LAN
 */
void QCCTV_LocalCamera::generateDataStream()
{
    /* Only generate the data stream if the previous one has been sent */
    if (m_dataStream.isEmpty()) {
        /* Add camera name */
        m_dataStream.append (cameraName().length());
        m_dataStream.append (cameraName());

        /* Add FPS, light status and camera status */
        m_dataStream.append (fps());
        m_dataStream.append (lightStatus());
        m_dataStream.append (cameraStatus());

        /* Add image to data stream */
        if (!currentImage().isNull()) {
            QByteArray img;
            QBuffer buffer (&img);
            currentImage().save (&buffer, QCCTV_IMAGE_FORMAT, 50);
            m_dataStream.append (img);
            buffer.close();
        }

        /* Add end bytes */
        m_dataStream.append (QCCTV_EOD);
    }
}

/**
 * Replaces the current image and notifies the application
 */
void QCCTV_LocalCamera::changeImage (const QImage& image)
{
    if (!image.isNull()) {
        m_image = image;
        emit imageChanged();
    }
}

/**
 * Registers the given \a status flag to the operation status flags
 */
void QCCTV_LocalCamera::addStatusFlag (const QCCTV_CameraStatus status)
{
    if (! (m_cameraStatus & status)) {
        m_cameraStatus |= status;
        emit cameraStatusChanged();
    }
}

/**
 * Overrides the camera status flags with the given \a status
 */
void QCCTV_LocalCamera::setCameraStatus (const QCCTV_CameraStatus status)
{
    m_cameraStatus = status;
    emit cameraStatusChanged();
}

/**
 * Removes the given \a status flag from the operation status of the camera
 */
void QCCTV_LocalCamera::removeStatusFlag (const QCCTV_CameraStatus status)
{
    if (m_cameraStatus & status) {
        m_cameraStatus ^= status;
        emit cameraStatusChanged();
    }
}

/**
 * Changes the light status of the camera
 */
void QCCTV_LocalCamera::setFlashlightStatus (const QCCTV_LightStatus status)
{
    if (m_flashlightStatus != status) {
        m_flashlightStatus = status;

        /* The camera is not available */
        if (!m_camera)
            return;

        /* The flashlight is not available */
        if (!flashlightAvailable())
            return;

        /* Turn on the flashlight (and focus the camera) */
        if (flashlightOn()) {
            m_camera->exposure()->setFlashMode (QCameraExposure::FlashVideoLight);
            focusCamera();
        }

        /* Turn off the flashlight */
        else
            m_camera->exposure()->setFlashMode (QCameraExposure::FlashOff);

        /* Tell everyone that we changed the flashlight status */
        emit lightStatusChanged();
    }
}
