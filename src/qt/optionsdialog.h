// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OPTIONSDIALOG_H
#define BITCOIN_QT_OPTIONSDIALOG_H

#include <QDialog>
#include <QValidator>

class BitcoinAmountField;
class OptionsModel;
class QValidatedLineEdit;

QT_BEGIN_NAMESPACE
class QBoxLayout;
class QCheckBox;
class QDataWidgetMapper;
class QSpinBox;
class QString;
class QValueComboBox;
class QWidget;
QT_END_NAMESPACE

namespace Ui {
class OptionsDialog;
}

/** Proxy address widget validator, checks for a valid proxy address.
 */
class ProxyAddressValidator : public QValidator
{
    Q_OBJECT

public:
    explicit ProxyAddressValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Preferences dialog. */
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent, bool enableWallet);
    ~OptionsDialog();

    void setModel(OptionsModel *model);
    void setMapper();

private Q_SLOTS:
    /* set OK button state (enabled / disabled) */
    void setOkButtonState(bool fState);
    void on_resetButton_clicked();
    void on_openBitcoinConfButton_clicked();
    void on_okButton_clicked();
    void on_cancelButton_clicked();
    
    void on_hideTrayIcon_stateChanged(int fState);

    void showRestartWarning(bool fPersistent = false);
    void clearStatusLabel();
    void updateProxyValidationState();
    /* query the networks, for which the default proxy is used */
    void updateDefaultProxyNets();
    void maxuploadtargetCheckboxStateChanged(int);
    void checkLineEdit();

    void incrementalrelayfee_changed();
    void blockmaxsize_changed(int);
    void blockmaxsize_increase(int);
    void blockmaxweight_changed(int);

Q_SIGNALS:
    void proxyIpChecks(QValidatedLineEdit *pUiProxyIp, int nProxyPort);

private:
    Ui::OptionsDialog *ui;
    OptionsModel *model;
    QDataWidgetMapper *mapper;

    QWidget *prevwidget;
    void FixTabOrder(QWidget *);
    void CreateOptionUI(QBoxLayout *, QWidget *, const QString& text);

    QCheckBox *walletrbf;

    QSpinBox *blockreconstructionextratxn;

    QValueComboBox *mempoolreplacement;
    QSpinBox *maxorphantx;
    BitcoinAmountField *incrementalrelayfee;
    QSpinBox *maxmempool;
    QSpinBox *mempoolexpiry;

    QCheckBox *rejectunknownscripts;
    QCheckBox *rejectspkreuse;
    BitcoinAmountField *minrelaytxfee;
    QSpinBox *bytespersigop, *bytespersigopstrict;
    QSpinBox *limitancestorcount;
    QSpinBox *limitancestorsize;
    QSpinBox *limitdescendantcount;
    QSpinBox *limitdescendantsize;
    QCheckBox *rejectbaremultisig;
    QSpinBox *datacarriersize;
    BitcoinAmountField *dustrelayfee;

    BitcoinAmountField *blockmintxfee;
    QSpinBox *blockmaxsize, *blockprioritysize, *blockmaxweight;
};

#endif // BITCOIN_QT_OPTIONSDIALOG_H
