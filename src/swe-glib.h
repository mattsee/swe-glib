/* SWE-GLib - GLib style wrapper library around Astrodienst's Swiss Ephemeris
 *
 * Copyright © 2013  Gergely Polonkai
 *
 * SWE-GLib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * SWE-GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __SWE_GLIB_H__
#define __SWE_GLIB_H__

#include <glib.h>
#include "gswe-types.h"
#include "gswe-moon-phase-data.h"
#include "gswe-sign-info.h"
#include "gswe-planet-info.h"
#include "gswe-planet-data.h"
#include "gswe-aspect-info.h"
#include "gswe-aspect-data.h"
#include "gswe-antiscion-axis-info.h"
#include "gswe-antiscion-data.h"
#include "gswe-house-system-info.h"
#include "gswe-house-data.h"
#include "gswe-timestamp.h"
#include "gswe-moment.h"
#include "gswe-enumtypes.h"

void gswe_init();

#endif /* __SWE_GLIB_H__ */

