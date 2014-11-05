/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "subcompositor_interface.h"
#include "subsurface_interface_p.h"
#include "display.h"
#include "surface_interface_p.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class SubCompositorInterface::Private
{
public:
    Private(SubCompositorInterface *q, Display *d);
    void create();

    Display *display;
    wl_global *compositor;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void subsurface(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *parent);

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void unbind(wl_resource *resource);
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void subsurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *parent);

    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    SubCompositorInterface *q;
    static const struct wl_subcompositor_interface s_interface;
};

const struct wl_subcompositor_interface SubCompositorInterface::Private::s_interface = {
    destroyCallback,
    subsurfaceCallback
};

SubCompositorInterface::Private::Private(SubCompositorInterface *q, Display *d)
    : display(d)
    , compositor(nullptr)
    , q(q)
{
}

void SubCompositorInterface::Private::create()
{
    Q_ASSERT(!compositor);
    compositor = wl_global_create(*display, &wl_subcompositor_interface, s_version, this, bind);
}

void SubCompositorInterface::Private::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    reinterpret_cast<SubCompositorInterface::Private*>(data)->bind(client, version, id);
}

void SubCompositorInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_subcompositor_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void SubCompositorInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

void SubCompositorInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void SubCompositorInterface::Private::subsurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface, wl_resource *sparent)
{
    cast(resource)->subsurface(client, resource, id, surface, sparent);
}

void SubCompositorInterface::Private::subsurface(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *nativeSurface, wl_resource *nativeParentSurface)
{
    Q_UNUSED(client)
    SurfaceInterface *surface = SurfaceInterface::get(nativeSurface);
    SurfaceInterface *parentSurface = SurfaceInterface::get(nativeParentSurface);
    if (!surface || !parentSurface) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Surface or parent surface not found");
        return;
    }
    if (surface == parentSurface) {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Cannot become sub composite to same surface");
        return;
    }
    // TODO: add check that surface is not already used in an interface (e.g. Shell)
    // TODO: add check that parentSurface is not a child of surface
    SubSurfaceInterface *s = new SubSurfaceInterface(q);
    s->d->create(client, wl_resource_get_version(resource), id, surface, parentSurface);
    if (!s->subSurface()) {
        wl_resource_post_no_memory(resource);
        delete s;
        return;
    }
    emit q->subSurfaceCreated(s);
}

SubCompositorInterface::SubCompositorInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(this, display))
{
}

SubCompositorInterface::~SubCompositorInterface()
{
    destroy();
}

void SubCompositorInterface::destroy()
{
    if (!d->compositor) {
        return;
    }
    wl_global_destroy(d->compositor);
    d->compositor = nullptr;
}

void SubCompositorInterface::create()
{
    d->create();
}

bool SubCompositorInterface::isValid() const
{
    return d->compositor != nullptr;
}

const struct wl_subsurface_interface SubSurfaceInterface::Private::s_interface = {
    destroyCallback,
    setPositionCallback,
    placeAboveCallback,
    placeBelowCallback,
    setSyncCallback,
    setDeSyncCallback
};

SubSurfaceInterface::Private *SubSurfaceInterface::Private::cast(wl_resource *r)
{
    return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
}

SubSurfaceInterface::Private::Private(SubSurfaceInterface *q)
    : q(q)
{
}

SubSurfaceInterface::Private::~Private()
{
    // no need to notify the surface as it's tracking a QPointer which will be reset automatically
    if (parent) {
        parent->d->removeChild(QPointer<SubSurfaceInterface>(q));
    }
    if (subSurface) {
        wl_resource_destroy(subSurface);
    }
}

void SubSurfaceInterface::Private::create(wl_client *client, quint32 version, quint32 id, SurfaceInterface *s, SurfaceInterface *p)
{
    Q_ASSERT(!subSurface);
    subSurface = wl_resource_create(client, &wl_subsurface_interface, version, id);
    if (!subSurface) {
        return;
    }
    surface = s;
    parent = p;
    surface->d->subSurface = QPointer<SubSurfaceInterface>(q);
    parent->d->addChild(QPointer<SubSurfaceInterface>(q));
    wl_resource_set_implementation(subSurface, &s_interface, this, unbind);
}

void SubSurfaceInterface::Private::commit()
{
    if (scheduledPosChange) {
        scheduledPosChange = false;
        pos = scheduledPos;
        scheduledPos = QPoint();
        emit q->positionChanged(pos);
    }
}

void SubSurfaceInterface::Private::unbind(wl_resource *r)
{
    auto s = cast(r);
    s->subSurface = nullptr;
    s->q->deleteLater();
}

void SubSurfaceInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->q->deleteLater();
}

void SubSurfaceInterface::Private::setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(client)
    // TODO: is this a fixed position?
    cast(resource)->setPosition(QPoint(x, y));
}

void SubSurfaceInterface::Private::setPosition(const QPoint &p)
{
    if (scheduledPos == p) {
        return;
    }
    scheduledPos = p;
    scheduledPosChange = true;
}

void SubSurfaceInterface::Private::placeAboveCallback(wl_client *client, wl_resource *resource, wl_resource *sibling)
{
    Q_UNUSED(client)
    cast(resource)->placeAbove(SurfaceInterface::get(sibling));
}

void SubSurfaceInterface::Private::placeAbove(SurfaceInterface *sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    if (!parent->d->raiseChild(QPointer<SubSurfaceInterface>(q), sibling)) {
        wl_resource_post_error(subSurface, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void SubSurfaceInterface::Private::placeBelowCallback(wl_client *client, wl_resource *resource, wl_resource *sibling)
{
    Q_UNUSED(client)
    cast(resource)->placeBelow(SurfaceInterface::get(sibling));
}

void SubSurfaceInterface::Private::placeBelow(SurfaceInterface *sibling)
{
    if (parent.isNull()) {
        // TODO: raise error
        return;
    }
    if (!parent->d->lowerChild(QPointer<SubSurfaceInterface>(q), sibling)) {
        wl_resource_post_error(subSurface, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Incorrect sibling");
    }
}

void SubSurfaceInterface::Private::setSyncCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->setMode(Mode::Synchronized);
}

void SubSurfaceInterface::Private::setDeSyncCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->setMode(Mode::Desynchronized);
}

void SubSurfaceInterface::Private::setMode(Mode m)
{
    if (mode == m) {
        return;
    }
    mode = m;
    emit q->modeChanged(m);
}

SubSurfaceInterface::SubSurfaceInterface(SubCompositorInterface *parent)
    : QObject(/*parent*/)
    , d(new Private(this))
{
    Q_UNUSED(parent)
}

SubSurfaceInterface::~SubSurfaceInterface() = default;

QPoint SubSurfaceInterface::position() const
{
    return d->pos;
}

wl_resource *SubSurfaceInterface::subSurface()
{
    return d->subSurface;
}

QPointer<SurfaceInterface> SubSurfaceInterface::surface()
{
    return d->surface;
}

QPointer<SurfaceInterface> SubSurfaceInterface::parentSurface()
{
    return d->parent;
}

SubSurfaceInterface::Mode SubSurfaceInterface::mode() const
{
    return d->mode;
}

}
}