#include "../src/power_button.h"

static_assert(!power_button::warningDue(4999), "no warning before five seconds");
static_assert(power_button::warningDue(5000), "warn at five seconds");
static_assert(!power_button::shutdownDue(9999, false), "do not shut down before ten seconds");
static_assert(power_button::shutdownDue(10000, false), "shut down at ten seconds");
static_assert(!power_button::shutdownDue(10000, true), "do not shut down when this role is running");
static_assert(power_button::secondsRemaining(5000) == 5, "show five-second countdown");
static_assert(power_button::secondsRemaining(6000) == 4, "count down once per second");
static_assert(power_button::secondsRemaining(9999) == 1, "show final second");
