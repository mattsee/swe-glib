/* gswe-timestamp.c - Converter GObject between Gregorian calendar date and
 * Julian day
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
#include <math.h>
#include <glib.h>

#include "../swe/src/swephexp.h"
#include "swe-glib-private.h"
#include "swe-glib.h"
#include "gswe-timestamp.h"

/**
 * SECTION:gswe-timestamp
 * @short_description: conversion between Gregorian dates and Julian day values
 * @title: GsweTimestamp
 * @stability: Stable
 * @include: swe-glib/swe-glib.h
 *
 * This object converts Gregorian dates to Julian days and vice versa.
 */

#define GSWE_TIMESTAMP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE( \
            (obj), \
            GSWE_TYPE_TIMESTAMP, \
            GsweTimestampPrivate \
        ))

struct _GsweTimestampPrivate {
    gboolean instant_recalc;
    GsweTimestampValidityFlags valid_dates;

    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint gregorian_hour;
    gint gregorian_minute;
    gint gregorian_second;
    gint gregorian_microsecond;
    gdouble gregorian_timezone_offset;

    gdouble julian_day;
    gdouble julian_day_ut;
};

enum {
    SIGNAL_CHANGED,
    SIGNAL_LAST
};

enum {
    PROP_0,
    PROP_INSTANT_RECALC,
    PROP_GREGORIAN_VALID,
    PROP_GREGORIAN_YEAR,
    PROP_GREGORIAN_MONTH,
    PROP_GREGORIAN_DAY,
    PROP_GREGORIAN_HOUR,
    PROP_GREGORIAN_MINUTE,
    PROP_GREGORIAN_SECOND,
    PROP_GREGORIAN_MICROSECOND,
    PROP_GREGORIAN_TIMEZONE_OFFSET,
    PROP_JULIAN_DAY,
    PROP_JULIAN_DAY_VALID,
    PROP_COUNT
};

static guint gswe_timestamp_signals[SIGNAL_LAST] = { 0 };
static GParamSpec *gswe_timestamp_props[PROP_COUNT];

static void gswe_timestamp_dispose(
        GObject *gobject);

static void gswe_timestamp_finalize(
        GObject *gobject);

static void gswe_timestamp_set_property(
        GObject *gobject,
        guint prop_id,
        const GValue *value,
        GParamSpec *pspec);

static void gswe_timestamp_get_property(
        GObject *gobject,
        guint prop_id,
        GValue *value,
        GParamSpec *pspec);

static void gswe_timestamp_calculate_all(
        GsweTimestamp *timestamp,
        GError **err);

static void gswe_timestamp_calculate_gregorian(
        GsweTimestamp *timestamp,
        GError **err);

static void gswe_timestamp_calculate_julian(
        GsweTimestamp *timestamp,
        GError **err);

G_DEFINE_TYPE(GsweTimestamp, gswe_timestamp, G_TYPE_OBJECT);

