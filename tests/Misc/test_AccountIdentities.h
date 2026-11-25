#ifndef TEST_ACCOUNTIDENTITIES_H
#define TEST_ACCOUNTIDENTITIES_H

#include <QObject>

/** @short Verify per-account identity defaults, the shared-identity option and
 * migration */
class AccountIdentitiesTest : public QObject {
    Q_OBJECT

private slots:
    void accountsUseTheirOwnIdentitiesByDefault();
    void newAccountStartsWithOwnEmptyIdentities();
    void sharedIdentitiesCanBeChosen();
    void migrationCopiesSharedIdentities();
    void migrationKeepsExistingOwnIdentities();
    void migrationRunsOnlyOnce();
    void migrationLeavesSharedIdentitiesUnchanged();
};

#endif
