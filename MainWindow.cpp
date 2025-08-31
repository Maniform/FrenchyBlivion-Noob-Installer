#include "MainWindow.h"
#include "./ui_MainWindow.h"

#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>

#include <QtConcurrent>

#define STEAM_OBLIVION_FOLDER "C:/Program Files (x86)/Steam/steamapps/common/Oblivion Remastered"
#define XBOX_OBLIVION_FOLDER "C:/XboxGames/The Elder Scrolls IV- Oblivion Remastered/Content"

#define PATCH_FOLDER "../OblivionRemastered"
#define PLUGINS_FILE "/OblivionRemastered/Content/Dev/ObvData/Data/Plugins.txt"
#define OBLIVION_BSA_SUBFOLDER "/OblivionRemastered/Content/Dev/ObvData/Data/"
#define OBLIVION_PAK_SUBFOLDER "/OblivionRemastered/Content/Paks/"
#define FRENCHYBLIVION_TEST_FILE "FrenchyBlivionVanilla.bsa"

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, settings("Manicorp", "FrenchyBlivion-Noob-Installer", this)
	, cursorPixmap(":/icon/AFCursor.png")
	, cursor(cursorPixmap, 13, 6)
	, plugins{}
{
    ui->setupUi(this);

	qApp->setOverrideCursor(cursor);

	if (!QDir(PATCH_FOLDER).exists())
	{
		QMessageBox::critical(this, tr("FrenchyBlivion Noob Installer"), tr("Le dossier du patch n'a pas été trouvé.\nMettez le dossier \"OblivionRemastered\" à côté de l'exécutable."), QMessageBox::Ok);
		exit(-1);
	}
	
	player.setSource(QUrl("qrc:/music/music.mp3"));
	player.setAudioOutput(&audioOutput);
	player.setLoops(QMediaPlayer::Infinite);
	audioOutput.setVolume(0.3f);
	player.play();

	patchFiles = discoverFiles(PATCH_FOLDER);
	discoverPlugins();

	ui->gameFolderLineEdit->setText(settings.value("gameFolder").toString());
	ui->progressBar->setVisible(false);
	ui->progressBar->setMaximum(patchFiles.size());

	if (settings.value("gameFolder").toString().isEmpty())
	{
		if (QDir(STEAM_OBLIVION_FOLDER).exists())
		{
			ui->gameFolderLineEdit->setText(STEAM_OBLIVION_FOLDER);
			settings.setValue("gameFolder", STEAM_OBLIVION_FOLDER);
			QMessageBox::information(this, tr("FrenchyBlivion Noob Installer"), tr("Le dossier du jeu a été trouvé automatiquement dans le dossier Steam. Si ce n'est pas le bon dossier, veuillez le changer manuellement."), QMessageBox::Ok);
		}
		else if (QDir(XBOX_OBLIVION_FOLDER).exists())
		{
			ui->gameFolderLineEdit->setText(XBOX_OBLIVION_FOLDER);
			settings.setValue("gameFolder", XBOX_OBLIVION_FOLDER);
			QMessageBox::information(this, tr("FrenchyBlivion Noob Installer"), tr("Le dossier du jeu a été trouvé automatiquement dans le dossier Xbox Game Pass. Si ce n'est pas le bon dossier, veuillez le changer manuellement."), QMessageBox::Ok);
		}
		else
		{
			QMessageBox::information(this, tr("FrenchyBlivion Noob Installer"), tr("Aucun dossier d'installation du jeu n'a été trouvé. Veuillez le saisir manuellement."), QMessageBox::Ok);
		}
	}

	connect(&patchFutureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::on_patchFutureFinished);
	connect(&unpatchFutureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::on_unpatchFutureFinished);
	connect(&patchFutureWatcher, &QFutureWatcher<void>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
	connect(&unpatchFutureWatcher, &QFutureWatcher<void>::progressValueChanged, ui->progressBar, &QProgressBar::setValue);
}

MainWindow::~MainWindow()
{
	patchFuture.cancel();
	patchFutureWatcher.cancel();
	unpatchFuture.cancel();
	unpatchFutureWatcher.cancel();

    delete ui;
}

QStringList MainWindow::discoverFiles(const QString& folderPath)
{
	QStringList files;
	QDirIterator it(folderPath, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		files << it.next();
	}

	return files;
}

void MainWindow::discoverPlugins()
{
	for (const QString& patchFile : patchFiles)
	{
		QFileInfo patchFileInfo(patchFile);
		QString suffix = patchFileInfo.suffix();
		if (patchFileInfo.isFile() && patchFileInfo.suffix() == "esp")
		{
			plugins.append(patchFileInfo.fileName());
		}
	}
}

void MainWindow::patch(const QString& filePath)
{
	const QString gameFolder = ui->gameFolderLineEdit->text();
	const QString inFile = "/" + filePath;
	QString outFile = gameFolder + filePath;
	outFile.remove("..");
	if (QFile(outFile).exists())
	{
		const QString oldFile = outFile + ".old";
		if (QFile(oldFile).exists())
		{
			QFile::remove(oldFile);
		}
		QFile::rename(outFile, oldFile);
	}
	QFile::copy(filePath, outFile);
}

void MainWindow::patchPluginFile(const QString& gameFolder)
{
	QFile pluginFile(gameFolder + PLUGINS_FILE);
	if (pluginFile.open(QFile::ReadOnly | QFile::Text))
	{
		QString pluginText = pluginFile.readAll();
		pluginFile.close();

		if (!pluginText.endsWith('\n'))
		{
			pluginText += '\n';
		}

		QStringList espFiles;
		QDirIterator it(gameFolder + OBLIVION_BSA_SUBFOLDER);
		while (it.hasNext())
		{
			const QString file = it.next();
			const QFileInfo fileInfo(file);
			if (fileInfo.suffix() == "esp")
			{
				espFiles << file;
			}
		}

		QStringList FrenchyBlivionEspFiles;
		for (const QString& espFile : espFiles)
		{
			const QString espFileName = QFileInfo(espFile).fileName();
			if (!pluginText.contains(espFileName))
			{
				if (espFileName.contains("FrenchyBlivion"))
				{
					FrenchyBlivionEspFiles << espFile;
				}
				else
				{
					pluginText += espFileName + "\n";
				}
			}
		}

		QMap<qint64, QStringList> espFilesBySize;
		for (const QString& FrenchyBlivionEspFile : FrenchyBlivionEspFiles)
		{
			QString FrenchyBlivionBsaFile = FrenchyBlivionEspFile;
			FrenchyBlivionBsaFile.replace(".esp", ".bsa");
			QFileInfo FrenchyBlivionBsaFileInfo(FrenchyBlivionBsaFile);
			espFilesBySize[FrenchyBlivionBsaFileInfo.size()].append(FrenchyBlivionEspFile);
		}

		QStringList espFilesSorted;
		for (const qint64 key : espFilesBySize.keys())
		{
			for (const QString& espFile : espFilesBySize[key])
			{
				const QString espFileName = QFileInfo(espFile).fileName();
				if (key == 0)
				{
					continue;
				}
				else
				{
					espFilesSorted.push_back(espFileName);
				}
			}
		}
		for (const QString& espFile : espFilesBySize[0])
		{
			const QString espFileName = QFileInfo(espFile).fileName();
			espFilesSorted.push_back(espFileName);
		}

		for (const QString& espFile : espFilesSorted)
		{
			pluginText.replace("AltarESPLocal.esp", "AltarESPLocal.esp\n" + espFile + "\n");
		}

		pluginText.replace("\n\n", "\n");
		if (pluginFile.open(QFile::WriteOnly | QFile::Text))
		{
			QTextStream ts(&pluginFile);
			ts << pluginText;
			pluginFile.close();
		}
	}
}

void MainWindow::unpatch(const QString& filePath)
{
	const QString gameFolder = ui->gameFolderLineEdit->text();
	QString outFile = gameFolder + filePath;
	outFile.remove("..");
	if (QFile(outFile).exists())
	{
		QFile::remove(outFile);
		const QString oldFile = outFile + ".old";
		if (QFile(oldFile).exists())
		{
			QFile::rename(oldFile, outFile);
		}
	}
}

void MainWindow::unpatchPluginFile(const QString& gameFolder)
{
	QString pluginText;
	QFile pluginFile(gameFolder + PLUGINS_FILE);
	if (pluginFile.open(QFile::ReadOnly | QFile::Text))
	{
		pluginText = pluginFile.readAll();
		pluginFile.close();
		for (const QString& plugin : plugins)
		{
			if (pluginText.contains(plugin))
			{
				pluginText.remove(plugin + "\n");
			}
		}
		pluginText.replace("\n\n", "\n");
		if (pluginFile.open(QFile::WriteOnly | QFile::Text))
		{
			QTextStream ts(&pluginFile);
			ts << pluginText;
			pluginFile.close();
		}
		else
		{
			QMessageBox::warning(this, tr("FrenchyBlivion Noob Installer"), tr("Impossible d'ouvrir le fichier Plugins.txt en écriture. Le patch n'a pas pu être appliqué."), QMessageBox::Ok);
			return;
		}
	}
	else
	{
		QMessageBox::warning(this, tr("FrenchyBlivion Noob Installer"), tr("Impossible d'ouvrir le fichier Plugins.txt en lecture. Le patch n'a pas pu être appliqué."), QMessageBox::Ok);
		return;
	}
}

void MainWindow::on_openFolderPushButton_clicked()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Dossier du jeu"), settings.value("gameFolder", QDir::homePath()).toString());
	if (folder.isEmpty())
	{
		return;
	}
	ui->gameFolderLineEdit->setText(folder);
	settings.setValue("gameFolder", folder);
}

