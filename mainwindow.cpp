#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Downsampler.h"
#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <chrono>
#include <lsl_cpp.h>
#include <iostream>
#include <sstream>




#ifdef WIN32
#include <winioctl.h>
#else
// dummy declarations to test compilation / static analysis on Linux/OS X
static HANDLE INVALID_HANDLE_VALUE = nullptr;
enum Dummy {
	FILE_DEVICE_UNKNOWN,
	NORMAL_PRIORITY_CLASS,
	METHOD_BUFFERED,
	FILE_WRITE_DATA,
	FILE_READ_DATA,
	GENERIC_READ,
	GENERIC_WRITE,
	FILE_ATTRIBUTE_NORMAL,
	FILE_FLAG_WRITE_THROUGH,
	OPEN_EXISTING,
	HIGH_PRIORITY_CLASS
};
inline int GetCurrentProcess() { return 0; }
inline int SetPriorityClass(int, int) { return 0; }
inline int CTL_CODE(int, int, int, int) { return 0; }
inline void CloseHandle(HANDLE) {}
inline bool DeviceIoControl(HANDLE, int, void *, int, void *, int, void *, void *) { return true; }
inline HANDLE CreateFileA(const char *, int, int, void *, int, int, void *) {
	return static_cast<void *>(&INVALID_HANDLE_VALUE);
}
inline int32_t GetLastError() { return 0; }
inline bool ReadFile(HANDLE, int16_t *, int, ulong *, void *) { return false; }
using DWORD = unsigned long;
using USHORT = uint16_t;
using ULONG = unsigned long;
using CHAR = signed char;
using UCHAR = unsigned char;
#endif

#include "BrainAmpIoCtl.h"

const int sampling_rates[] = { 5000, 2500, 1000, 500, 250, 200, 100 };
double sampling_rate = (double)sampling_rates[0];
const int downsampling_factors[] = { 1, 2, 5, 10, 20, 25, 50 };
int downsampling_factor = downsampling_factors[0];
static const char *error_messages[] = {"No error.", "Loss lock.", "Low power.",
	"Can't establish communication at start.", "Synchronisation error"};


#define LSLVERSIONSTREAM(version) (version/100) << "." << (version%100)
#define APPVERSIONSTREAM(version) version.Major << "." << version.Minor << "." << version.Bugfix

