#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QCursor>
#include <QFuture>
#include <QFutureWatcher>
#include <QPixmap>
#include <QSettings>

#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private:
	Ui::MainWindow *ui;

	QSettings settings;

	QAudioOutput audioOutput;
	QMediaPlayer player;

	QPixmap cursorPixmap;
	QCursor cursor;

	QStringList discoverFiles(const QString& folderPath);
	void discoverPlugins();

	QFuture<void> patchFuture;
	QFutureWatcher<void> patchFutureWatcher;
	void patch(const QString& filePath);
	void patchPluginFile(const QString& gameFolder);
	QFuture<void> unpatchFuture;
	QFutureWatcher<void> unpatchFutureWatcher;
	void unpatch(const QString& filePath);
	void unpatchPluginFile(const QString& gameFolder);

	QStringList plugins;
	QStringList patchFiles;

private slots:
	void on_openFolderPushButton_clicked();
	void on_gameFolderLineEdit_textChanged(const QString& text);

	void on_patchPushButton_clicked();
	void on_patchFutureFinished();

	void on_unpatchPushButton_clicked();
	void on_unpatchFutureFinished();

	void on_actionMusic_toggled(bool enabled);
	void on_actionCredits_triggered();
};
#endif // MAINWINDOW_H
