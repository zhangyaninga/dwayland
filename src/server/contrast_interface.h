/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class ContrastManagerInterfacePrivate;
class ContrastInterfacePrivate;

/**
 * @brief Represents the Global for org_kde_kwin_contrast_manager interface.
 *
 * This class creates ContrastInterfaces and attaches them to SurfaceInterfaces.
 *
 * @see ContrastInterface
 * @see SurfaceInterface
 */
class KWAYLANDSERVER_EXPORT ContrastManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ContrastManagerInterface(Display *display, QObject *parent = nullptr);
    ~ContrastManagerInterface() override;

    void remove();

private:
    QScopedPointer<ContrastManagerInterfacePrivate> d;
};

/**
 * @brief Represents the Resource for the org_kde_kwin_contrast interface.
 *
 * Instances of this class are only generated by the ContrastManagerInterface.
 * The ContrastInterface gets attached to a SurfaceInterface and can be assessed
 * from there using @link SurfaceInterface::contrast() @endlink. Please note that
 * the ContrastInterface is only available on the SurfaceInterface after it has been
 * committed.
 *
 * @see ContrastManagerInterface
 * @see SurfaceInterface
 */
class KWAYLANDSERVER_EXPORT ContrastInterface : public QObject
{
    Q_OBJECT
public:
    ~ContrastInterface() override;

    QRegion region() const;
    qreal contrast() const;
    qreal intensity() const;
    qreal saturation() const;

private:
    explicit ContrastInterface(wl_resource *resource);
    friend class ContrastManagerInterfacePrivate;

    QScopedPointer<ContrastInterfacePrivate> d;
};

}
