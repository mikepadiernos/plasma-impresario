#ifndef TEST_ACCOUNTCAPABILITIES_H
#define TEST_ACCOUNTCAPABILITIES_H

#include <QObject>

/** @short Verify protocol capability tables and stored protocol names */
class AccountCapabilitiesTest : public QObject
{
    Q_OBJECT

private slots:
    void imapSupportsEveryOperation_data();
    void imapSupportsEveryOperation();
    void pop3LacksServerSideOperations_data();
    void pop3LacksServerSideOperations();
    void pop3KeepsMessagesOnlyByOption();
    void jmapMatchesImapExceptPush();
    void limitationIsEmptyOnlyWhenSupported_data();
    void limitationIsEmptyOnlyWhenSupported();
    void parsesStoredProtocolNames_data();
    void parsesStoredProtocolNames();
};

#endif
