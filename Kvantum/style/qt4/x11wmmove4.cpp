/*
    Adapted from Bespin's WM.cpp v0.r1710
    and QtCurve's x11helpers.c v1.8.17.
*/

#include "x11wmmove4.h"
#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <QX11Info>
#endif

namespace Kvantum {
void X11MoveTrigger (WId wid, int x, int y)
{
#if defined Q_WS_X11 || defined Q_OS_LINUX
  QX11Info info;
  Atom netMoveResize = XInternAtom (QX11Info::display(), "_NET_WM_MOVERESIZE", False);
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = netMoveResize;
  xev.xclient.display = QX11Info::display();
  xev.xclient.window = wid;
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = x;
  xev.xclient.data.l[1] = y;
  xev.xclient.data.l[2] = 8;
  xev.xclient.data.l[3] = Button1;
  xev.xclient.data.l[4] = 0;
  XUngrabPointer (QX11Info::display(), QX11Info::appTime());
  XSendEvent (QX11Info::display(),
              QX11Info::appRootWindow (info.screen()),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
#else
  Q_UNUSED (wid); Q_UNUSED (x); Q_UNUSED (y);
#endif
  return;
}
}
