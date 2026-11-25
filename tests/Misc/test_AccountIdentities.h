#ifndef TEST_ACCOUNTIDENTITIES_H
#define TEST_ACCOUNTIDENTITIES_H

#include <QObject>

/** @short Verify per-account identity defaults, the shared and no-identity
 * options, and migration */
class AccountIdentitiesTest : public QObject {
    Q_OBJECT

private slots:
    void accountsUseTheirOwnIdentitiesByDefault();
    void newAccountStartsWithOwnEmptyIdentities();
    void sharedIdentitiesCanBeChosen();
    void noIdentityCanBeChosen();
    void earlierSharedChoiceIsKept();
    void migrationCopiesSharedIdentities();
    void migrationKeepsExistingOwnIdentities();
    void migrationRunsOnlyOnce();
    void migrationLeavesSharedIdentitiesUnchanged();
    void loginAddressIsAPlainEmailAddress();
    void loginAddressIsAPlainEmailAddress_data();
};

#endif