void MainWindow::on_gameFolderLineEdit_textChanged(const QString& text)
{
	if (QDir(text).exists() && (QFile(text + "/OblivionRemastered.exe").exists() || QFile(text + "/OblivionRemastered/Binaries/WinGDK/OblivionRemastered-WinGDK-Shipping.exe").exists()))
	{
		const QString modTestFile = text + OBLIVION_BSA_SUBFOLDER + FRENCHYBLIVION_TEST_FILE;
		if (QFile(modTestFile).exists())
		{
			ui->patchPushButton->setEnabled(false);
			ui->patchPushButton->setVisible(false);
			ui->unpatchPushButton->setVisible(true);
			ui->unpatchPushButton->setEnabled(true);
		}
		else
		{
			ui->unpatchPushButton->setVisible(false);
			ui->unpatchPushButton->setEnabled(false);
			ui->patchPushButton->setEnabled(true);
			ui->patchPushButton->setVisible(true);
		}
	}
	else
	{
		ui->unpatchPushButton->setVisible(false);
		ui->unpatchPushButton->setEnabled(false);
		ui->patchPushButton->setEnabled(false);
		ui->patchPushButton->setVisible(true);
	}
}

void MainWindow::on_patchPushButton_clicked()
{
	ui->patchPushButton->setEnabled(false);
	ui->gameFolderLineEdit->setEnabled(false);
	ui->openFolderPushButton->setEnabled(false);
	ui->progressBar->setVisible(true);

	patchFuture = QtConcurrent::map(patchFiles,
		[this](const QString& filePath)
		{
			if (QFileInfo(filePath).isFile())
			{
				this->patch(filePath);
			}
		}
	);
	patchFutureWatcher.setFuture(patchFuture);
}