MainWindow::MainWindow(QWidget *parent, const char *config_file)
	: QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	m_AppVersion.Major = 1;
	m_AppVersion.Minor = 17;
	m_AppVersion.Bugfix = 0;
	m_bOverrideAutoUpdate = false;

	// make GUI connections
	connect(ui->actionLoad_Configuration, &QAction::triggered, [this]() {
		load_config(QFileDialog::getOpenFileName(
			this, "Load Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->actionSave_Configuration, &QAction::triggered, [this]() {
		save_config(QFileDialog::getSaveFileName(
			this, "Save Configuration File", "", "Configuration Files (*.cfg)"));
	});
	connect(ui->cbSamplingRate, SIGNAL(currentIndexChanged(int)), this, SLOT(setSamplingRate()));
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
	connect(ui->linkButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);
	QObject::connect(ui->actionVersions, SIGNAL(triggered()), this, SLOT(VersionsDialog()));
	QObject::connect(ui->channelCount, SIGNAL(valueChanged(int)), this, SLOT(UpdateChannelLabelsGUI(int)));
	QObject::connect(ui->cbUseMRSettings, SIGNAL(stateChanged(int)), this, SLOT(MRSettingsToggled(int)));
	for (int i = 0; i < 7; i++)
		ui->cbSamplingRate->addItem(QString::fromStdString(std::to_string(sampling_rates[i])));
	QString cfgfilepath = find_config_file(config_file);
	load_config(cfgfilepath);
}


void MainWindow::UpdateChannelLabelsGUI(int n)
{

	if (m_bOverrideAutoUpdate)return;
	UpdateChannelLabels();
}
void MainWindow::UpdateChannelLabels()
{
	if (!ui->overwriteChannelLabels->isChecked())return;
	int nEeg = ui->channelCount->value();
	std::string str;
	std::vector<std::string> psEEGChannelLabels;
	std::istringstream iss(ui->channelLabels->toPlainText().toStdString());
	while (std::getline(iss, str, '\n'))
		psEEGChannelLabels.push_back(str);
	while (psEEGChannelLabels.size() > ui->channelCount->value())
		psEEGChannelLabels.pop_back();
	ui->channelLabels->clear();
	for (int i = 0; i < ui->channelCount->value(); i++)
	{
		if (i < psEEGChannelLabels.size())
			str = psEEGChannelLabels[i];
		else
			str = std::to_string(i + 1);
		ui->channelLabels->appendPlainText(str.c_str());
	}



}
void MainWindow::VersionsDialog()
{


	int32_t lslProtocolVersion = lsl::protocol_version();
	int32_t lslLibVersion = lsl::library_version();
	std::stringstream ss;
	ss << "lsl protocol: " << LSLVERSIONSTREAM(lslProtocolVersion) << "\n" <<
		"liblsl: " << LSLVERSIONSTREAM(lslLibVersion) << "\n" <<
		"App: " << APPVERSIONSTREAM(m_AppVersion);
	QMessageBox::information(this, "Versions", ss.str().c_str(), QMessageBox::Ok);
}
void MainWindow::setSamplingRate()
{
	sampling_rate = sampling_rates[ui->cbSamplingRate->currentIndex()];
	downsampling_factor = downsampling_factors[ui->cbSamplingRate->currentIndex()];
}

void MainWindow::MRSettingsToggled(int)
{
	if (ui->cbUseMRSettings->isChecked())
	{
		ui->resolution->setCurrentIndex(1);
		ui->resolution->setDisabled(true);		
		ui->hwFilter->setCurrentIndex(1);
		ui->hwFilter->setDisabled(true);
		ui->cbSamplingRate->setCurrentIndex(0);
		ui->cbSamplingRate->setDisabled(true);
	}
	else
	{
		ui->resolution->setDisabled(false);
		ui->hwFilter->setDisabled(false);
		ui->cbSamplingRate->setDisabled(false);
	}
}

int getSamplingRateIndex(int nSamplingRate)
{
	switch (nSamplingRate)
	{
	case 5000:
		return 0;
	case 2500:
		return 1;
	case 1000:
		return 2;
	case 500:
		return 3;
	case 250:
		return 4;
	case 200:
		return 5;
	case 100:
		return 6;
	default:
		break;
	}
	return 0;
}

void MainWindow::load_config(const QString &filename) {
	QSettings pt(filename, QSettings::IniFormat);

	ui->channelCount->setValue(pt.value("settings/channelcount", 32).toInt());
	ui->impedanceMode->setCurrentIndex(pt.value("settings/impedancemode", 0).toInt());
	ui->cbSamplingRate->setCurrentIndex(getSamplingRateIndex(pt.value("settings/samplingrate", 500).toInt()));
	setSamplingRate();
	ui->cbUseMRSettings->setChecked(pt.value("settings/usemr", false).toBool());
	ui->resolution->setCurrentIndex(pt.value("settings/resolution", 0).toInt());
	ui->dcCoupling->setCurrentIndex(pt.value("settings/dccoupling", 0).toInt());
	ui->hwFilter->setCurrentIndex(pt.value("settings/hardwarefilter", 0).toInt());
	ui->chunkSize->setValue(pt.value("settings/chunksize", 32).toInt());
	ui->usePolyBox->setChecked(pt.value("settings/usepolybox", false).toBool());
	ui->useAuxChannels->setChecked(pt.value("settings/useauxchannels", false).toBool());
	//ui->sendRawStream->setChecked(pt.value("settings/sendrawstream", false).toBool());
	ui->unsampledMarkers->setChecked(pt.value("settings/unsampledmarkers", false).toBool());
	ui->sampledMarkersEEG->setChecked(pt.value("settings/sampledmarkersEEG", false).toBool());
	ui->channelLabels->setPlainText(pt.value("channels/labels").toStringList().join('\n'));
}

void MainWindow::save_config(const QString &filename) {
	QSettings pt(filename, QSettings::IniFormat);

	// transfer UI content into property tree
	pt.beginGroup("settings");
	pt.setValue("channelcount", ui->channelCount->value());
	pt.setValue("usemr", ui->cbUseMRSettings->isChecked());
	pt.setValue("samplingrate", ui->cbSamplingRate->currentText());
	pt.setValue("impedancemode", ui->impedanceMode->currentIndex());
	pt.setValue("resolution", ui->resolution->currentIndex());
	pt.setValue("dccoupling", ui->dcCoupling->currentIndex());
	pt.setValue("hardwarefilter", ui->hwFilter->currentIndex());
	pt.setValue("chunksize", ui->chunkSize->value());
	pt.setValue("usepolybox", ui->usePolyBox->isChecked());
	pt.setValue("useauxchannels", ui->useAuxChannels->isChecked());
	//pt.setValue("sendrawstream", ui->sendRawStream->isChecked());
	pt.setValue("unsampledmarkers", ui->unsampledMarkers->isChecked());
	pt.setValue("sampledmarkersEEG", ui->sampledMarkersEEG->isChecked());
	pt.endGroup();

	pt.beginGroup("channels");
	pt.setValue("labels", ui->channelLabels->toPlainText().split('\n'));
	pt.endGroup();
}

void MainWindow::closeEvent(QCloseEvent *ev) {
	if (reader) {
		QMessageBox::warning(this, "Recording still running", "Can't quit while recording");
		ev->ignore();
	}
}

void MainWindow::CheckAmpTypeAgainstConfig(BA_SETUP* setup, USHORT* ampTypes, ReaderConfig conf)
{
	int nChannels = 0;
	bool bHasMR, bHasExG16, bHasPoly, bHasDC = false;
	for (int i = 0; i < 4; i++)
	{
		switch (ampTypes[i])
		{
		case 0:
			break;
		case 1:
			nChannels += 32;
			break;
		case 2:
			nChannels += 32;
			bHasMR = true;
			break;
		case 3:
			nChannels += 32;
			bHasDC = true;
			break;
		case 4:
			nChannels += 8;
			break;
		case 5:
			nChannels += 16;
			bHasExG16 = true;
			break;
		case 6:
			bHasPoly = true;
			break;
		}
	}
	if (nChannels != conf.channelCount)
		throw std::runtime_error(std::string("Channel count does not match available channels on device: " + nChannels));
	if (!bHasDC)
	{
		if (conf.dcCoupling)
			throw std::runtime_error("DC Coupling not supported on this device.");
		if(conf.lowImpedanceMode)
			throw std::runtime_error("Low Impedance Mode not supported on this device.");
	}
	if (bHasDC || bHasMR)
	{
		if (conf.resolution == 0)
			throw std::runtime_error("100 nanoVolt Resolution not supported by this device.");
	}
	if (!bHasPoly)
		if (conf.usePolyBox)
			throw std::runtime_error("No polybox found. Please check device or uncheck Use PolyBox option.");

	// set resolutions based on amp types and settings
	SetResolutions(setup, ampTypes, conf.resolution, conf.useAuxChannels);
	// set dc coupling based on amp types and settings
	SetDCCoupling(setup, ampTypes, conf.dcCoupling);
	// set mr lowpass based on amp types and settings
	SetLowPass(setup, ampTypes, conf.useMRLowPass);
	setup->nLowImpedance = conf.lowImpedanceMode;
}

void MainWindow::SetResolutions(BA_SETUP* setup, USHORT* ampTypes, uint8_t resolution, bool useAuxChannels)
{
	UCHAR c = 0;
	for (int j = 0; j < 4; j++)
	{
		switch (ampTypes[j])
		{
		case 0:
			break;
		case 1:
		case 2:
		case 3:
			for (int i = 0; i < 32; i++)
				setup->nResolution[c++] = resolution;
			break;
		case 4:
			for (int i = 0; i < 8; i++)
				setup->nResolution[c++] = resolution;
			break;
		case 5:
			for (int i = 0; i < 16; i++)
				setup->nResolution[c++] = (useAuxChannels && i>7)? 123 : resolution;
			break;
		case 6:
			break;
		}
	}
}

void MainWindow::SetDCCoupling(BA_SETUP* setup, USHORT* ampTypes, bool dcCoupling)
{
	UCHAR c = 0;
	for (int j = 0; j < 4; j++)
	{
		switch (ampTypes[j])
		{
		case 0:
			break;
		case 1:
		case 2:
			for (int i = 0; i < 32; i++)
				setup->nDCCoupling[c++] = false;
			break;
		case 3:
			for (int i = 0; i < 32; i++)
				setup->nDCCoupling[c++] = dcCoupling;
			break;
		case 4:
			for (int i = 0; i < 8; i++)
				setup->nDCCoupling[c++] = false;
			break;
		case 5:
			for (int i = 0; i < 16; i++)
				setup->nDCCoupling[c++] = false;
			break;
		case 6:
			break;
		}
	}
}

void MainWindow::SetLowPass(BA_SETUP* setup, USHORT* ampTypes, bool useMRLowPass)
{
	UCHAR c = 0;
	for (int j = 0; j < 4; j++)
	{
		switch (ampTypes[j])
		{
		case 0:
			break;
		case 1:
			for (int i = 0; i < 32; i++)
				setup->n250Hertz[c] = false;
			break;
		case 2:
		case 3:
			for (int i = 0; i < 32; i++)
				setup->n250Hertz[c] = useMRLowPass;
			break;
		case 4:
			for (int i = 0; i < 8; i++)
				setup->nDCCoupling[c++] = false;
			break;
		case 5:
			for (int i = 0; i < 16; i++)
				setup->nDCCoupling[c++] = false;
			break;
		case 6:
			break;
		}
	}
}

// start/stop the BrainAmpSeries connection
void MainWindow::toggleRecording() {
	DWORD bytes_returned;
	if (reader) {
		// === perform unlink action ===
		try {
			shutdown = true;
			reader->join();
			reader.reset();
			SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
			if (m_hDevice != nullptr) {
				DeviceIoControl(
					m_hDevice, IOCTL_BA_STOP, nullptr, 0, nullptr, 0, &bytes_returned, nullptr);
				CloseHandle(m_hDevice);
				m_hDevice = nullptr;
			}
		} catch (std::exception &e) {
			QMessageBox::critical(this, "Error",
				QString("Could not stop the background processing: ") + e.what(), QMessageBox::Ok);
			return;
		}

		// indicate that we are now successfully unlinked
		ui->linkButton->setText("Link");
		ui->deviceSettingsGroup->setEnabled(true);
		ui->triggerSettingsGroup->setEnabled(true);
		ui->channelLabelsGroup->setEnabled(true);
	} else {
		// === perform link action ===

		try {
			// get the UI parameters...
			setSamplingRate();
			ReaderConfig conf;
			conf.channelCount = static_cast<unsigned int>(ui->channelCount->value());
			conf.lowImpedanceMode = ui->impedanceMode->currentIndex() == 1;
			conf.resolution = static_cast<ReaderConfig::Resolution>(ui->resolution->currentIndex());
			conf.dcCoupling = static_cast<unsigned char>(ui->dcCoupling->currentIndex());
			conf.chunkSize = ui->chunkSize->value();
			conf.usePolyBox = ui->usePolyBox->checkState() == Qt::Checked;
			conf.useAuxChannels = ui->useAuxChannels->checkState() == Qt::Checked;
			conf.useMRLowPass = ui->hwFilter->currentIndex();; // default, check later if it is brai
			//bool sendRawStream = ui->sendRawStream->isChecked();

			m_bUnsampledMarkers = ui->unsampledMarkers->checkState() == Qt::Checked;
			
			m_bSampledMarkersEEG = ui->sampledMarkersEEG->checkState() == Qt::Checked;

			for (auto &label : ui->channelLabels->toPlainText().split('\n'))
				conf.channelLabels.push_back(label.toStdString());
			if (conf.channelLabels.size() != conf.channelCount)
				throw std::runtime_error("The number of channels labels does not match the channel "
										 "count device setting.");

			// try to open the device
			std::string deviceName = R"(\\.\BrainAmpUSB)" + std::to_string(conf.deviceNumber);
			m_hDevice = CreateFileA(deviceName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
			if (m_hDevice == INVALID_HANDLE_VALUE)
				throw std::runtime_error(
					"Could not open USB device. Please make sure that the device is plugged in, "
					"turned on, and that the driver is installed correctly.");


			// get serial number
			ULONG serialNumber = 0;
			if (!DeviceIoControl(m_hDevice, IOCTL_BA_GET_SERIALNUMBER, nullptr, 0, &serialNumber,
					sizeof(serialNumber), &bytes_returned, nullptr))
				throw std::runtime_error("Could not get device serial number.");
			// why is this always 0?
			conf.serialNumber = (int)serialNumber;
			// set up device parameters
			BA_SETUP setup = {0};
			setup.nChannels = conf.channelCount;
			for (unsigned char c = 0; c < conf.channelCount; c++)
				setup.nChannelList[c] = c + (conf.usePolyBox ? -8 : 0);
			setup.nPoints = conf.chunkSize * downsampling_factor;
			setup.nHoldValue = 0;

			USHORT amp_types[4];
			if (!DeviceIoControl(m_hDevice, IOCTL_BA_AMPLIFIER_TYPE, nullptr, 0, amp_types,
				sizeof(amp_types), &bytes_returned, nullptr))
				throw std::runtime_error("Could not get amplifier type.");
			
			CheckAmpTypeAgainstConfig(&setup, amp_types, conf);

			m_bPullUpHiBits = true;
			m_bPullUpLowBits = false;
			m_nPullDir = (m_bPullUpLowBits ? 0xff : 0) | (m_bPullUpHiBits ? 0xff00 : 0);
			if (!DeviceIoControl(m_hDevice, IOCTL_BA_DIGITALINPUT_PULL_UP, &m_nPullDir,
					sizeof(m_nPullDir), nullptr, 0, &bytes_returned, nullptr))
				throw std::runtime_error("Could not apply pull up/down parameter.");

			if (!DeviceIoControl(m_hDevice, IOCTL_BA_SETUP, &setup, sizeof(setup), nullptr, 0,
					&bytes_returned, nullptr))
				throw std::runtime_error("Could not apply device setup parameters.");

			// start recording
			long acquire_eeg = 1;
			if (!DeviceIoControl(m_hDevice, IOCTL_BA_START, &acquire_eeg, sizeof(acquire_eeg),
					nullptr, 0, &bytes_returned, nullptr))
				throw std::runtime_error("Could not start recording.");

			// start reader thread
			shutdown = false;
			auto function_handle = &MainWindow::read_thread<float>;
				//sendRawStream ? &MainWindow::read_thread<int16_t> : &MainWindow::read_thread<float>;
			reader.reset(new std::thread(function_handle, this, conf));
		}

		catch (std::exception &e) {
			// try to decode the error message
			const char *msg = "Could not open USB device.";
			if (m_hDevice != nullptr) {
				long error_code = 0;
				if (DeviceIoControl(m_hDevice, IOCTL_BA_ERROR_STATE, nullptr, 0, &error_code,
						sizeof(error_code), &bytes_returned, nullptr) &&
					bytes_returned)
					msg = ((error_code & 0xFFFF) >= 0 && (error_code & 0xFFFF) <= 4)
							  ? error_messages[error_code & 0xFFFF]
							  : "Unknown error (your driver version might not yet be supported).";
				else
					msg = "Could not retrieve error message because the device is closed";
				CloseHandle(m_hDevice);
				m_hDevice = nullptr;
			}
			QMessageBox::critical(this, "Error",
				QString("Could not initialize the BrainAmpSeries interface: ") + e.what() +
					" (driver message: " + msg + ")",
				QMessageBox::Ok);
			return;
		}

		// done, all successful
		ui->linkButton->setText("Unlink");
		ui->deviceSettingsGroup->setEnabled(false);
		ui->triggerSettingsGroup->setEnabled(false);
		ui->channelLabelsGroup->setEnabled(false);

	}
}

// background data reader thread
template <typename T> void MainWindow::read_thread(const ReaderConfig conf) {
	const float unit_scales[] = {0.1f, 0.5f};
	const char *unit_strings[] = {"100 nV", "500 nV"};
	const bool sendRawStream = std::is_same<T, int16_t>::value;
	// reserve buffers to receive and send data
	unsigned int chunk_words = conf.chunkSize * (conf.channelCount + 1) * downsampling_factor;
	std::vector<int16_t> recv_buffer(chunk_words, 0);
	int sz = sizeof(int16_t);
	int nTransferSz = sz * (int)recv_buffer.size();
	unsigned int outbufferChannelCount = conf.channelCount +(m_bSampledMarkersEEG ? 1 : 0);
	std::vector<std::vector<T>> send_buffer_vec(conf.chunkSize, std::vector<T>(outbufferChannelCount));
	std::vector<T> sample_buffer(outbufferChannelCount, 0);
	std::vector<T> send_buffer(conf.chunkSize * outbufferChannelCount, 0);
	std::vector<T> inter_buffer(conf.chunkSize * downsampling_factor, 0);
	std::vector<Downsampler<T>> downsamplers;
	bool bDoFiltering = (sampling_rate == 5000) ? false : true;
	for (int i = 0; i < conf.channelCount; i++)
		downsamplers.push_back(Downsampler<T>(downsampling_factor, conf.chunkSize, bDoFiltering));
	//downsamplers.push_back(Downsampler<T>(downsampling_factor, conf.chunkSize, false));
	std::vector<std::string> marker_buffer(conf.chunkSize, std::string());
	std::string s_mrkr;

	const std::string streamprefix = "BrainAmpSeries-Dev_" + std::to_string(conf.deviceNumber);

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// for keeping track of sampled marker stream data
	uint16_t mrkr = 0;
	uint16_t prev_mrkr = 0;
	uint16_t outside_prev_mrkr = 0;

	// for keeping track of unsampled markers
	// uint16_t us_prev_mrkr = 0;

	std::unique_ptr<lsl::stream_outlet> marker_outlet;
	try {
		// create data streaminfo and append some meta-data
		auto stream_format = sendRawStream ? lsl::cf_int16 : lsl::cf_float32;
		lsl::stream_info data_info(streamprefix, "EEG", outbufferChannelCount, sampling_rate,
			stream_format, streamprefix /*+ '_' + std::to_string(conf.serialNumber)*/ + "_SR-" + std::to_string(sampling_rate));
		lsl::xml_element channels = data_info.desc().append_child("channels");
		std::string postprocessing_factor =
			sendRawStream ? std::to_string(unit_scales[conf.resolution]) : "1";
		for (const auto &channelLabel : conf.channelLabels)
			channels.append_child("channel")
				.append_child_value("label", channelLabel)
				.append_child_value("type", "EEG")
				.append_child_value("unit", "microvolts")
				.append_child_value("scaling_factor", postprocessing_factor);
		if (m_bSampledMarkersEEG) {
			channels.append_child("channel")
				.append_child_value("label", "triggerStream")
				.append_child_value("type", "EEG")
				.append_child_value("unit", "code");
		}

		data_info.desc()
			.append_child("amplifier")
			.append_child("settings")
			.append_child_value("low_impedance_mode", conf.lowImpedanceMode ? "true" : "false")
			.append_child_value("resolution", unit_strings[conf.resolution])
			.append_child_value("resolutionfactor", std::to_string(unit_scales[conf.resolution]))
			.append_child_value("dc_coupling", conf.dcCoupling ? "DC" : "AC");
		data_info.desc()
			.append_child("acquisition")
			.append_child_value("manufacturer", "Brain Products")
			.append_child_value("serial_number", std::to_string(conf.serialNumber));

		int32_t lslProtocolVersion = lsl::protocol_version();
		int32_t lslLibVersion = lsl::library_version();
		std::stringstream ssProt;
		ssProt << LSLVERSIONSTREAM(lslProtocolVersion);
		std::stringstream ssLSL;
		ssLSL << LSLVERSIONSTREAM(lslLibVersion);
		std::stringstream ssApp;
		ssApp << APPVERSIONSTREAM(m_AppVersion);

		data_info.desc().append_child("versions")
			.append_child_value("lsl_protocol", ssProt.str())
			.append_child_value("liblsl", ssLSL.str())
			.append_child_value("App", ssApp.str());
		// make a data outlet
		lsl::stream_outlet data_outlet(data_info);

		//// create marker streaminfo and outlet
		// create unsampled marker streaminfo and outlet

		if (m_bUnsampledMarkers) {
			lsl::stream_info marker_info(streamprefix + "-Markers", "Markers", 1, 0, lsl::cf_string,
				streamprefix /*+ '_' + std::to_string(conf.serialNumber)*/ + "_markers");
			marker_outlet.reset(new lsl::stream_outlet(marker_info));
		}  

		// enter transmission loop
		DWORD bytes_read;
		const T scale = std::is_same<T, float>::value ? unit_scales[conf.resolution] : 1;

		while (!shutdown) {
			// read chunk into recv_buffer
			if (!ReadFile(m_hDevice, &recv_buffer[0], (int)2 * chunk_words, &bytes_read, nullptr))
				throw std::runtime_error(
					"Could not read data, error code " + std::to_string(GetLastError()));

			if (bytes_read <= 0) {
				// CPU saver, this is ok even at higher sampling rates
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			if (bytes_read != 2 * chunk_words) {
				// check for errors
				long error_code = 0;
				if (DeviceIoControl(m_hDevice, IOCTL_BA_ERROR_STATE, nullptr, 0, &error_code,
						sizeof(error_code), &bytes_read, nullptr) &&
					error_code)
					throw std::runtime_error(
						((error_code & 0xFFFF) >= 0 && (error_code & 0xFFFF) <= 4)
							? error_messages[error_code & 0xFFFF]
							: "Unknown error (your driver version might not yet be supported).");
				std::this_thread::yield();
				continue;
			}

			// All checks completed, transform and send the data
			double now = lsl::local_clock();

			auto recvbuf_it = recv_buffer.cbegin();
			auto sendbuf_it = send_buffer.begin();
			auto inter_it = inter_buffer.begin();

			//for (int c = 0; c < conf.channelCount + 1; c++)
			for (int c = 0; c < conf.channelCount; c++)
			{
				inter_it = inter_buffer.begin();
				for (int s = 0; s < conf.chunkSize*downsampling_factor; s++)
					// +1 to include trigger data, which doesn't get downsampled or filtered
					*inter_it++ = *(recvbuf_it + (c + s * (conf.channelCount + 1)));
				downsamplers[c].Downsample(&inter_buffer[0]);
			}

			for (int c = 0; c < conf.channelCount; c++)
				for (int s = 0; s < conf.chunkSize; s++)
					send_buffer_vec[s][c] = downsamplers[c].m_ptDataOut[s] * scale;


			//for (int s = 0; s < conf.chunkSize; s++)
			int nOutBufferSampleCtr = 0;
			for (int s = 0; s < conf.chunkSize*downsampling_factor; s++)
			{
				//mrkr = (uint16_t)downsamplers[conf.channelCount].m_ptDataOut[s];
				mrkr = (uint16_t)recv_buffer[conf.channelCount + s * (conf.channelCount + 1)];
				mrkr ^= m_nPullDir;

				if (m_bSampledMarkersEEG)
					// if (sampling_rate != 5000)
						// send_buffer_vec[nOutBufferSampleCtr][conf.channelCount] = static_cast<T>(mrkr);// ((mrkr == prev_mrkr) ? -1 : static_cast<T>(mrkr));
					// else
					send_buffer_vec[s][conf.channelCount] = ((mrkr == prev_mrkr) ? -1 : static_cast<T>(mrkr));
					

				if (m_bUnsampledMarkers)
				{
					if (mrkr != prev_mrkr)
					{
						s_mrkr = std::to_string((int)mrkr);
						int num = 1;
						if(sampling_rate != 5000)
							num = nOutBufferSampleCtr + 1 - conf.chunkSize;
						else
							num = s + 1 - conf.chunkSize;
						double dNum = (double)num;
						double ts = dNum / sampling_rate;
						marker_outlet->push_sample(&s_mrkr, now + ts);
					}
				}
				prev_mrkr = mrkr;
				if((s%downsampling_factor)==0 && s!=0)
					nOutBufferSampleCtr++;
			}

			// push data chunk into the outlet
			data_outlet.push_chunk(send_buffer_vec, now);
		}
	} catch (std::exception &e) {
		// any other error
		std::cout << "Exception in read thread: " << e.what();
	}
}

/**
 * Find a config file to load. This is (in descending order or preference):
 * - a file supplied on the command line
 * - [executablename].cfg in one the the following folders:
 *	- the current working directory
 *	- the default config folder, e.g. '~/Library/Preferences' on OS X
 *	- the executable folder
 * @param filename	Optional file name supplied e.g. as command line parameter
 * @return Path to a found config file
 */
QString MainWindow::find_config_file(const char *filename) {
	if (filename) {
		QString qfilename(filename);
		if (!QFileInfo::exists(qfilename))
			QMessageBox(QMessageBox::Warning, "Config file not found",
				QStringLiteral("The file '%1' doesn't exist").arg(qfilename), QMessageBox::Ok,
				this);
		else
			return qfilename;
	}
	QFileInfo exeInfo(QCoreApplication::applicationFilePath());
	QString defaultCfgFilename(exeInfo.completeBaseName() + ".cfg");
	QStringList cfgpaths;
	cfgpaths << QDir::currentPath()
			 << QStandardPaths::standardLocations(QStandardPaths::ConfigLocation) << exeInfo.path();
	for (auto path : cfgpaths) {
		QString cfgfilepath = path + QDir::separator() + defaultCfgFilename;
		if (QFileInfo::exists(cfgfilepath)) return cfgfilepath;
	}
	QMessageBox(QMessageBox::Warning, "No config file not found",
		QStringLiteral("No default config file could be found"), QMessageBox::Ok, this);
	return "";
}

MainWindow::~MainWindow() noexcept { delete ui; }