static void
gswe_timestamp_class_init(GsweTimestampClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GDateTime *local_time = g_date_time_new_now_utc();

    g_type_class_add_private(klass, sizeof(GsweTimestampPrivate));

    gobject_class->dispose = gswe_timestamp_dispose;
    gobject_class->finalize = gswe_timestamp_finalize;
    gobject_class->set_property = gswe_timestamp_set_property;
    gobject_class->get_property = gswe_timestamp_get_property;

    /**
     * GsweTimestamp::changed:
     * @timestamp: the GsweTimestamp that receives the signal
     *
     * The ::changed signal is emitted each time the timestamp is changed
     */
    gswe_timestamp_signals[SIGNAL_CHANGED] = g_signal_new(
            "changed",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_FIRST,
            0, NULL, NULL,
            g_cclosure_marshal_generic, G_TYPE_NONE, 0);

    /**
     * GsweTimestamp:instant-recalc:
     *
     * If set to TRUE, recalculate timestamp values instantly, when changing a
     * parameter (e.g. recalculate Julian date when changing Gregorian year).
     * Otherwise, the values are recalculated only upon request (e.g. on
     * calling gswe_timestamp_get_julian_day()).
     */
    gswe_timestamp_props[PROP_INSTANT_RECALC] = g_param_spec_boolean(
            "instant-recalc",
            "Instant recalculation",
            "Instantly recalculate values upon parameter change",
            FALSE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_INSTANT_RECALC,
            gswe_timestamp_props[PROP_INSTANT_RECALC]
        );

    /**
     * GsweTimestamp:gregorian-valid:
     *
     * If TRUE, the Gregorian value stored in the GsweTimestamp object is
     * currently considered as valid, thus, no recalculation is needed.
     * Otherwise, the Gregorian date components will be recalculated upon
     * request.
     */
    gswe_timestamp_props[PROP_GREGORIAN_VALID] = g_param_spec_boolean(
            "gregorian-valid",
            "Gregorian date is valid",
            "TRUE if the Gregorian date components are considered as valid.",
            TRUE, G_PARAM_READABLE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_VALID,
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );

    /**
     * GsweTimestamp:gregorian-year:
     *
     * The Gregorian year of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_YEAR] = g_param_spec_int(
            "gregorian-year",
            "Gregorian year",
            "The year according to the Gregorian calendar",
            G_MININT, G_MAXINT, g_date_time_get_year(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_YEAR,
            gswe_timestamp_props[PROP_GREGORIAN_YEAR]
        );

    /**
     * GsweTimestamp:gregorian-month:
     *
     * The Gregorian month of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_MONTH] = g_param_spec_int(
            "gregorian-month",
            "Gregorian month",
            "The month according to the Gregorian calendar",
            1, 12, g_date_time_get_month(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_MONTH,
            gswe_timestamp_props[PROP_GREGORIAN_MONTH]
        );

    /**
     * GsweTimestamp:gregorian-day:
     *
     * The Gregorian day of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_DAY] = g_param_spec_int(
            "gregorian-day",
            "Gregorian day",
            "The day according to the Gregorian calendar",
            1, 31, g_date_time_get_day_of_month(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_DAY,
            gswe_timestamp_props[PROP_GREGORIAN_DAY]
        );

    /**
     * GsweTimestamp:gregorian-hour:
     *
     * The Gregorian hour of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_HOUR] = g_param_spec_int(
            "gregorian-hour",
            "Gregorian hour",
            "The hour according to the Gregorian calendar",
            0, 23, g_date_time_get_hour(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_HOUR,
            gswe_timestamp_props[PROP_GREGORIAN_HOUR]
        );

    /**
     * GsweTimestamp:gregorian-minute:
     *
     * The Gregorian minute of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_MINUTE] = g_param_spec_int(
            "gregorian-minute",
            "Gregorian minute",
            "The minute according to the Gregorian calendar",
            0, 59, g_date_time_get_minute(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_MINUTE,
            gswe_timestamp_props[PROP_GREGORIAN_MINUTE]
        );

    /**
     * GsweTimestamp:gregorian-second:
     *
     * The Gregorian second of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_SECOND] = g_param_spec_int(
            "gregorian-second",
            "Gregorian second",
            "The second according to the Gregorian calendar",
            0, 61, g_date_time_get_second(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_SECOND,
            gswe_timestamp_props[PROP_GREGORIAN_SECOND]
        );

    /**
     * GsweTimestamp:gregorian-microsecond:
     *
     * The Gregorian microsecond of the timestamp
     */
    gswe_timestamp_props[PROP_GREGORIAN_MICROSECOND] = g_param_spec_int(
            "gregorian-microsecond",
            "Gregorian microsecond",
            "The microsecond according to the Gregorian calendar",
            0, G_MAXINT, g_date_time_get_microsecond(local_time),
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_MICROSECOND,
            gswe_timestamp_props[PROP_GREGORIAN_MICROSECOND]
        );

    /**
     * GsweTimestamp:gregorian-timezone-offset:
     *
     * The time zone offset in hours, relative to UTC
     */
    gswe_timestamp_props[PROP_GREGORIAN_TIMEZONE_OFFSET] = g_param_spec_double(
            "gregorian-timezone-offset",
            "Gregorian timezone offset",
            "The offset relative to UTC in the Gregorian calendar",
            -24.0, 24.0,
            0.0,
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_GREGORIAN_TIMEZONE_OFFSET,
            gswe_timestamp_props[PROP_GREGORIAN_TIMEZONE_OFFSET]
        );

    /**
     * GsweTimestamp:julian-day:
     *
     * The Julian Day represented by #GsweTimestamp
     */
    gswe_timestamp_props[PROP_JULIAN_DAY] = g_param_spec_double(
            "julian-day",
            "Julian Day",
            "The Julian Day represented by this object",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_JULIAN_DAY,
            gswe_timestamp_props[PROP_JULIAN_DAY]
        );

    /**
     * GsweTimestamp:julian-day-valid:
     *
     * If TRUE, the Julian day value stored in the GsweTimestamp object is
     * currently considered as valid, thus, no recalculation is needed.
     * Otherwise, the Julian day components will be recalculated upon request.
     */
    gswe_timestamp_props[PROP_JULIAN_DAY_VALID] = g_param_spec_boolean(
            "julian-day-valid",
            "Julian day is valid",
            "TRUE if the Julian day components are considered as valid.",
            TRUE, G_PARAM_READABLE
        );
    g_object_class_install_property(
            gobject_class,
            PROP_JULIAN_DAY_VALID,
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );

    g_date_time_unref(local_time);
}

