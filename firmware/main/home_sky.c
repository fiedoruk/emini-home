/* Sun and Moon for the Home "Sky" screen.
 * Solar position: NOAA General Solar Position Calculations, the equations
 * behind gml.noaa.gov/grad/solcalc (Meeus, Astronomical Algorithms, ch. 25).
 * Lunar position: Meeus ch. 47, tables 47.A and 47.B truncated to the terms
 * that stay above one arcminute; illuminated fraction from ch. 48.
 * Rise, set and twilight come from bracketed bisection on the altitude itself
 * rather than from the closed-form hour angle, so the polar limits need no
 * special case and the answer is exact to a fraction of a second.
 * Pure, reentrant C: no heap, I/O, global mutable state or device dependency. */
#include "home_sky.h"
#include <math.h>

static const double D2R = 3.14159265358979323846 / 180.0;
static const double R2D = 180.0 / 3.14159265358979323846;
static const double SYNODIC_DAYS = 29.530588853;
static const double ELONG_DEG_PER_DAY = 360.0 / 29.530588853;
static const double AU_KM = 149597870.7;
static const double SUN_ZENITH = -0.833; /* refraction plus semidiameter */
static const double CIVIL_ZENITH = -6.0;
static const double HALF_DAY = 43200.0;

/* --- small helpers ------------------------------------------------------ */

static double wrap360(double d)
{
    d = fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}
static double wrap180(double d)
{
    return wrap360(d + 180.0) - 180.0;
}
static double clampd(double v, double a, double b)
{
    return v < a ? a : v > b ? b : v;
}
/* Delta T, TT minus UT, seconds. Espenak and Meeus polynomial for 2005-2050,
 * held flat outside that window; the Home clock never leaves it. */
static double delta_t(double utc)
{
    double y = clampd(1970.0 + utc / 31556952.0, 2005.0, 2050.0) - 2000.0;
    return 62.92 + 0.32217 * y + 0.005589 * y * y;
}
static double julian_century_tt(double utc)
{
    return ((utc + delta_t(utc)) / 86400.0 + 2440587.5 - 2451545.0) / 36525.0;
}

/* --- Sun ---------------------------------------------------------------- */

typedef struct {
    double decl_deg;    /* apparent declination */
    double eqtime_min;  /* equation of time, minutes */
    double app_lon_deg; /* apparent ecliptic longitude */
    double radius_km;   /* Earth-Sun distance */
} sun_t;

static sun_t sun_at(double utc)
{
    double t = julian_century_tt(utc);
    double l0 = wrap360(280.46646 + t * (36000.76983 + t * 0.0003032));
    double m = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    double mr = m * D2R;
    double c = sin(mr) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
               sin(2.0 * mr) * (0.019993 - 0.000101 * t) + sin(3.0 * mr) * 0.000289;
    double true_lon = l0 + c, true_anom = m + c;
    double omega = 125.04 - 1934.136 * t;
    double app_lon = true_lon - 0.00569 - 0.00478 * sin(omega * D2R);
    double e0 = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    double eps = (e0 + 0.00256 * cos(omega * D2R)) * D2R;
    double y = tan(eps / 2.0) * tan(eps / 2.0);
    sun_t s;
    s.decl_deg = asin(clampd(sin(eps) * sin(app_lon * D2R), -1.0, 1.0)) * R2D;
    s.eqtime_min = 4.0 * R2D *
                   (y * sin(2.0 * l0 * D2R) - 2.0 * e * sin(mr) +
                    4.0 * e * y * sin(mr) * cos(2.0 * l0 * D2R) -
                    0.5 * y * y * sin(4.0 * l0 * D2R) - 1.25 * e * e * sin(2.0 * mr));
    s.app_lon_deg = wrap360(app_lon);
    s.radius_km = AU_KM * 1.000001018 * (1.0 - e * e) / (1.0 + e * cos(true_anom * D2R));
    return s;
}

/* Local hour angle of the Sun, degrees, negative before local solar noon. */
static double sun_hour_angle(double lon, double utc, const sun_t *s)
{
    return wrap180(utc / 240.0 - 180.0 + s->eqtime_min * 0.25 + lon);
}

