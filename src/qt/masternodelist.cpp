#include "masternodelist.h"
#include "ui_masternodelist.h"

#include "activemasternode.h"
#include "init.h"
#include "guiutil.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "sync.h"
#include "walletmodel.h"
#include "createmasternodedlg.h"

#include <QTimer>
#include <QTime>
#include <QMessageBox>

int GetOffsetFromUtc() {
#if QT_VERSION < 0x050200
	const QDateTime dateTime1 = QDateTime::currentDateTime();
	const QDateTime dateTime2 = QDateTime(dateTime1.date(), dateTime1.time(),
			Qt::UTC);
	return dateTime1.secsTo(dateTime2);
#else
	return QDateTime::currentDateTime().offsetFromUtc();
#endif
}

MasternodeList::MasternodeList(QDialog *parent) :
		QDialog(parent), ui(new Ui::MasternodeList), model(0) {
	ui->setupUi(this);

	ui->startButton->setEnabled(false);
	ui->removeMNButton->setEnabled(false);

	int columnAliasWidth = 100;
	int columnAddressWidth = 200;
	int columnProtocolWidth = 60;
	int columnStatusWidth = 80;
	int columnActiveWidth = 130;
	int columnLastSeenWidth = 130;
	QPalette pal = ui->tabWidget->palette();
	pal.setColor(ui->tabWidget->backgroundRole(), QColor(0x08, 0x33, 0x46));
	ui->tabWidget->setPalette(pal);
	ui->tabWidget->tabBar()->setPalette(pal);
	ui->tableWidgetMyMasternodes->setPalette(pal);
	ui->tableWidgetMasternodes->setPalette(pal);
	ui->tabMyMasternodes->setPalette(pal);
	ui->tabAllMasternodes->setPalette(pal);
	ui->tabWidget->setAutoFillBackground(true);
	ui->tabWidget->tabBar()->setAutoFillBackground(true);
	ui->tableWidgetMyMasternodes->setAutoFillBackground(true);
	ui->tableWidgetMasternodes->setAutoFillBackground(true);
	ui->tabMyMasternodes->setAutoFillBackground(true);
	ui->tabAllMasternodes->setAutoFillBackground(true);

/*	ui->tabWidget->tabBar()->setStyleSheet("QTabBar::tab:selected {\
	                                   color: #ffffff;\
	                                   background-color: rgb(0x00,0x78,0xa0);\
	                               } QTabBar::tab {\
		                                   color: #000000;\
		                                   background-color: rgb(0x08,0x33,0xa0);\
		                               }");*/
	ui->tableWidgetMyMasternodes->setColumnWidth(0, columnAliasWidth);
	ui->tableWidgetMyMasternodes->setColumnWidth(1, columnAddressWidth);
	ui->tableWidgetMyMasternodes->setColumnWidth(2, columnProtocolWidth);
	ui->tableWidgetMyMasternodes->setColumnWidth(3, columnStatusWidth);
	ui->tableWidgetMyMasternodes->setColumnWidth(4, columnActiveWidth);
	ui->tableWidgetMyMasternodes->setColumnWidth(5, columnLastSeenWidth);

	ui->tableWidgetMasternodes->setColumnWidth(0, columnAddressWidth);
	ui->tableWidgetMasternodes->setColumnWidth(1, columnProtocolWidth);
	ui->tableWidgetMasternodes->setColumnWidth(2, columnStatusWidth);
	ui->tableWidgetMasternodes->setColumnWidth(3, columnActiveWidth);
	ui->tableWidgetMasternodes->setColumnWidth(4, columnLastSeenWidth);

	ui->tableWidgetMyMasternodes->setContextMenuPolicy(Qt::CustomContextMenu);

	QAction *startAliasAction = new QAction(tr("Start alias"), this);
	contextMenu = new QMenu();
	contextMenu->addAction(startAliasAction);
	connect(ui->tableWidgetMyMasternodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
	connect(startAliasAction, SIGNAL(triggered()), this,
			SLOT(on_startButton_clicked()));

	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
	connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
	timer->start(1000);

	fFilterUpdated = false;
	nTimeFilterUpdated = GetTime();
	updateNodeList();
}

MasternodeList::~MasternodeList() {
	delete ui;
}

void MasternodeList::setModel(WalletModel *model) {
	this->model = model;
}

void MasternodeList::showContextMenu(const QPoint &point) {
	QTableWidgetItem *item = ui->tableWidgetMyMasternodes->itemAt(point);
	if (item)
		contextMenu->exec(QCursor::pos());
}

void MasternodeList::StartAlias(std::string strAlias) {
	std::string strStatusHtml;
	strStatusHtml += "<center>Alias: " + strAlias;

	BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
		if (mne.getAlias() == strAlias) {
			std::string strError;
			CMasternodeBroadcast mnb;
//	    		bool fSuccess = activeMasternode.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError);
			bool fSuccess = CMasternodeBroadcast::Create(mne.getIp(),
					mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(),
					strError, mnb);

			if (fSuccess) {
				strStatusHtml += "<br>Successfully started masternode.";
				mnodeman.UpdateMasternodeList(mnb);
				mnb.Relay();
			} else {
				strStatusHtml += "<br>Failed to start masternode.<br>Error: "
						+ strError;
			}
			break;
		}
	}
	strStatusHtml += "</center>";

	QMessageBox msg;
	msg.setText(QString::fromStdString(strStatusHtml));
	msg.exec();

	updateMyNodeList(true);
}