static void
gswe_timestamp_emit_changed(GsweTimestamp *timestamp)
{
    g_signal_emit(timestamp, gswe_timestamp_signals[SIGNAL_CHANGED], 0);
}

void
gswe_timestamp_init(GsweTimestamp *self)
{
    self->priv = GSWE_TIMESTAMP_GET_PRIVATE(self);
}

static void
gswe_timestamp_dispose(GObject *gobject)
{
    G_OBJECT_CLASS(gswe_timestamp_parent_class)->dispose(gobject);
}

static void
gswe_timestamp_finalize(GObject *gobject)
{
    G_OBJECT_CLASS(gswe_timestamp_parent_class)->finalize(gobject);
}

static void
gswe_timestamp_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GsweTimestamp *timestamp = GSWE_TIMESTAMP(object);

    switch (prop_id) {
        case PROP_INSTANT_RECALC:
            timestamp->priv->instant_recalc = g_value_get_boolean(value);

            if (timestamp->priv->instant_recalc) {
                gswe_timestamp_calculate_all(timestamp, NULL);
            }

            break;

        case PROP_GREGORIAN_YEAR:
            gswe_timestamp_set_gregorian_year(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_MONTH:
            gswe_timestamp_set_gregorian_month(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_DAY:
            gswe_timestamp_set_gregorian_day(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_HOUR:
            gswe_timestamp_set_gregorian_hour(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_MINUTE:
            gswe_timestamp_set_gregorian_minute(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_SECOND:
            gswe_timestamp_set_gregorian_second(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_MICROSECOND:
            gswe_timestamp_set_gregorian_microsecond(
                    timestamp,
                    g_value_get_int(value),
                    NULL
                );

            break;

        case PROP_GREGORIAN_TIMEZONE_OFFSET:
            gswe_timestamp_set_gregorian_timezone(
                    timestamp,
                    g_value_get_double(value),
                    NULL
                );

            break;

        case PROP_JULIAN_DAY:
            gswe_timestamp_set_julian_day_et(
                    timestamp,
                    g_value_get_double(value),
                    NULL
                );

            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);

            break;
    }
}

static void
gswe_timestamp_get_property(
        GObject *object,
        guint prop_id,
        GValue *value,
        GParamSpec *pspec)
{
    GsweTimestamp *timestamp = GSWE_TIMESTAMP(object);

    switch (prop_id) {
        case PROP_INSTANT_RECALC:
            g_value_set_boolean(value, timestamp->priv->instant_recalc);

            break;

        case PROP_GREGORIAN_VALID:
            g_value_set_boolean(
                    value,
                    ((timestamp->priv->valid_dates & GSWE_VALID_GREGORIAN)
                        == GSWE_VALID_GREGORIAN)
                );

            break;

        case PROP_GREGORIAN_YEAR:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_year);

            break;

        case PROP_GREGORIAN_MONTH:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_month);

            break;

        case PROP_GREGORIAN_DAY:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_day);

            break;

        case PROP_GREGORIAN_HOUR:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_hour);

            break;

        case PROP_GREGORIAN_MINUTE:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_minute);

            break;

        case PROP_GREGORIAN_SECOND:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_second);

            break;

        case PROP_GREGORIAN_MICROSECOND:
            gswe_timestamp_calculate_gregorian(timestamp, NULL);
            g_value_set_int(value, timestamp->priv->gregorian_microsecond);

            break;

        case PROP_GREGORIAN_TIMEZONE_OFFSET:
            g_value_set_double(
                    value,
                    timestamp->priv->gregorian_timezone_offset);

            break;

        case PROP_JULIAN_DAY:
            g_value_set_double(
                    value,
                    timestamp->priv->julian_day
                );

            break;

        case PROP_JULIAN_DAY_VALID:
            g_value_set_boolean(
                    value,
                    ((timestamp->priv->valid_dates & GSWE_VALID_JULIAN_DAY)
                        == GSWE_VALID_JULIAN_DAY)
                );

            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);

            break;
    }
}

static void
gswe_timestamp_calculate_all(GsweTimestamp *timestamp, GError **err)
{
    if ((timestamp->priv->valid_dates & GSWE_VALID_JULIAN_DAY) != GSWE_VALID_JULIAN_DAY) {
        gswe_timestamp_calculate_julian(timestamp, err);
    }

    if ((timestamp->priv->valid_dates & GSWE_VALID_GREGORIAN) != GSWE_VALID_GREGORIAN) {
        gswe_timestamp_calculate_gregorian(timestamp, err);
    }
}

