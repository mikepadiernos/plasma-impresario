/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RetryBackoff.h"

#include <algorithm>
#include <QRandomGenerator>

namespace Common {

RetryBackoff::RetryBackoff(int initialMs, int maximumMs, double jitter)
    : m_initial(std::max(1, initialMs))
    , m_maximum(std::max(m_initial, maximumMs))
    , m_jitter(std::clamp(jitter, 0.0, 1.0))
    , m_random([]() { return QRandomGenerator::global()->generateDouble(); })
{
}

int RetryBackoff::baseDelay() const
{
    qint64 delay = m_initial;
    for (int i = 0; i < m_attempts && delay < m_maximum; ++i)
        delay *= 2;
    return int(std::min<qint64>(delay, m_maximum));
}

int RetryBackoff::nextDelay()
{
    const int base = baseDelay();
    ++m_attempts;
    // From base - jitter to base + jitter, kept within the first delay and the limit
    const double spread = (m_random() * 2.0 - 1.0) * m_jitter * base;
    const qint64 delay = qint64(base + spread);
    return int(std::clamp<qint64>(delay, m_initial, m_maximum));
}

int RetryBackoff::attempts() const
{
    return m_attempts;
}

void RetryBackoff::reset()
{
    m_attempts = 0;
}

int RetryBackoff::initialDelay() const
{
    return m_initial;
}

int RetryBackoff::maximumDelay() const
{
    return m_maximum;
}

void RetryBackoff::setRandomSource(const std::function<double()> &random)
{
    if (random)
        m_random = random;
}

}