void MasternodeList::RemoveAlias(std::string strAlias) {

	int linenumber = 1;
	boost::filesystem::path pathMasternodeConfigFile =
			GetMasternodeConfigFile();
	boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);

	if (!streamConfig.good()) {
		return;
	}

	std::string path_1 = pathMasternodeConfigFile.string() + "bak";
	FILE* configFile1 = fopen(path_1.c_str(), "a");
	if (configFile1 == NULL) {
		return;
	}

	for (std::string line; std::getline(streamConfig, line); linenumber++) {
		if (line.empty())
			continue;

		std::istringstream iss(line);
		std::string comment, alias, ip, privKey, txHash, outputIndex;

		if (iss >> comment) {
			if (comment.at(0) == '#') {
				line += "\n";
				fwrite(line.c_str(), std::strlen(line.c_str()), 1, configFile1);
				continue;
			}
			iss.str(line);
			iss.clear();
		}

		if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
			iss.str(line);
			iss.clear();
			if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
				streamConfig.close();
				return;
			}
		}
		if (alias == strAlias)
			continue;
		line += "\n";
		fwrite(line.c_str(), std::strlen(line.c_str()), 1, configFile1);
	}

	streamConfig.close();
	fclose(configFile1);

#ifdef WIN32
	std::string removeCommand = "del " + GetConfigFile().string();
	std::string renameCommand = "rename " + path_1 + " " + GetConfigFile().string();
#else
	std::string removeCommand = "rm -f " + GetConfigFile().string();
	std::string renameCommand = "mv -f " + path_1 + " "
			+ GetConfigFile().string();
#endif
	std::system(removeCommand.c_str());
	std::system(renameCommand.c_str());

	std::string strStatusHtml;
	strStatusHtml += "<center>Alias: " + strAlias;
	strStatusHtml +=
			"<br>Removed the masternode!<br>Please restart the wallet!";
	strStatusHtml += "</center>";

	QMessageBox msg;
	msg.setText(QString::fromStdString(strStatusHtml));
	msg.exec();

	StartShutdown();
}

void MasternodeList::StartAll(std::string strCommand) {
	int nCountSuccessful = 0;
	int nCountFailed = 0;
	std::string strFailedHtml;

	BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
		std::string strError;
		CMasternodeBroadcast mnb;

		int32_t nOutputIndex = 0;
		if (!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
			continue;
		}

		COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), nOutputIndex);

//    		bool fSuccess = activeMasternode.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError);
		bool fSuccess = CMasternodeBroadcast::Create(mne.getIp(),
				mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(),
				strError, mnb);

		if (fSuccess) {
			nCountSuccessful++;
			mnodeman.UpdateMasternodeList(mnb);
			mnb.Relay();
		} else {
			nCountFailed++;
			strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: "
					+ strError;
		}
	}
	pwalletMain->Lock();

	std::string returnObj;
	returnObj = strprintf("Successfully started %d masternodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
	if (nCountFailed > 0) {
		returnObj += strFailedHtml;
	}

	QMessageBox msg;
	msg.setText(QString::fromStdString(returnObj));
	msg.exec();

	updateMyNodeList(true);
}

std::string DurationToDHMS(int64_t nDurationTime) {
	int seconds = nDurationTime % 60;
	nDurationTime /= 60;
	int minutes = nDurationTime % 60;
	nDurationTime /= 60;
	int hours = nDurationTime % 24;
	int days = nDurationTime / 24;
	if (days)
		return strprintf("%dd %02dh:%02dm:%02ds", days, hours, minutes, seconds);
	if (hours)
		return strprintf("%02dh:%02dm:%02ds", hours, minutes, seconds);
	return strprintf("%02dm:%02ds", minutes, seconds);
}