static void
gswe_timestamp_calculate_gregorian(GsweTimestamp *timestamp, GError **err)
{
    gint utc_year,
         utc_month,
         utc_day,
         utc_hour,
         utc_minute;
    gdouble utc_second,
            local_second;

    if ((timestamp->priv->valid_dates & GSWE_VALID_GREGORIAN) == GSWE_VALID_GREGORIAN) {
        return;
    }

    if (timestamp->priv->valid_dates == 0) {
        g_set_error(
                err,
                GSWE_ERROR,
                GSWE_ERROR_NO_VALID_VALUE,
                "This timestamp object holds no valid values"
            );

        return;
    }

    swe_jdet_to_utc(
            timestamp->priv->julian_day,
            SE_GREG_CAL,
            &utc_year, &utc_month, &utc_day,
            &utc_hour, &utc_minute, &utc_second
        );
    swe_utc_time_zone(
            utc_year, utc_month, utc_day,
            utc_hour, utc_minute, utc_second,
            0 - timestamp->priv->gregorian_timezone_offset,
            &timestamp->priv->gregorian_year,
            &timestamp->priv->gregorian_month,
            &timestamp->priv->gregorian_day,
            &timestamp->priv->gregorian_hour,
            &timestamp->priv->gregorian_minute,
            &local_second
        );
    timestamp->priv->gregorian_second = floor(local_second);
    timestamp->priv->gregorian_microsecond = (
            local_second - floor(local_second))
               * 1000;

    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_YEAR]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MONTH]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_DAY]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_HOUR]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MINUTE]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_SECOND]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MICROSECOND]
        );
}

/**
 * gswe_timestamp_set_instant_recalc:
 * @timestamp: a GsweTimestamp
 * @instant_recalc: the new value
 * @err: a #GError
 *
 * Sets the value of the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property. For
 * details, see the property's description. @err is populated with calculation
 * errors if @instant_recalc is TRUE and a calculation error happens.
 */
void
gswe_timestamp_set_instant_recalc(
        GsweTimestamp *timestamp,
        gboolean instant_recalc,
        GError **err)
{
    timestamp->priv->instant_recalc = instant_recalc;

    if (instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_INSTANT_RECALC]
        );
}

/**
 * gswe_timestamp_get_instant_recalc:
 * @timestamp: a GsweTimestamp
 *
 * Gets the value of the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property. For
 * details, see the property's description.
 */
gboolean
gswe_timestamp_get_instant_recalc(GsweTimestamp *timestamp)
{
    return timestamp->priv->instant_recalc;
}

