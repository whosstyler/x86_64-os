#ifndef DESKTOP_H
#define DESKTOP_H

#include "../include/kernel.h"
#include "window.h"

VOID
INIT_DESKTOP(VOID);

PWINDOW
DESKTOP_OPEN_TERMINAL(VOID);

PWINDOW
DESKTOP_OPEN_SYSINFO(VOID);

#endif
