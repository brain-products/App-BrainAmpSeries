#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <atomic>
#include <thread>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
using HANDLE = void *;
using ULONG = unsigned long;
#endif
#include "BrainAmpIoCtl.h"


struct ReaderConfig 
{
	int deviceNumber;
	enum Resolution : uint8_t { V_100nV = 0, V_500nV = 1, V_10microV = 2, V_152microV = 3 } resolution;
	bool dcCoupling, usePolyBox, useAuxChannels, lowImpedanceMode;
	int chunkSize, channelCount, serialNumber;
	std::vector<std::string> channelLabels;
	int useMRLowPass;
};



struct t_AppVersion
{
	int32_t Major;
	int32_t Minor;
	int32_t Bugfix;
};

namespace Ui {
class MainWindow;
}
class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent, const char *config_file);
	~MainWindow() noexcept override;

private slots:
	// close event (potentially disabled)
	void closeEvent(QCloseEvent *ev) override;
	// start the BrainAmpSeries connection
	void toggleRecording();
	void VersionsDialog();
	void UpdateChannelLabels();
	void UpdateChannelLabelsGUI(int);
	void setSamplingRate();
	void MRSettingsToggled(int);

private:
	// function for loading / saving the config file
	QString find_config_file(const char *filename);
	void CheckAmpTypeAgainstConfig(BA_SETUP* setup, USHORT* ampTypes, ReaderConfig config);
	void SetResolutions(BA_SETUP* setup, USHORT* ampTypes, uint8_t resolution, bool useAuxChannels);
	void SetDCCoupling(BA_SETUP* setup, USHORT* ampTypes, bool dcCoupling);
	void SetLowPass(BA_SETUP* setup, USHORT* ampTypes, bool useMRLowPass);
	bool IsAuxChannel(int c);
	// background data reader thread
	template <typename T>
	void read_thread(const ReaderConfig config);

	// raw config file IO
	void load_config(const QString &filename);
	void save_config(const QString &filename);
	std::unique_ptr<std::thread> reader{nullptr};
	HANDLE m_hDevice{nullptr};

	bool m_bUnsampledMarkers{false};
	bool m_bSampledMarkersEEG{false};
	bool m_bOverrideAutoUpdate;
	bool m_bPullUpHiBits;
	bool m_bPullUpLowBits;
	uint16_t m_nPullDir;
	t_AppVersion m_AppVersion;
	Ui::MainWindow *ui;
	std::atomic<bool> shutdown{false}; // flag indicating whether the recording thread should quit
	std::vector<int> m_vnAuxChannelMap;
};

#endif // MAINWINDOW_H