/**
 * gswe_timestamp_set_gregorian_full:
 * @timestamp: a GsweTimestamp
 * @year: the new Gregorian year
 * @month: the new Gregorian month
 * @day: the new Gregorian day
 * @hour: the new hour value
 * @minute: the new minute value
 * @second: the new second value
 * @microsecond: the new microsecond value
 * @time_zone_offset: the time zone offset, in hours
 * @err: a #GError
 *
 * Sets the Gregorian date of @timestamp. @err is populated with calculation
 * errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_full(
        GsweTimestamp *timestamp,
        gint year, gint month, gint day,
        gint hour, gint minute, gint second, gint microsecond,
        gdouble time_zone_offset,
        GError **err)
{
    gboolean changed = FALSE;

    if (timestamp->priv->gregorian_year != year) {
        timestamp->priv->gregorian_year = year;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_YEAR]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_month != month) {
        timestamp->priv->gregorian_month = month;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_MONTH]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_day != day) {
        timestamp->priv->gregorian_day = day;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_DAY]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_hour != hour) {
        timestamp->priv->gregorian_hour = hour;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_HOUR]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_minute != minute) {
        timestamp->priv->gregorian_minute = minute;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_MINUTE]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_second != second) {
        timestamp->priv->gregorian_second = second;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_SECOND]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_microsecond != microsecond) {
        timestamp->priv->gregorian_microsecond = microsecond;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_MICROSECOND]
            );

        changed = TRUE;
    }

    if (timestamp->priv->gregorian_timezone_offset != time_zone_offset) {
        timestamp->priv->gregorian_timezone_offset = time_zone_offset;

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_TIMEZONE_OFFSET]
            );

        changed = TRUE;
    }

    if (changed) {
        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_GREGORIAN_VALID]
            );

        g_object_notify_by_pspec(
                G_OBJECT(timestamp),
                gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
            );

        timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;

        if (timestamp->priv->instant_recalc == TRUE) {
            gswe_timestamp_calculate_all(timestamp, err);
        }

        gswe_timestamp_emit_changed(timestamp);
    }
}

/**
 * gswe_timestamp_set_gregorian_year:
 * @timestamp: a GsweTimestamp
 * @gregorian_year: the new Gregorian year
 * @err: a #GError
 *
 * Sets the Gregorian year of @timestamp. @err is populated with calculation
 * errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_year(
        GsweTimestamp *timestamp,
        gint gregorian_year,
        GError **err)
{
    if (timestamp->priv->gregorian_year == gregorian_year) {
        return;
    }

    timestamp->priv->gregorian_year = gregorian_year;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;

    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_YEAR]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_year:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the Gregorian year of @timestamp.
 *
 * Returns: the year part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_year(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_year;
}

/**
 * gswe_timestamp_set_gregorian_month:
 * @timestamp: a GsweTimestamp
 * @gregorian_month: the new Gregorian month
 * @err: a #GError
 *
 * Sets the Gregorian month of @timestamp. @err is populated with calculation
 * errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_month(
        GsweTimestamp *timestamp,
        gint gregorian_month,
        GError **err)
{
    if (timestamp->priv->gregorian_month == gregorian_month) {
        return;
    }

    timestamp->priv->gregorian_month = gregorian_month;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MONTH]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_month:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the Gregorian month of @timestamp.
 *
 * Returns: the month part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_month(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_month;
}

/**
 * gswe_timestamp_set_gregorian_day:
 * @timestamp: a GsweTimestamp
 * @gregorian_day: the new Gregorian day
 * @err: a #GError
 *
 * Sets the Gregorian day of @timestamp. @err is populated with calculation
 * errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_day(
        GsweTimestamp *timestamp,
        gint gregorian_day,
        GError **err)
{
    if (timestamp->priv->gregorian_day == gregorian_day) {
        return;
    }

    timestamp->priv->gregorian_day = gregorian_day;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_DAY]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_day:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the Gregorian day of @timestamp.
 *
 * Returns: the day part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_day(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_day;
}

/**
 * gswe_timestamp_set_gregorian_hour:
 * @timestamp: a GsweTimestamp
 * @gregorian_hour: the new hour value
 * @err: a #GError
 *
 * Sets the hour value of @timestamp, which may be modified by the time zone.
 * @err is populated with calculation errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_hour(
        GsweTimestamp *timestamp,
        gint gregorian_hour,
        GError **err)
{
    if (timestamp->priv->gregorian_hour == gregorian_hour) {
        return;
    }

    timestamp->priv->gregorian_hour = gregorian_hour;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_HOUR]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_hour:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the hour of @timestamp.
 *
 * Returns: the hour part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_hour(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_hour;
}

/**
 * gswe_timestamp_set_gregorian_minute:
 * @timestamp: a GsweTimestamp
 * @gregorian_minute: the new minute value
 * @err: a #GError
 *
 * Sets the minute value of @timestamp, which may be modified by the timezone.
 * @err is populated with calculation errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_minute(
        GsweTimestamp *timestamp,
        gint gregorian_minute,
        GError **err)
{
    if (timestamp->priv->gregorian_minute == gregorian_minute) {
        return;
    }

    timestamp->priv->gregorian_minute = gregorian_minute;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MINUTE]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_minute:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the minute of @timestamp.
 *
 * Returns: the minute part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_minute(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_minute;
}

/**
 * gswe_timestamp_set_gregorian_second:
 * @timestamp: a GsweTimestamp
 * @gregorian_second: the new second value
 * @err: a #GError
 *
 * Sets the second value of @timestamp, which may be modified by the timezone.
 * @err is populated with calculation errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE.
 */
void
gswe_timestamp_set_gregorian_second(
        GsweTimestamp *timestamp,
        gint gregorian_second,
        GError **err)
{
    if (timestamp->priv->gregorian_second == gregorian_second) {
        return;
    }

    timestamp->priv->gregorian_second = gregorian_second;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_SECOND]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_second:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the second of @timestamp.
 *
 * Returns: the second part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_second(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_second;
}

/**
 * gswe_timestamp_set_gregorian_microsecond:
 * @timestamp: a GsweTimestamp
 * @gregorian_microsecond: the new microsecond value
 * @err: a #GError
 *
 * Sets the microsecond value of @timestamp. @err is populated with calculation
 * errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property is
 * TRUE
 */
