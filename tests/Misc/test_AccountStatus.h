#ifndef TEST_ACCOUNTSTATUS_H
#define TEST_ACCOUNTSTATUS_H

#include <QObject>

/** @short Verify per-account status tracking */
class AccountStatusTest : public QObject
{
    Q_OBJECT

private slots:
    void unknownAccountIsIdle();
    void recordsStatusAndDetail();
    void emitsOnlyOnChange();
    void forgetResetsToIdle();
    void attentionKinds_data();
    void attentionKinds();
    void listsAccountsNeedingAttention();
    void raisesAttentionOnceUntilConnected();
    void labelsAreDistinct();
};

#endif
