#ifndef TEST_ACCOUNTREGISTRY_H
#define TEST_ACCOUNTREGISTRY_H

#include <QObject>

/** @short Verify account lifecycle, isolation and persistence in AccountRegistry */
class AccountRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void addsAccountsInOrderWithUniqueIds();
    void firstAccountBecomesActive();
    void neverReusesRemovedIds();
    void settingsAreIsolatedPerAccount();
    void removeDeletesStoredConfiguration();
    void removingActiveAccountActivatesFirstRemaining();
    void removingLastAccountClearsActive();
    void persistsAccountsNamesAndActiveAccount();
    void emitsLifecycleSignals();
    void unknownIdsAreHandled();
};

#endif
