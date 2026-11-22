# Mail client inspirations

This page records the main product and UX inspirations for Plasma Mail. These are reference points for workflow and layout, not direct feature cloning.

## KDE / Trojitá

- Project: https://github.com/KDE/trojita
- Why it matters: Trojitá is the closest baseline for this project because it is a Qt-based IMAP client built in the KDE ecosystem.
- Takeaways:
  - Local-first, desktop-native mail interaction
  - Clear separation between account/folder navigation and message reading
  - Strong emphasis on IMAP correctness and protocol cleanliness
  - KDE-style app behavior, especially as a lightweight desktop client

## Aqua Mail

- Project: https://www.aqua-mail.com/
- Why it matters: Aqua Mail is a strong reference for modern mobile-first mail workflow clarity.
- Takeaways:
  - Simple, readable message list density
  - Thoughtful handling of quick actions and conversation flow
  - Strong support for account grouping and low-friction navigation
  - Fast, readable filters and search flows

## The Bat!

- Project: https://www.ritlabs.com/en/products/thebat/
- Why it matters: The Bat! provides a classic power-user messaging interface with a traditional desktop layout.
- Takeaways:
  - Dense but highly functional email management
  - Robust folder and message organization patterns
  - Useful mail-management conventions for keyboard-centric users
  - Traditional workflow structure with strong prioritization of information density

## Kontact

- Project: https://kontact.kde.org/
- Why it matters: Kontact is the most relevant KDE integration reference for cross-application behavior and desktop polish.
- Takeaways:
  - Natural fit with KDE and Plasma conventions
  - Integration-minded interaction modes and common desktop patterns
  - Clean treatment of personal information management tasks
  - Strong alignment with KDE account and system integration expectations

## Design synthesis

Plasma Mail should combine:

- The KDE-native and IMAP-first discipline of Trojitá
- The clarity and quick-action flow of Aqua Mail
- The power-user density and organization model of The Bat!
- The desktop integration and KDE polish of Kontact

This should produce a client that feels fast, familiar, and well-integrated with Plasma without simply copying any single application.