void MasternodeList::updateMyMasternodeInfo(QString strAlias, QString strAddr,
		const COutPoint& outpoint) {
	bool fOldRowFound = false;
	int nNewRow = 0;
	int offsetFromUtc = GetOffsetFromUtc();

	for (int i = 0; i < ui->tableWidgetMyMasternodes->rowCount(); i++) {
		if (ui->tableWidgetMyMasternodes->item(i, 0)->text() == strAlias) {
			fOldRowFound = true;
			nNewRow = i;
			break;
		}
	}

	if (nNewRow == 0 && !fOldRowFound) {
		nNewRow = ui->tableWidgetMyMasternodes->rowCount();
		ui->tableWidgetMyMasternodes->insertRow(nNewRow);
	}

//    masternode_info_t infoMn;

	try {
		CMasternode* pmn = mnodeman.Find(outpoint);
		bool fFound = (pmn != NULL);

		QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
		QTableWidgetItem *addrItem = new QTableWidgetItem(strAddr);
		QTableWidgetItem *protocolItem = new QTableWidgetItem("70004");
		QTableWidgetItem *statusItem = new QTableWidgetItem(
				QString::fromStdString(fFound ? pmn->GetStatus() : "MISSING"));
		QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(
				QString::fromStdString(
						DurationToDHMS(
								fFound ?
										pmn->lastPing.sigTime - pmn->sigTime :
										0)));
		QTableWidgetItem *lastSeenItem = new QTableWidgetItem(
				QString::fromStdString(
						DateTimeStrFormat("%Y-%m-%d %H:%M",
								fFound ?
										pmn->lastPing.sigTime + offsetFromUtc :
										0)));
		QTableWidgetItem *pubkeyItem =
				new QTableWidgetItem(
						QString::fromStdString(
								fFound ?
										CBitcoinAddress(
												pmn->pubKeyCollateralAddress.GetID()).ToString() :
										""));

		/*  	QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(fFound ? pmn->GetStatus() : "MISSING"));
		 QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(fFound ? pmn->lastPing.sigTime - pmn->sigTime : 0)));
		 QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", fFound ? pmn->lastPing.sigTime + offsetFromUtc : 0)));
		 QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(fFound ? CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString() : 0));
		 */
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 0, aliasItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 1, addrItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 2, protocolItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 3, statusItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 4, activeSecondsItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 5, lastSeenItem);
		ui->tableWidgetMyMasternodes->setItem(nNewRow, 6, pubkeyItem);
	} catch (const std::exception& e) {
		throw runtime_error("Exception on my masternode status");
	}

}

void MasternodeList::updateMyNodeList(bool fForce) {
	TRY_LOCK(cs_mymnlist, fLockAcquired);
	if (!fLockAcquired) {
		return;
	}
	static int64_t nTimeMyListUpdated = 0;

	// automatically update my masternode list only once in MY_MASTERNODELIST_UPDATE_SECONDS seconds,
	// this update still can be triggered manually at any time via button click
	int64_t nSecondsTillUpdate = nTimeMyListUpdated
			+ MY_MASTERNODELIST_UPDATE_SECONDS - GetTime();
	ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

	if (nSecondsTillUpdate > 0 && !fForce)
		return;
	nTimeMyListUpdated = GetTime();

	ui->tableWidgetMasternodes->setSortingEnabled(false);
	BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
		int32_t nOutputIndex = 0;
		if (!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
			continue;
		}

		updateMyMasternodeInfo(QString::fromStdString(mne.getAlias()),
				QString::fromStdString(mne.getIp()),
				COutPoint(uint256S(mne.getTxHash()), nOutputIndex));
	}
	ui->tableWidgetMasternodes->setSortingEnabled(true);

	// reset "timer"
	ui->secondsLabel->setText("0");
}