void
gswe_timestamp_set_gregorian_microsecond(
        GsweTimestamp *timestamp,
        gint gregorian_microsecond,
        GError **err)
{
    if (timestamp->priv->gregorian_microsecond == gregorian_microsecond) {
        return;
    }

    timestamp->priv->gregorian_microsecond = gregorian_microsecond;
    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_MICROSECOND]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_gregorian_microsecond:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Returns the microsecond of @timestamp.
 *
 * Returns: the microsecond part of @timestamp's Gregorian Date value.
 */
gint
gswe_timestamp_get_gregorian_microsecond(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_gregorian(timestamp, err);

    return timestamp->priv->gregorian_microsecond;
}

/**
 * gswe_timestamp_set_gregorian_timezone:
 * @timestamp: a GsweTimestamp
 * @gregorian_timezone_offset: the offset of the desired time zone, in hours
 * @err: a #GError
 *
 * Sets the time zone used in Gregorian date calculations. @err is populated
 * with calculation errors if the <link
 * linkend="GsweTimestamp--instant-recalc">instant-recalc</link> property's
 * value is TRUE and a calculation error happens.
 */
void
gswe_timestamp_set_gregorian_timezone(
        GsweTimestamp *timestamp,
        gdouble gregorian_timezone_offset,
        GError **err)
{
    if (timestamp->priv->gregorian_timezone_offset == gregorian_timezone_offset) {
        return;
    }

    gswe_timestamp_calculate_julian(timestamp, NULL);
    timestamp->priv->gregorian_timezone_offset = gregorian_timezone_offset;
    timestamp->priv->valid_dates &= ~GSWE_VALID_GREGORIAN;

    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_TIMEZONE_OFFSET]
        );

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }
}

/**
 * gswe_timestamp_get_gregorian_timezone:
 * @timestamp: a GsweTimestamp
 *
 * Gets the time zone used in Gregorian date calculations.
 */
gdouble
gswe_timestamp_get_gregorian_timezone(GsweTimestamp *timestamp)
{
    return timestamp->priv->gregorian_timezone_offset;
}

static void
gswe_timestamp_calculate_julian(GsweTimestamp *timestamp, GError **err)
{
    gint utc_year,
         utc_month,
         utc_day,
         utc_hour,
         utc_minute,
         retval;
    gdouble utc_second,
            dret[2];
    gchar serr[AS_MAXCH];

    if ((timestamp->priv->valid_dates & GSWE_VALID_JULIAN_DAY) == GSWE_VALID_JULIAN_DAY) {
        return;
    }

    if (timestamp->priv->valid_dates == 0) {
        g_set_error(
                err,
                GSWE_ERROR, GSWE_ERROR_NO_VALID_VALUE,
                "This timestamp object holds no valid values"
            );

        return;
    }

    swe_utc_time_zone(
            timestamp->priv->gregorian_year,
            timestamp->priv->gregorian_month,
            timestamp->priv->gregorian_day,
            timestamp->priv->gregorian_hour,
            timestamp->priv->gregorian_minute,
            timestamp->priv->gregorian_second
                + timestamp->priv->gregorian_microsecond / 1000.0,
            timestamp->priv->gregorian_timezone_offset,
            &utc_year, &utc_month, &utc_day,
            &utc_hour, &utc_minute, &utc_second
        );

    if (
            (retval = swe_utc_to_jd(
                utc_year, utc_month, utc_day,
                utc_hour, utc_minute, utc_second,
                SE_GREG_CAL, dret, serr)
            ) == ERR) {
        g_set_error(
                err,
                GSWE_ERROR, GSWE_ERROR_SWE_FATAL,
                "Swiss Ephemeris error: %s", serr
            );
    } else {
        timestamp->priv->julian_day = dret[0];
        timestamp->priv->julian_day_ut = dret[1];
        timestamp->priv->valid_dates |= GSWE_VALID_JULIAN_DAY;
    }
}

/**
 * gswe_timestamp_set_julian_day:
 * @timestamp: a #GsweTimestamp
 * @julian_day: The Julian day number, with hours included as fractions
 *
 * Deprecated: This function is deprecated in version 2.1. In newly written
 *             code use gswe_timestamp_set_julian_day_et()
 */
void
gswe_timestamp_set_julian_day(GsweTimestamp *timestamp, gdouble julian_day)
{
    gswe_timestamp_set_julian_day_et(timestamp, julian_day, NULL);
}

/**
 * gswe_timestamp_get_julian_day:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Deprecated: This function is deprecated in version 2.1. In newly written
 *             code use gswe_timestamp_get_julian_day_et()
 */
