// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "bitcoinamountfield.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "consensus/consensus.h" // for MAX_BLOCK_SERIALIZED_SIZE
#include "validation.h" // for DEFAULT_SCRIPTCHECK_THREADS and MAX_SCRIPTCHECK_THREADS
#include "netbase.h"
#include "primitives/transaction.h" // for WITNESS_SCALE_FACTOR
#include "txdb.h" // for -dbcache defaults
#include "txmempool.h" // for maxmempoolMinimum

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // for CWallet::GetRequiredFee()
#endif

#include <QBoxLayout>
#include <QDataWidgetMapper>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QSpacerItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

void OptionsDialog::FixTabOrder(QWidget * const o)
{
    BitcoinAmountField * const af = qobject_cast<BitcoinAmountField *>(o);
    if (af) {
        af->setupTabChain(prevwidget);
    } else {
        setTabOrder(prevwidget, o);
    }
    prevwidget = o;
}

void OptionsDialog::CreateOptionUI(QBoxLayout * const layout, QWidget * const o, const QString& text)
{
    QWidget * const parent = o->parentWidget();
    const QStringList text_parts = text.split("%s");

    QHBoxLayout * const horizontalLayout = new QHBoxLayout();

    QLabel * const labelBefore = new QLabel(parent);
    labelBefore->setText(text_parts[0]);
    labelBefore->setTextFormat(Qt::PlainText);
    labelBefore->setBuddy(o);
    labelBefore->setToolTip(o->toolTip());

    horizontalLayout->addWidget(labelBefore);
    horizontalLayout->addWidget(o);

    QLabel * const labelAfter = new QLabel(parent);
    labelAfter->setText(text_parts[1]);
    labelAfter->setTextFormat(Qt::PlainText);
    labelAfter->setBuddy(o);
    labelAfter->setToolTip(o->toolTip());

    horizontalLayout->addWidget(labelAfter);

    QSpacerItem * const horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    horizontalLayout->addItem(horizontalSpacer);

    layout->addLayout(horizontalLayout);

    FixTabOrder(o);
}

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0)
{
    ui->setupUi(this);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    ui->networkPort->setValidator(new QIntValidator(1024, 65535, this));
    connect(ui->networkPort, SIGNAL(textChanged(const QString&)), this, SLOT(checkLineEdit()));

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpTor->setEnabled(false);
    ui->proxyPortTor->setEnabled(false);
    ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyIpTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyPortTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    ui->maxuploadtarget->setMinimum(144 /* MB/day */);
    ui->maxuploadtarget->setMaximum(std::numeric_limits<int>::max());
    connect(ui->maxuploadtargetCheckbox, SIGNAL(stateChanged(int)), this, SLOT(maxuploadtargetCheckboxStateChanged(int)));

    prevwidget = ui->tabWidget;

    walletrbf = new QCheckBox(ui->tabWallet);
    walletrbf->setText(tr("Request Replace-By-Fee"));
    walletrbf->setToolTip(tr("Indicates that the sender may wish to replace this transaction with a new one paying higher fees (prior to being confirmed). Can be overridden per send."));
    ui->verticalLayout_Wallet->insertWidget(0, walletrbf);
    FixTabOrder(walletrbf);

    /* Network tab */
    QLayoutItem *spacer = ui->verticalLayout_Network->takeAt(ui->verticalLayout_Network->count() - 1);
    prevwidget = dynamic_cast<QWidgetItem*>(ui->verticalLayout_Network->itemAt(ui->verticalLayout_Network->count() - 1))->widget();

    blockreconstructionextratxn = new QSpinBox(ui->tabNetwork);
    blockreconstructionextratxn->setMinimum(0);
    blockreconstructionextratxn->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(ui->verticalLayout_Network, blockreconstructionextratxn, tr("Keep at most %s extra transactions in memory for compact block reconstruction"));

    ui->verticalLayout_Network->addItem(spacer);

    prevwidget = ui->peerbloomfilters;

    /* Mempool tab */

    QWidget * const tabMempool = new QWidget();
    QVBoxLayout * const verticalLayout_Mempool = new QVBoxLayout(tabMempool);
    ui->tabWidget->insertTab(ui->tabWidget->indexOf(ui->tabWindow), tabMempool, tr("Mem&pool"));

    mempoolreplacement = new QValueComboBox(tabMempool);
    mempoolreplacement->addItem(QString("never"), QVariant("never"));
    mempoolreplacement->addItem(QString("with a higher mining fee, and opt-in"), QVariant("fee,optin"));
    mempoolreplacement->addItem(QString("with a higher mining fee (no opt-out)"), QVariant("fee,-optin"));
    CreateOptionUI(verticalLayout_Mempool, mempoolreplacement, tr("Transaction &replacement: %s"));

    maxorphantx = new QSpinBox(tabMempool);
    maxorphantx->setMinimum(0);
    maxorphantx->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Mempool, maxorphantx, tr("Keep at most %s unconnected transactions in memory"));

    maxmempool = new QSpinBox(tabMempool);
    const int64_t nMempoolSizeMinMB = maxmempoolMinimum(gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT));
    maxmempool->setMinimum(nMempoolSizeMinMB);
    maxmempool->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Mempool, maxmempool, tr("Keep the transaction memory pool below %s MB"));

    incrementalrelayfee = new BitcoinAmountField(tabMempool);
    connect(incrementalrelayfee, SIGNAL(valueChanged()), this, SLOT(incrementalrelayfee_changed()));
    CreateOptionUI(verticalLayout_Mempool, incrementalrelayfee, tr("Require transaction fees to be at least %s per kB higher than transactions they are replacing."));

    mempoolexpiry = new QSpinBox(tabMempool);
    mempoolexpiry->setMinimum(1);
    mempoolexpiry->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Mempool, mempoolexpiry, tr("Do not keep transactions in memory more than %s hours"));

    QGroupBox * const groupBox_Spamfiltering = new QGroupBox(tabMempool);
    groupBox_Spamfiltering->setTitle(tr("Spam filtering"));
    QVBoxLayout * const verticalLayout_Spamfiltering = new QVBoxLayout(groupBox_Spamfiltering);

    rejectunknownscripts = new QCheckBox(groupBox_Spamfiltering);
    rejectunknownscripts->setText(tr("Ignore unrecognised receiver scripts"));
    rejectunknownscripts->setToolTip(tr("With this option enabled, unrecognised receiver (\"pubkey\") scripts will be ignored. Unrecognisable scripts could be used to bypass further spam filters. If your software is outdated, they may also be used to trick you into thinking you were sent bitcoins that will never confirm."));
    verticalLayout_Spamfiltering->addWidget(rejectunknownscripts);
    FixTabOrder(rejectunknownscripts);

    rejectspkreuse = new QCheckBox(groupBox_Spamfiltering);
    rejectspkreuse->setText(tr("Disallow most address reuse"));
    rejectspkreuse->setToolTip(tr("With this option enabled, your memory pool will only allow each unique payment destination to be used once, effectively deprioritising address reuse. Address reuse is not technically supported, and harms the privacy of all Bitcoin users. It also has limited real-world utility, and has been known to be common with spam."));
    verticalLayout_Spamfiltering->addWidget(rejectspkreuse);
    FixTabOrder(rejectspkreuse);

    minrelaytxfee = new BitcoinAmountField(groupBox_Spamfiltering);
    CreateOptionUI(verticalLayout_Spamfiltering, minrelaytxfee, tr("Consider transaction fees less than %s per kB relayed to be worthless."));

    bytespersigop = new QSpinBox(groupBox_Spamfiltering);
    bytespersigop->setMinimum(1);
    bytespersigop->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, bytespersigop, tr("Treat each consensus-counted sigop as at least %s bytes."));

    bytespersigopstrict = new QSpinBox(groupBox_Spamfiltering);
    bytespersigopstrict->setMinimum(1);
    bytespersigopstrict->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, bytespersigopstrict, tr("Ignore transactions with fewer than %s bytes per potentially-executed sigop."));

    limitancestorcount = new QSpinBox(groupBox_Spamfiltering);
    limitancestorcount->setMinimum(1);
    limitancestorcount->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, limitancestorcount, tr("Ignore transactions with %s or more unconfirmed ancestors."));

    limitancestorsize = new QSpinBox(groupBox_Spamfiltering);
    limitancestorsize->setMinimum(1);
    limitancestorsize->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, limitancestorsize, tr("Ignore transactions whose size with all unconfirmed ancestors exceeds %s kilobytes."));

    limitdescendantcount = new QSpinBox(groupBox_Spamfiltering);
    limitdescendantcount->setMinimum(1);
    limitdescendantcount->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, limitdescendantcount, tr("Ignore transactions if any ancestor would have %s or more unconfirmed descendants."));

    limitdescendantsize = new QSpinBox(groupBox_Spamfiltering);
    limitdescendantsize->setMinimum(1);
    limitdescendantsize->setMaximum(std::numeric_limits<int>::max());
    CreateOptionUI(verticalLayout_Spamfiltering, limitdescendantsize, tr("Ignore transactions if any ancestor would have more than %s kilobytes of unconfirmed descendants."));

    rejectbaremultisig = new QCheckBox(groupBox_Spamfiltering);
    rejectbaremultisig->setText(tr("Ignore bare/exposed \"multisig\" scripts"));
    rejectbaremultisig->setToolTip(tr("Spam is sometimes disguised to appear as if it is an old-style N-of-M multi-party transaction, where most of the keys are really bogus. At the same time, legitimate multi-party transactions typically have always used P2SH format (which is not filtered by this option), which is more secure."));
    verticalLayout_Spamfiltering->addWidget(rejectbaremultisig);
    FixTabOrder(rejectbaremultisig);

    datacarriersize = new QSpinBox(groupBox_Spamfiltering);
    datacarriersize->setMinimum(0);
    datacarriersize->setMaximum(std::numeric_limits<int>::max());
    datacarriersize->setToolTip(tr("Since 2014, a specific method for attaching arbitrary data to transactions has been recognised as not requiring space in the coin database. Since it is sometimes impractical to detect small spam disguised as ordinary transactions, it is sometimes considered beneficial to treat these less harmful data attachments as equals to legitimate usage."));
    CreateOptionUI(verticalLayout_Spamfiltering, datacarriersize, tr("Ignore transactions with additional data larger than %s bytes."));

    dustrelayfee = new BitcoinAmountField(groupBox_Spamfiltering);
    CreateOptionUI(verticalLayout_Spamfiltering, dustrelayfee, tr("Ignore transactions with values that would cost more to spend at a fee rate of %s per kB."));

    verticalLayout_Mempool->addWidget(groupBox_Spamfiltering);

    verticalLayout_Mempool->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    /* Mining tab */

    QWidget * const tabMining = new QWidget();
    QVBoxLayout * const verticalLayout_Mining = new QVBoxLayout(tabMining);
    ui->tabWidget->insertTab(ui->tabWidget->indexOf(ui->tabWindow), tabMining, tr("M&ining"));

    verticalLayout_Mining->addWidget(new QLabel(tr("<strong>Note that mining is heavily influenced by the settings on the Mempool tab.</strong>")));

    blockmintxfee = new BitcoinAmountField(tabMining);
    CreateOptionUI(verticalLayout_Mining, blockmintxfee, tr("Only mine transactions paying a fee of at least %s per kB."));

    blockmaxsize = new QSpinBox(tabMining);
    blockmaxsize->setMinimum(1);
    blockmaxsize->setMaximum((MAX_BLOCK_SERIALIZED_SIZE - 1000) / 1000);
    connect(blockmaxsize, SIGNAL(valueChanged(int)), this, SLOT(blockmaxsize_changed(int)));
    CreateOptionUI(verticalLayout_Mining, blockmaxsize, tr("Never mine a block larger than %s kB."));

    blockprioritysize = new QSpinBox(tabMining);
    blockprioritysize->setMinimum(0);
    blockprioritysize->setMaximum(blockmaxsize->maximum());
    connect(blockprioritysize, SIGNAL(valueChanged(int)), this, SLOT(blockmaxsize_increase(int)));
    CreateOptionUI(verticalLayout_Mining, blockprioritysize, tr("Mine first %s kB of transactions sorted by coin-age priority."));

    blockmaxweight = new QSpinBox(tabMining);
    blockmaxweight->setMinimum(1);
    blockmaxweight->setMaximum((MAX_BLOCK_WEIGHT-4000) / 1000);
    connect(blockmaxweight, SIGNAL(valueChanged(int)), this, SLOT(blockmaxweight_changed(int)));
    CreateOptionUI(verticalLayout_Mining, blockmaxweight, tr("Never mine a block weighing more than %s,000."));

    verticalLayout_Mining->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    /* Window elements init */