static double altitude_at(double lat, double lon, double utc)
{
    sun_t s = sun_at(utc);
    double h = sun_hour_angle(lon, utc, &s) * D2R;
    double phi = lat * D2R, dec = s.decl_deg * D2R;
    double sa = sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(h);
    return asin(clampd(sa, -1.0, 1.0)) * R2D;
}

/* Instant of upper transit nearest the guess. The hour angle runs at a steady
 * 1 degree per 240 s, so three corrections converge below a millisecond. */
static double solar_transit(double lon, double guess)
{
    double t = guess;
    for (int i = 0; i < 4; ++i) {
        sun_t s = sun_at(t);
        t -= sun_hour_angle(lon, t, &s) * 240.0;
    }
    return t;
}

/* Bisects the altitude curve for the crossing of h0 on the ascending (rising)
 * or descending branch around a transit. False when the branch never crosses. */
static bool altitude_crossing(double lat, double lon, double transit, double h0, bool rising,
                              double *out)
{
    double a = rising ? transit - HALF_DAY : transit;
    double b = rising ? transit : transit + HALF_DAY;
    double fa = altitude_at(lat, lon, a) - h0;
    if ((fa > 0.0) == (altitude_at(lat, lon, b) - h0 > 0.0))
        return false;
    for (int i = 0; i < 40 && b - a > 0.25; ++i) {
        double mid = 0.5 * (a + b), fm = altitude_at(lat, lon, mid) - h0;
        if ((fm > 0.0) == (fa > 0.0)) {
            a = mid;
            fa = fm;
        } else {
            b = mid;
        }
    }
    *out = 0.5 * (a + b);
    return true;
}

typedef struct {
    int64_t rise, set, dawn, dusk, noon;
    bool polar_day, polar_night;
    int32_t length_s;
} sun_day_t;

static void sun_day(double lat, double lon, double local_midnight, sun_day_t *d)
{
    double transit = solar_transit(lon, local_midnight + HALF_DAY);
    double top = altitude_at(lat, lon, transit);
    double bottom = altitude_at(lat, lon, transit + HALF_DAY);
    double v;
    d->noon = (int64_t)llround(transit);
    d->rise = d->set = d->dawn = d->dusk = 0;
    d->polar_day = bottom >= SUN_ZENITH;
    d->polar_night = top < SUN_ZENITH;
    if (altitude_crossing(lat, lon, transit, SUN_ZENITH, true, &v))
        d->rise = (int64_t)llround(v);
    if (altitude_crossing(lat, lon, transit, SUN_ZENITH, false, &v))
        d->set = (int64_t)llround(v);
    if (altitude_crossing(lat, lon, transit, CIVIL_ZENITH, true, &v))
        d->dawn = (int64_t)llround(v);
    if (altitude_crossing(lat, lon, transit, CIVIL_ZENITH, false, &v))
        d->dusk = (int64_t)llround(v);
    d->length_s = d->polar_day ? 86400 : d->polar_night ? 0
                  : (d->rise && d->set) ? (int32_t)(d->set - d->rise)
                                        : 0;
}

/* --- Moon --------------------------------------------------------------- */

typedef struct {
    int8_t d, m, mp, f;
    int32_t l; /* 1e-6 degrees */
    int32_t r; /* 1e-3 km */
} moon_lr_t;

typedef struct {
    int8_t d, m, mp, f;
    int32_t b; /* 1e-6 degrees */
} moon_b_t;