void MainWindow::on_patchFutureFinished()
{
	patchPluginFile(ui->gameFolderLineEdit->text());
	QMessageBox::information(this, tr("FrenchyBlivion Noob Installer"), tr("Le patch a été appliqué avec succès.\nYouhou !!"), QMessageBox::Ok);

	ui->patchPushButton->setVisible(false);
	ui->patchPushButton->setEnabled(false);
	ui->unpatchPushButton->setVisible(true);
	ui->unpatchPushButton->setEnabled(true);
	ui->gameFolderLineEdit->setEnabled(true);
	ui->openFolderPushButton->setEnabled(true);
	ui->progressBar->setVisible(false);
}

void MainWindow::on_unpatchPushButton_clicked()
{
	QMessageBox questionBox(QMessageBox::Question, tr("FrenchyBlivion Noob Installer"), tr("Êtes-vous SÛR de vouloir désinstaller notre super patch ? :("), QMessageBox::Yes | QMessageBox::No, this);
	questionBox.button(QMessageBox::Yes)->setText("Oui");
	questionBox.button(QMessageBox::No)->setText("Non");
	questionBox.exec();
	int result = questionBox.result();
	if (result == QMessageBox::Yes)
	{
		ui->unpatchPushButton->setEnabled(false);
		ui->gameFolderLineEdit->setEnabled(false);
		ui->openFolderPushButton->setEnabled(false);
		ui->progressBar->setVisible(true);

		unpatchFuture = QtConcurrent::map(patchFiles,
			[this](const QString& filePath)
			{
				this->unpatch(filePath);
			}
		);
		unpatchFutureWatcher.setFuture(unpatchFuture);

		unpatchPluginFile(ui->gameFolderLineEdit->text());
	}
}

void MainWindow::on_unpatchFutureFinished()
{
	QMessageBox::information(this, tr("FrenchyBlivion Noob Installer"), tr("Le patch a été désinstallé avec succès.\n:ccc"), QMessageBox::Ok);
	ui->unpatchPushButton->setVisible(false);
	ui->unpatchPushButton->setEnabled(false);
	ui->patchPushButton->setVisible(true);
	ui->patchPushButton->setEnabled(true);
	ui->gameFolderLineEdit->setEnabled(true);
	ui->openFolderPushButton->setEnabled(true);
	ui->progressBar->setVisible(false);
}

void MainWindow::on_actionMusic_toggled(bool enabled)
{
	if (enabled)
	{
		player.play();
	}
	else
	{
		player.pause();
	}
}

void MainWindow::on_actionCredits_triggered()
{
	QMessageBox::information(this, tr("Crédits"), tr("--- FrenchyBlivion Team ---\nAnthasix,  dB-, NulsRamm : Audios, & Recherche\nAlekso, Anthasix, dB-, NulsRamm, Shadows : Testeur, Contrôle qualité & Recherche\nManiform, Kaoxyd : Développeur\nSoftstar, Kapi : Modération\nNulsRamm, Yohan.exe, Alekso : Responsable du doublage\nRaspac : Assets Graphiques\nNulsRamm, Yohan.exe : Communications\n\n--- Musique ---\nChristopher Niskala - Oblivion 8-bit Chiptune Medley"));
}