#ifdef Q_OS_MAC
    /* remove Window tab on Mac */
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
#endif

    /* remove Wallet tab in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
    }

    /* Display elements init */
    QDir translations(":translations");

    ui->bitcoinAtStartup->setToolTip(ui->bitcoinAtStartup->toolTip().arg(tr(PACKAGE_NAME)));
    ui->bitcoinAtStartup->setText(ui->bitcoinAtStartup->text().arg(tr(PACKAGE_NAME)));

    ui->openBitcoinConfButton->setToolTip(ui->openBitcoinConfButton->toolTip().arg(tr(PACKAGE_NAME)));

    ui->lang->setToolTip(ui->lang->toolTip().arg(tr(PACKAGE_NAME)));
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString &langStr : translations.entryList())
    {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language - country (locale name)", e.g. "German - Germany (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" - ") + QLocale::countryToString(locale.country()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
        else
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language (locale name)", e.g. "German (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
    }
#if QT_VERSION >= 0x040700
    ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");
#endif

    ui->unit->setModel(new BitcoinUnits(this));

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* setup/change UI elements when proxy IPs are invalid/valid */
    ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
    connect(ui->proxyIp, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPort, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPortTor, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *_model)
{
    this->model = _model;

    if(_model)
    {
        /* check if client restart is needed and show persistent message */
        if (_model->isRestartRequired())
            showRestartWarning(true);

        QString strLabel = _model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
            strLabel = tr("none");
        ui->overriddenByCommandLineLabel->setText(strLabel);

        mapper->setModel(_model);
        setMapper();
        mapper->toFirst();

        updateDefaultProxyNets();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->databaseCache, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    connect(ui->threadsScriptVerif, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    /* Wallet */
    connect(ui->spendZeroConfChange, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Network */
    connect(ui->networkPort, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));
    connect(ui->allowIncoming, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocksTor, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->peerbloomfilters, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Mempool */
    connect(rejectspkreuse, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Display */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
    connect(ui->thirdPartyTxUrls, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);

    /* Wallet */
    mapper->addMapping(walletrbf, OptionsModel::walletrbf);
    mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);

    /* Network */
    mapper->addMapping(ui->networkPort, OptionsModel::NetworkPort);
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
    mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
    mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

    int current_maxuploadtarget = model->data(model->index(OptionsModel::maxuploadtarget, 0), Qt::EditRole).toInt();
    if (current_maxuploadtarget == 0) {
        ui->maxuploadtargetCheckbox->setChecked(false);
        ui->maxuploadtarget->setEnabled(false);
        ui->maxuploadtarget->setValue(ui->maxuploadtarget->minimum());
    } else {
        if (current_maxuploadtarget < ui->maxuploadtarget->minimum()) {
            ui->maxuploadtarget->setMinimum(current_maxuploadtarget);
        }
        ui->maxuploadtargetCheckbox->setChecked(true);
        ui->maxuploadtarget->setEnabled(true);
        ui->maxuploadtarget->setValue(current_maxuploadtarget);
    }

    mapper->addMapping(ui->peerbloomfilters, OptionsModel::peerbloomfilters);

    mapper->addMapping(blockreconstructionextratxn, OptionsModel::blockreconstructionextratxn);

    /* Mempool tab */

    QVariant current_mempoolreplacement = model->data(model->index(OptionsModel::mempoolreplacement, 0), Qt::EditRole);
    int current_mempoolreplacement_index = mempoolreplacement->findData(current_mempoolreplacement);
    if (current_mempoolreplacement_index == -1) {
        mempoolreplacement->addItem(current_mempoolreplacement.toString(), current_mempoolreplacement);
        current_mempoolreplacement_index = mempoolreplacement->count() - 1;
    }
    mempoolreplacement->setCurrentIndex(current_mempoolreplacement_index);

    mapper->addMapping(maxorphantx, OptionsModel::maxorphantx);
    mapper->addMapping(maxmempool, OptionsModel::maxmempool);
    mapper->addMapping(incrementalrelayfee, OptionsModel::incrementalrelayfee);
    mapper->addMapping(mempoolexpiry, OptionsModel::mempoolexpiry);

    mapper->addMapping(rejectunknownscripts, OptionsModel::rejectunknownscripts);
    mapper->addMapping(rejectspkreuse, OptionsModel::rejectspkreuse);
    mapper->addMapping(minrelaytxfee, OptionsModel::minrelaytxfee);
    mapper->addMapping(bytespersigop, OptionsModel::bytespersigop);
    mapper->addMapping(bytespersigopstrict, OptionsModel::bytespersigopstrict);
    mapper->addMapping(limitancestorcount, OptionsModel::limitancestorcount);
    mapper->addMapping(limitancestorsize, OptionsModel::limitancestorsize);
    mapper->addMapping(limitdescendantcount, OptionsModel::limitdescendantcount);
    mapper->addMapping(limitdescendantsize, OptionsModel::limitdescendantsize);
    mapper->addMapping(rejectbaremultisig, OptionsModel::rejectbaremultisig);
    mapper->addMapping(datacarriersize, OptionsModel::datacarriersize);
    mapper->addMapping(dustrelayfee, OptionsModel::dustrelayfee);

    /* Mining tab */

    mapper->addMapping(blockmintxfee, OptionsModel::blockmintxfee);
    mapper->addMapping(blockmaxsize, OptionsModel::blockmaxsize);
    mapper->addMapping(blockprioritysize, OptionsModel::blockprioritysize);
    mapper->addMapping(blockmaxweight, OptionsModel::blockmaxweight);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->displayAddresses, OptionsModel::DisplayAddresses);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
}

void OptionsDialog::checkLineEdit()
{
    QLineEdit * const lineedit = qobject_cast<QLineEdit*>(QObject::sender());
    if (lineedit->hasAcceptableInput()) {
        lineedit->setStyleSheet("");
    } else {
        lineedit->setStyleSheet("color: red;");
    }
}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::maxuploadtargetCheckboxStateChanged(const int state)
{
    ui->maxuploadtarget->setEnabled(state);
}

void OptionsDialog::incrementalrelayfee_changed()
{
    if (incrementalrelayfee->value() > minrelaytxfee->value()) {
        minrelaytxfee->setValue(incrementalrelayfee->value());
    }
}

void OptionsDialog::blockmaxsize_changed(int i)
{
    if (blockprioritysize->value() > i) {
        blockprioritysize->setValue(i);
    }

    if (blockmaxweight->value() < i) {
        blockmaxweight->setValue(i);
    } else if (blockmaxweight->value() > i * WITNESS_SCALE_FACTOR) {
        blockmaxweight->setValue(i * WITNESS_SCALE_FACTOR);
    }
}

void OptionsDialog::blockmaxsize_increase(int i)
{
    if (blockmaxsize->value() < i) {
        blockmaxsize->setValue(i);
    }
}

void OptionsDialog::blockmaxweight_changed(int i)
{
    if (blockmaxsize->value() < i / WITNESS_SCALE_FACTOR) {
        blockmaxsize->setValue(i / WITNESS_SCALE_FACTOR);
    } else if (blockmaxsize->value() > i) {
        blockmaxsize->setValue(i);
    }
}

void OptionsDialog::on_resetButton_clicked()
{
    if(model)
    {
        // confirmation dialog
        QStringList items;
        QString strPrefix = tr("Use policy defaults for %1");
        items << strPrefix.arg(tr(PACKAGE_NAME));
        items << strPrefix.arg(tr("Bitcoin Core")+" ");

        QInputDialog dialog(this);
        dialog.setWindowTitle(tr("Confirm options reset"));
        dialog.setLabelText(tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"));
        dialog.setComboBoxItems(items);
        dialog.setTextValue(items[0]);
        dialog.setComboBoxEditable(false);

        if (!dialog.exec()) {
            return;
        }

        /* reset all options and close GUI */
        model->Reset();
        model->setData(model->index(OptionsModel::corepolicy, 0), items.indexOf(dialog.textValue()));
        QApplication::quit();
    }
}

void OptionsDialog::on_openBitcoinConfButton_clicked()
{
    /* explain the purpose of the config file */
    QMessageBox::information(this, tr("Configuration options"),
        tr("The configuration file is used to specify advanced user options which override GUI settings. "
           "Additionally, any command-line options will override this configuration file."));

    /* show an error if there was some problem opening the file */
    if (!GUIUtil::openBitcoinConf())
        QMessageBox::critical(this, tr("Error"), tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked()
{
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget * const tab = ui->tabWidget->widget(i);
        Q_FOREACH(QObject* o, tab->children()) {
            QLineEdit * const lineedit = qobject_cast<QLineEdit*>(o);
            if (lineedit && !lineedit->hasAcceptableInput()) {
                int row = mapper->mappedSection(lineedit);
                if (model->data(model->index(row, 0), Qt::EditRole) == lineedit->text()) {
                    // Allow unchanged fields through
                    continue;
                }
                ui->tabWidget->setCurrentWidget(tab);
                lineedit->setFocus(Qt::OtherFocusReason);
                lineedit->selectAll();
                QMessageBox::critical(this, tr("Invalid setting"), tr("The value entered is invalid."));
                return;
            }
        }
    }

    if (ui->maxuploadtargetCheckbox->isChecked()) {
        model->setData(model->index(OptionsModel::maxuploadtarget, 0), ui->maxuploadtarget->value());
    } else {
        model->setData(model->index(OptionsModel::maxuploadtarget, 0), 0);
    }

    model->setData(model->index(OptionsModel::mempoolreplacement, 0), mempoolreplacement->itemData(mempoolreplacement->currentIndex()));

    mapper->submit();
    accept();
    updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
    }
}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
    if (model && model->isRestartRequired()) {
        showRestartWarning(true);
    }
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
    QValidatedLineEdit *otherProxyWidget = (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
    if (pUiProxyIp->isValid() && (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0) && (!ui->proxyPortTor->isEnabled() || ui->proxyPortTor->text().toInt() > 0))
    {
        setOkButtonState(otherProxyWidget->isValid()); //only enable ok button if both proxys are valid
        clearStatusLabel();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    proxyType proxy;
    std::string strProxy;
    QString strDefaultProxyGUI;

    GetProxy(NET_IPV4, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv4->setChecked(true) : ui->proxyReachIPv4->setChecked(false);

    GetProxy(NET_IPV6, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv6->setChecked(true) : ui->proxyReachIPv6->setChecked(false);

    GetProxy(NET_TOR, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachTor->setChecked(true) : ui->proxyReachTor->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString().c_str(), 9050));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