/* Meeus table 47.A, the thirty largest periodic terms. */
static const moon_lr_t MOON_LR[] = {
    {0, 0, 1, 0, 6288774, -20905355},  {2, 0, -1, 0, 1274027, -3699111},
    {2, 0, 0, 0, 658314, -2955968},    {0, 0, 2, 0, 213618, -569925},
    {0, 1, 0, 0, -185116, 48888},      {0, 0, 0, 2, -114332, -3149},
    {2, 0, -2, 0, 58793, 246158},      {2, -1, -1, 0, 57066, -152138},
    {2, 0, 1, 0, 53322, -170733},      {2, -1, 0, 0, 45758, -204586},
    {0, 1, -1, 0, -40923, -129620},    {1, 0, 0, 0, -34720, 108743},
    {0, 1, 1, 0, -30383, 104755},      {2, 0, 0, -2, 15327, 10321},
    {0, 0, 1, 2, -12528, 0},           {0, 0, 1, -2, 10980, 79661},
    {4, 0, -1, 0, 10675, -34782},      {0, 0, 3, 0, 10034, -23210},
    {4, 0, -2, 0, 8548, -21636},       {2, 1, -1, 0, -7888, 24208},
    {2, 1, 0, 0, -6766, 30824},        {1, 0, -1, 0, -5163, -8379},
    {1, 1, 0, 0, 4987, -16675},        {2, -1, 1, 0, 4036, -12831},
    {2, 0, 2, 0, 3994, -10445},        {4, 0, 0, 0, 3861, -11650},
    {2, 0, -3, 0, 3665, 14403},        {0, 1, -2, 0, -2689, -7003},
    {2, 0, -1, 2, -2602, 0},           {2, -1, -2, 0, 2390, 10056},
};

/* Meeus table 47.B, the twenty-one largest latitude terms. */
static const moon_b_t MOON_B[] = {
    {0, 0, 0, 1, 5128122}, {0, 0, 1, 1, 280602},  {0, 0, 1, -1, 277693},
    {2, 0, 0, -1, 173237}, {2, 0, -1, 1, 55413},  {2, 0, -1, -1, 46271},
    {2, 0, 0, 1, 32573},   {0, 0, 2, 1, 17198},   {2, 0, 1, -1, 9266},
    {0, 0, 2, -1, 8822},   {2, -1, 0, -1, 8216},  {2, 0, -2, -1, 4324},
    {2, 0, 1, 1, 4200},    {2, 1, 0, -1, -3359},  {2, -1, -1, 1, 2463},
    {2, -1, 0, 1, 2211},   {2, -1, -1, -1, 2065}, {0, 1, -1, -1, -1870},
    {4, 0, -1, -1, 1828},  {0, 1, 0, 1, -1794},   {0, 0, 0, 3, -1749},
};

typedef struct {
    double lon_deg, lat_deg, dist_km;
} moon_t;

static moon_t moon_at(double utc)
{
    double t = julian_century_tt(utc), t2 = t * t, t3 = t2 * t, t4 = t3 * t;
    double lp = 218.3164477 + 481267.88123421 * t - 0.0015786 * t2 + t3 / 538841.0 - t4 / 65194000.0;
    double d = 297.8501921 + 445267.1114034 * t - 0.0018819 * t2 + t3 / 545868.0 - t4 / 113065000.0;
    double m = 357.5291092 + 35999.0502909 * t - 0.0001536 * t2 + t3 / 24490000.0;
    double mp = 134.9633964 + 477198.8675055 * t + 0.0087414 * t2 + t3 / 69699.0 - t4 / 14712000.0;
    double f = 93.2720950 + 483202.0175233 * t - 0.0036539 * t2 - t3 / 3526000.0 + t4 / 863310000.0;
    double ecc = 1.0 - 0.002516 * t - 0.0000074 * t2;
    double sl = 0.0, sr = 0.0, sb = 0.0;
    for (unsigned i = 0; i < sizeof MOON_LR / sizeof MOON_LR[0]; ++i) {
        const moon_lr_t *k = &MOON_LR[i];
        double arg = (k->d * d + k->m * m + k->mp * mp + k->f * f) * D2R;
        double w = k->m == 0 ? 1.0 : (k->m == 1 || k->m == -1) ? ecc : ecc * ecc;
        sl += k->l * w * sin(arg);
        sr += k->r * w * cos(arg);
    }
    for (unsigned i = 0; i < sizeof MOON_B / sizeof MOON_B[0]; ++i) {
        const moon_b_t *k = &MOON_B[i];
        double arg = (k->d * d + k->m * m + k->mp * mp + k->f * f) * D2R;
        double w = k->m == 0 ? 1.0 : (k->m == 1 || k->m == -1) ? ecc : ecc * ecc;
        sb += k->b * w * sin(arg);
    }
    moon_t out;
    out.lon_deg = wrap360(lp + sl / 1000000.0);
    out.lat_deg = sb / 1000000.0;
    out.dist_km = 385000.56 + sr / 1000.0;
    return out;
}