gdouble
gswe_timestamp_get_julian_day(GsweTimestamp *timestamp, GError **err)
{
    return gswe_timestamp_get_julian_day_et(timestamp, err);
}

/**
 * gswe_timestamp_set_julian_day_et:
 * @timestamp: A GsweTimestamp
 * @julian_day: The Julian day number, with hours included as fractions
 * @err: a #GError
 *
 * Sets the Julian day value of the timestamp. The Gregorian date will be
 * calculated as requested. Given Julian day must be given in Ephemeris Time
 * (ET). See gswe_timestamp_set_julian_day_ut() for Universal Time (UT)
 * version.
 *
 * Since: 2.1
 */
void
gswe_timestamp_set_julian_day_et(
        GsweTimestamp *timestamp,
        gdouble julian_day,
        GError **err)
{
    GError *err_local = NULL;

    timestamp->priv->julian_day = julian_day;
    timestamp->priv->valid_dates = GSWE_VALID_JULIAN_DAY;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );

    // to get exact UT value, we must calculate gregorian date, and
    // reverse-engineer the UT value from that. This is an overkill, but libswe
    // doesn’t provide an API for that.
    gswe_timestamp_calculate_gregorian(timestamp, &err_local);

    if (err_local) {
        if (err) {
            *err = err_local;
        }

        return;
    }

    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    gswe_timestamp_calculate_julian(timestamp, &err_local);

    if (err_local) {
        if (err) {
            *err = err_local;
        }

        return;
    }

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_julian_day_et:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Gets the Julian day value of @timestamp, Ephemeris Time (ET). For Universal
 * Time (UT), see gswe_timestamp_get_julian_day_ut(). @err is populated if the
 * calculations raises an error.
 *
 * Since: 2.1
 */
gdouble
gswe_timestamp_get_julian_day_et(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_julian(timestamp, err);

    return timestamp->priv->julian_day;
}

/**
 * gswe_timestamp_set_julian_day_ut:
 * @timestamp: a GsweTimestamp
 * @julian_day: The Julian day number in Universal Time
 * @err: a #GError
 *
 * Sets the Julian day value of the timestamp. The Julian day is considered to
 * be in Universal Time (UT). For Ephemeris time, see
 * gswe_timestamp_set_julian_day(). Should an error occur, @err is populated
 * with the error details.
 *
 * Since: 2.1
 */
void
gswe_timestamp_set_julian_day_ut(
        GsweTimestamp *timestamp,
        gdouble julian_day,
        GError **err)
{
    GError *err_local = NULL;

    timestamp->priv->julian_day_ut = julian_day;
    timestamp->priv->valid_dates = GSWE_VALID_JULIAN_DAY;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );

    // to get exact ET value, we must calculate gregorian date, and
    // reverse-engineer the ET value from that. This is an overkill, but libswe
    // doesn’t provide an API for that.
    gswe_timestamp_calculate_gregorian(timestamp, err);

    if (err_local) {
        if (err) {
            *err = err_local;
        }

        return;
    }

    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_GREGORIAN_VALID]
        );
    g_object_notify_by_pspec(
            G_OBJECT(timestamp),
            gswe_timestamp_props[PROP_JULIAN_DAY_VALID]
        );
    gswe_timestamp_calculate_julian(timestamp, err);

    if (err_local) {
        if (err) {
            *err = err_local;
        }

        return;
    }

    if (timestamp->priv->instant_recalc == TRUE) {
        gswe_timestamp_calculate_all(timestamp, err);
    }

    gswe_timestamp_emit_changed(timestamp);
}

/**
 * gswe_timestamp_get_julian_day_ut:
 * @timestamp: a #GsweTimestamp
 * @err: a #GError
 *
 * Gets the Julian day value of @timestamp, Universal Time (UT). For Ephemeris
 * Time (ET) see gswe_timestamp_get_julian_day_et(). @err is populated if the
 * calculations raise an error.
 *
 * Since: 2.1
 */
gdouble
gswe_timestamp_get_julian_day_ut(GsweTimestamp *timestamp, GError **err)
{
    gswe_timestamp_calculate_julian(timestamp, err);

    return timestamp->priv->julian_day_ut;
}

/**
 * gswe_timestamp_get_sidereal_time:
 * @timestamp: a GsweTimestamp
 * @err: a #GError
 *
 * Gets the sidereal time on the Greenwich Meridian.
 *
 * Returns: the sidereal time in hours. To get the degrees, multiply this value
 * by 15.
 *
 * Since: 2.1
 */
