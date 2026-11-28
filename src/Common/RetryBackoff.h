/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef COMMON_RETRYBACKOFF_H
#define COMMON_RETRYBACKOFF_H

#include <functional>

namespace Common {

/** @short Delays between attempts which double up to a limit, spread by a random jitter

When the network comes back, every account which lost it would otherwise try again at the same moment; the jitter
spreads their attempts. The jitter never makes a delay shorter than the first one nor longer than the limit.
*/
class RetryBackoff
{
public:
    /** @short Start at @arg initialMs, double up to @arg maximumMs, and spread each delay by up to @arg jitter of it */
    RetryBackoff(int initialMs, int maximumMs, double jitter = 0.2);

    /** @short The delay before the next attempt; each call counts as one attempt */
    int nextDelay();
    /** @short The delay before the next attempt without the jitter, without counting an attempt */
    int baseDelay() const;
    /** @short Attempts counted since the last reset() */
    int attempts() const;
    /** @short Start again from the first delay, as after a success */
    void reset();

    int initialDelay() const;
    int maximumDelay() const;

    /** @short Use @arg random, returning numbers from 0 up to 1, instead of the default generator; for tests */
    void setRandomSource(const std::function<double()> &random);

private:
    int m_initial;
    int m_maximum;
    double m_jitter;
    int m_attempts = 0;
    std::function<double()> m_random;
};

}

#endif