/* Elongation of the Moon from the Sun in ecliptic longitude: 0 at new,
 * 180 at full, growing through the lunation. */
static double moon_elongation(double utc)
{
    return wrap360(moon_at(utc).lon_deg - sun_at(utc).app_lon_deg);
}

/* Newton on the elongation for the instant it equals target. The start must be
 * within a few degrees of the wanted root, which the mean rate always gives. */
static double solve_phase(double start, double target)
{
    double t = start;
    for (int i = 0; i < 8; ++i) {
        double g = wrap180(moon_elongation(t) - target);
        double slope = (wrap180(moon_elongation(t + 3600.0) - target) -
                        wrap180(moon_elongation(t - 3600.0) - target)) /
                       7200.0;
        if (!(fabs(slope) > 1e-12))
            break;
        double step = clampd(g / slope, -259200.0, 259200.0);
        t -= step;
        if (fabs(step) < 1.0)
            break;
    }
    return t;
}

static void moon_day(double utc, home_sky_t *out)
{
    moon_t mo = moon_at(utc);
    sun_t su = sun_at(utc);
    double elong = wrap360(mo.lon_deg - su.app_lon_deg);
    /* True elongation, then Meeus 48.3: the phase angle seen from the Moon. */
    double psi = acos(clampd(cos(mo.lat_deg * D2R) * cos((mo.lon_deg - su.app_lon_deg) * D2R),
                             -1.0, 1.0));
    double phase_angle = atan2(su.radius_km * sin(psi), mo.dist_km - su.radius_km * cos(psi));
    out->moon_fraction = clampd((1.0 + cos(phase_angle)) / 2.0, 0.0, 1.0);
    out->moon_phase = (int)(((long)llround(elong / 45.0)) % 8);

    double prev_new = solve_phase(utc - elong / ELONG_DEG_PER_DAY * 86400.0, 0.0);
    if (prev_new > utc)
        prev_new = solve_phase(prev_new - SYNODIC_DAYS * 86400.0, 0.0);
    double next_new = solve_phase(prev_new + SYNODIC_DAYS * 86400.0, 0.0);
    double next_full = solve_phase(utc + wrap360(180.0 - elong) / ELONG_DEG_PER_DAY * 86400.0, 180.0);
    if (next_full < utc)
        next_full = solve_phase(next_full + SYNODIC_DAYS * 86400.0, 180.0);
    out->moon_age_days = (utc - prev_new) / 86400.0;
    out->days_to_new = (int)llround((next_new - utc) / 86400.0);
    out->days_to_full = (int)llround((next_full - utc) / 86400.0);
    if (out->days_to_new < 0)
        out->days_to_new = 0;
    if (out->days_to_full < 0)
        out->days_to_full = 0;
}

/* --- public ------------------------------------------------------------- */

bool home_sky_day(double lat, double lon, int64_t local_midnight_utc, home_sky_t *out)
{
    if (!out || !isfinite(lat) || !isfinite(lon) || fabs(lat) > 90.0 || fabs(lon) > 180.0)
        return false;
    double midnight = (double)local_midnight_utc;
    sun_day_t today, yesterday;
    sun_day(lat, lon, midnight, &today);
    sun_day(lat, lon, midnight - 86400.0, &yesterday);

    out->sunrise = today.rise;
    out->sunset = today.set;
    out->civil_dawn = today.dawn;
    out->civil_dusk = today.dusk;
    out->solar_noon = today.noon;
    out->polar_day = today.polar_day;
    out->polar_night = today.polar_night;
    out->day_length_s = today.length_s;
    out->day_length_delta_s = today.length_s - yesterday.length_s;
    moon_day(midnight + HALF_DAY, out);
    return true;
}

double home_sky_altitude(double lat, double lon, int64_t utc)
{
    if (!isfinite(lat) || !isfinite(lon) || fabs(lat) > 90.0 || fabs(lon) > 180.0)
        return 0.0;
    return altitude_at(lat, lon, (double)utc);
}