void MasternodeList::updateNodeList() {
	TRY_LOCK(cs_mnlist, fLockAcquired);
	if (!fLockAcquired) {
		return;
	}

	static int64_t nTimeListUpdated = GetTime();

	// to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
	// or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
	int64_t nSecondsToWait =
			fFilterUpdated ?
					nTimeFilterUpdated
							- GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS
							:
					nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

	if (fFilterUpdated)
		ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
	if (nSecondsToWait > 0)
		return;

	nTimeListUpdated = GetTime();
	fFilterUpdated = false;

	QString strToFilter;
	ui->countLabel->setText("Updating...");
	ui->tableWidgetMasternodes->setSortingEnabled(false);
	ui->tableWidgetMasternodes->clearContents();
	ui->tableWidgetMasternodes->setRowCount(0);
	std::vector<CMasternode> vecMasternodes =
			mnodeman.GetFullMasternodeVector();
	int offsetFromUtc = GetOffsetFromUtc();

	BOOST_FOREACH (CMasternode& mnpair, vecMasternodes) {
		CMasternode mn = mnpair;
		// populate list
		// Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
		QTableWidgetItem *addressItem = new QTableWidgetItem(
				QString::fromStdString(mn.addr.ToString()));
		QTableWidgetItem *protocolItem = new QTableWidgetItem(
				QString::number(70004));
		QTableWidgetItem *statusItem = new QTableWidgetItem(
				QString::fromStdString(mn.GetStatus()));
		QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(
				QString::fromStdString(
						DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
		QTableWidgetItem *lastSeenItem = new QTableWidgetItem(
				QString::fromStdString(
						DateTimeStrFormat("%Y-%m-%d %H:%M",
								mn.lastPing.sigTime + offsetFromUtc)));
		QTableWidgetItem *pubkeyItem =
				new QTableWidgetItem(
						QString::fromStdString(
								CBitcoinAddress(
										mn.pubKeyCollateralAddress.GetID()).ToString()));

		if (strCurrentFilter != "") {
			strToFilter = addressItem->text() + " " + protocolItem->text() + " "
					+ statusItem->text() + " " + activeSecondsItem->text() + " "
					+ lastSeenItem->text() + " " + pubkeyItem->text();
			if (!strToFilter.contains(strCurrentFilter))
				continue;
		}

		ui->tableWidgetMasternodes->insertRow(0);
		ui->tableWidgetMasternodes->setItem(0, 0, addressItem);
		ui->tableWidgetMasternodes->setItem(0, 1, protocolItem);
		ui->tableWidgetMasternodes->setItem(0, 2, statusItem);
		ui->tableWidgetMasternodes->setItem(0, 3, activeSecondsItem);
		ui->tableWidgetMasternodes->setItem(0, 4, lastSeenItem);
		ui->tableWidgetMasternodes->setItem(0, 5, pubkeyItem);
	}

	ui->countLabel->setText(
			QString::number(ui->tableWidgetMasternodes->rowCount()));
	ui->tableWidgetMasternodes->setSortingEnabled(true);
}

void MasternodeList::on_filterLineEdit_textChanged(const QString &strFilterIn) {
	strCurrentFilter = strFilterIn;
	nTimeFilterUpdated = GetTime();
	fFilterUpdated = true;
	ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void MasternodeList::on_startButton_clicked() {
	std::string strAlias;
	{
		LOCK(cs_mymnlist);
		// Find selected node alias
		QItemSelectionModel* selectionModel =
				ui->tableWidgetMyMasternodes->selectionModel();
		QModelIndexList selected = selectionModel->selectedRows();

		if (selected.count() == 0)
			return;

		QModelIndex index = selected.at(0);
		int nSelectedRow = index.row();
		strAlias =
				ui->tableWidgetMyMasternodes->item(nSelectedRow, 0)->text().toStdString();
	}

	// Display message box
	QMessageBox::StandardButton retval = QMessageBox::question(this,
			tr("Confirm masternode start"),
			tr("Are you sure you want to start masternode %1?").arg(
					QString::fromStdString(strAlias)),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

	if (retval != QMessageBox::Yes)
		return;

	WalletModel::EncryptionStatus encStatus = model->getEncryptionStatus();

	StartAlias(strAlias);
}

void MasternodeList::on_startAllButton_clicked() {
	// Display message box
	QMessageBox::StandardButton retval = QMessageBox::question(this,
			tr("Confirm all masternodes start"),
			tr("Are you sure you want to start ALL masternodes?"),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

	if (retval != QMessageBox::Yes)
		return;

	WalletModel::EncryptionStatus encStatus = model->getEncryptionStatus();

	StartAll();
}

void MasternodeList::on_createMNButton_clicked() {
	CreateMasternodeDlg* crtMN = new CreateMasternodeDlg();
	crtMN->exec();
}

void MasternodeList::on_removeMNButton_clicked() {
	std::string strAlias;
	{
		LOCK(cs_mymnlist);
		// Find selected node alias
		QItemSelectionModel* selectionModel =
				ui->tableWidgetMyMasternodes->selectionModel();
		QModelIndexList selected = selectionModel->selectedRows();

		if (selected.count() == 0)
			return;

		QModelIndex index = selected.at(0);
		int nSelectedRow = index.row();
		strAlias =
				ui->tableWidgetMyMasternodes->item(nSelectedRow, 0)->text().toStdString();
	}

	// Display message box
	QMessageBox::StandardButton retval = QMessageBox::question(this,
			tr("Confirm masternode remove"),
			tr("Are you sure you want to remove masternode %1?").arg(
					QString::fromStdString(strAlias)),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

	if (retval != QMessageBox::Yes)
		return;

	WalletModel::EncryptionStatus encStatus = model->getEncryptionStatus();
	RemoveAlias(strAlias);

}

void MasternodeList::on_tableWidgetMyMasternodes_itemSelectionChanged() {
	if (ui->tableWidgetMyMasternodes->selectedItems().count() > 0) {
		ui->startButton->setEnabled(true);
		ui->removeMNButton->setEnabled(true);
	}
}

void MasternodeList::on_UpdateButton_clicked() {
	updateMyNodeList(true);
}