gdouble
gswe_timestamp_get_sidereal_time(GsweTimestamp *timestamp, GError **err)
{
    GError *local_err = NULL;

    gswe_timestamp_calculate_julian(timestamp, &local_err);

    if (local_err) {
        if (err) {
            *err = local_err;
        } else {
            g_error_free(local_err);
        }

        return 0.0;
    }

    return swe_sidtime(timestamp->priv->julian_day_ut);
}

/**
 * gswe_timestamp_new:
 *
 * Creates a new GsweTimestamp object. The object is initialized with current
 * date and time in the local timezone.
 *
 * Returns: a new GsweTimestamp object
 */
GsweTimestamp *
gswe_timestamp_new(void)
{
    gswe_init();

    return GSWE_TIMESTAMP(g_object_new(GSWE_TYPE_TIMESTAMP, NULL));
}

/**
 * gswe_timestamp_new_from_gregorian_full:
 * @year: the year
 * @month: the month
 * @day: the day
 * @hour: the hour
 * @minute: the minute
 * @second: the second
 * @microsecond: the microsecond
 * @time_zone_offset: the time zone offset in hours
 *
 * Creates a new GsweTimestamp object, initialized with the Gregorian date
 * specified by the function parameters.
 *
 * Returns: a new GsweTimestamp object.
 */
GsweTimestamp *
gswe_timestamp_new_from_gregorian_full(
        gint year, gint month, gint day,
        gint hour, gint minute, gint second, gint microsecond,
        gdouble time_zone_offset)
{
    GsweTimestamp *timestamp;

    gswe_init();

    timestamp = GSWE_TIMESTAMP(g_object_new(GSWE_TYPE_TIMESTAMP,
                "gregorian-year",            year,
                "gregorian-month",           month,
                "gregorian-day",             day,
                "gregorian-hour",            hour,
                "gregorian-minute",          minute,
                "gregorian-second",          second,
                "gregorian-microsecond",     microsecond,
                "gregorian-timezone-offset", time_zone_offset,
                NULL));

    timestamp->priv->valid_dates = GSWE_VALID_GREGORIAN;

    return timestamp;
}

/**
 * gswe_timestamp_new_from_julian_day:
 * @julian_day: a Julian day value, with time included as fractions.
 *
 * Creates a new GsweTimestamp object with @julian_day as its initial date
 * value. The object can be used for astronomical calculations, e.g. with
 * #GsweMoment, but unless a time zone is provided with
 * gswe_timestamp_set_gregorian_timezone(), exact Gregorian dates can not be
 * calculated.
 *
 * Returns: a new GsweTimestamp object.
 */
GsweTimestamp *
gswe_timestamp_new_from_julian_day(gdouble julian_day)
{
    GsweTimestamp *timestamp;

    timestamp = GSWE_TIMESTAMP(g_object_new(GSWE_TYPE_TIMESTAMP,
                "julian-day", julian_day,
                NULL));

    return timestamp;
}

/**
 * gswe_timestamp_set_now_local:
 * @timestamp: the #GsweTimestamp to operate on
 * @err: a #GError to store errors
 *
 * Sets the date and time in @timestamp to the current time in the local time
 * zone. This also changes the timezone to the local time zone.
 */
void
gswe_timestamp_set_now_local(GsweTimestamp *timestamp,
                             GError        **err)
{
    GDateTime *datetime;
    gint      year,
              month,
              day,
              hour,
              minute,
              seconds,
              microsec;
    gdouble   timezone;

    datetime = g_date_time_new_now_local();
    year     = g_date_time_get_year(datetime);
    month    = g_date_time_get_month(datetime);
    day      = g_date_time_get_day_of_month(datetime);
    hour     = g_date_time_get_hour(datetime);
    minute   = g_date_time_get_minute(datetime);
    seconds  = g_date_time_get_seconds(datetime);
    microsec = g_date_time_get_microsecond(datetime);
    timezone = (gdouble)g_date_time_get_utc_offset(datetime) / 3600000000.0;
    g_date_time_unref(datetime);

    gswe_timestamp_set_gregorian_full(
            timestamp,
            year, month, day,
            hour, minute, seconds, microsec,
            timezone,
            err
        );
}

/**
 * gswe_timestamp_new_from_now_local:
 *
 * Create a new #GsweTimestamp initialised with the current local date and time.
 *
 * Returns: (transfer full): a new #GsweTimestamp object, or NULL upon an error.
 */
GsweTimestamp *
gswe_timestamp_new_from_now_local(void)
{
    GsweTimestamp *ret;
    GError        *err = NULL;

    ret = gswe_timestamp_new();
    gswe_timestamp_set_now_local(ret, &err);

    if (err) {
        g_clear_error(&err);

        return NULL;
    }

    return ret;
}
