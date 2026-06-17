#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <obs-frontend-api.h>
#include <obs-module.h>

namespace {

constexpr const char *kDockId = "obs-audio-recorder.dock";

QString moduleSettingsPath()
{
	char *path = obs_module_config_path("settings.ini");
	if (!path)
		return {};

	QString value = QString::fromUtf8(path);
	bfree(path);
	return value;
}

QString defaultOutputDirectory()
{
	const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
	if (!music.isEmpty())
		return QDir(music).filePath("OBS Audio Recorder");

	return QDir::home().filePath("OBS Audio Recorder");
}

QString sanitizeFileComponent(QString value)
{
	for (QChar &ch : value) {
		if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
			continue;
		ch = QLatin1Char('_');
	}

	value = value.trimmed();
	return value.isEmpty() ? QStringLiteral("audio") : value;
}

QString currentRecordingBaseName()
{
	return sanitizeFileComponent(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
}

int16_t floatToPcm16(float sample)
{
	sample = std::clamp(sample, -1.0f, 1.0f);
	return static_cast<int16_t>(std::lrintf(sample * 32767.0f));
}

struct WavHeader {
	char riff[4] = {'R', 'I', 'F', 'F'};
	uint32_t riff_size = 0;
	char wave[4] = {'W', 'A', 'V', 'E'};
	char fmt[4] = {'f', 'm', 't', ' '};
	uint32_t fmt_size = 16;
	uint16_t audio_format = 1;
	uint16_t channels = 0;
	uint32_t sample_rate = 0;
	uint32_t byte_rate = 0;
	uint16_t block_align = 0;
	uint16_t bits_per_sample = 16;
	char data[4] = {'d', 'a', 't', 'a'};
	uint32_t data_size = 0;
};

class AudioRecorderSession {
public:
	~AudioRecorderSession()
	{
		stop();
	}

	bool isRecording() const
	{
		return recording_.load();
	}

	QStringList outputPaths() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return output_paths_;
	}

	QString start(int mix_mask, bool combine_selected, const QString &output_dir, QString *error)
	{
		stop();

		if (mix_mask <= 0 || mix_mask > 0x3f) {
			if (error)
				*error = QStringLiteral("Select at least one audio channel.");
			return {};
		}

		struct obs_audio_info2 audio_info2 {};
		uint32_t sample_rate = 48000;
		enum speaker_layout speakers = SPEAKERS_STEREO;
		if (obs_get_audio_info2(&audio_info2)) {
			sample_rate = audio_info2.samples_per_sec;
			speakers = audio_info2.speakers;
		} else {
			struct obs_audio_info audio_info {};
			if (obs_get_audio_info(&audio_info)) {
				sample_rate = audio_info.samples_per_sec;
				speakers = audio_info.speakers;
			}
		}

		const uint32_t channels = std::max<uint32_t>(1, get_audio_channels(speakers));
		const struct audio_convert_info conversion = {
			.samples_per_sec = sample_rate,
			.format = AUDIO_FORMAT_FLOAT,
			.speakers = speakers,
			.allow_clipping = false,
		};

		const QString base_name = currentRecordingBaseName();
		const QString final_dir = output_dir.isEmpty() ? defaultOutputDirectory() : output_dir;
		const std::filesystem::path fs_dir = std::filesystem::u8path(final_dir.toUtf8().constData());
		std::error_code fs_error;
		if (std::filesystem::exists(fs_dir, fs_error)) {
			if (!std::filesystem::is_directory(fs_dir, fs_error)) {
				if (error)
					*error = QStringLiteral("The output path exists, but it is not a directory.");
				return {};
			}
		} else if (!std::filesystem::create_directories(fs_dir, fs_error)) {
			if (error)
				*error = QStringLiteral("Unable to create the output directory.");
			return {};
		}

		QDir dir(final_dir);

		std::array<int, 6> selected{};
		size_t selected_count = 0;
		for (int i = 0; i < 6; ++i) {
			if ((mix_mask & (1 << i)) != 0)
				selected[selected_count++] = i;
		}

		{
			std::lock_guard<std::mutex> lock(mutex_);
			mix_mask_ = mix_mask;
			combine_selected_ = combine_selected;
			output_paths_.clear();
			recording_.store(true);
		}

		if (combine_selected) {
			if (!combined_.start(mix_mask, selected_count, channels, sample_rate, dir, base_name, error)) {
				stop();
				return {};
			}

			for (size_t i = 0; i < selected_count; ++i)
				obs_add_raw_audio_callback(static_cast<size_t>(selected[i]), &conversion,
							   &AudioRecorderSession::rawAudioCaptureThunk, this);

			{
				std::lock_guard<std::mutex> lock(mutex_);
				output_paths_.clear();
				output_paths_ << combined_.outputPath();
			}
		} else {
			for (size_t i = 0; i < selected_count; ++i) {
				const int mix_index = selected[i];
				if (!channels_[static_cast<size_t>(mix_index)].start(mix_index, channels, sample_rate, dir, base_name, error)) {
					stop();
					return {};
				}

				obs_add_raw_audio_callback(static_cast<size_t>(mix_index), &conversion,
							   &AudioRecorderSession::rawAudioCaptureThunk, this);
			}

			{
				std::lock_guard<std::mutex> lock(mutex_);
				output_paths_.clear();
				for (size_t i = 0; i < selected_count; ++i)
					output_paths_ << channels_[static_cast<size_t>(selected[i])].outputPath();
			}
		}

		return outputPaths().join(QStringLiteral(", "));
	}

	void stop()
	{
		const int mask = mix_mask_;
		const bool was_recording = recording_.exchange(false);

		if (!was_recording && mask == 0)
			return;

		for (int i = 0; i < 6; ++i) {
			if ((mask & (1 << i)) != 0)
				obs_remove_raw_audio_callback(static_cast<size_t>(i), &AudioRecorderSession::rawAudioCaptureThunk, this);
		}

		combined_.stop();
		for (auto &state : channels_)
			state.stop();

		std::lock_guard<std::mutex> lock(mutex_);
		mix_mask_ = 0;
		combine_selected_ = false;
		output_paths_.clear();
	}

private:
	struct ChannelState {
		void handleAudio(const struct audio_data *audio_data)
		{
			const uint32_t frames = audio_data->frames;
			const uint32_t channel_count = channels_;
			if (frames == 0 || channel_count == 0)
				return;

			std::vector<int16_t> pcm;
			pcm.resize(static_cast<size_t>(frames) * channel_count);

			if (planar_) {
				for (uint32_t frame = 0; frame < frames; ++frame) {
					for (uint32_t channel = 0; channel < channel_count; ++channel) {
						float sample = 0.0f;
						if (audio_data->data[channel]) {
							const auto *channel_data = reinterpret_cast<const float *>(audio_data->data[channel]);
							sample = channel_data[frame];
						}
						pcm[static_cast<size_t>(frame) * channel_count + channel] = floatToPcm16(sample);
					}
				}
			} else {
				const auto *interleaved = reinterpret_cast<const float *>(audio_data->data[0]);
				if (!interleaved)
					return;

				for (uint32_t frame = 0; frame < frames; ++frame) {
					for (uint32_t channel = 0; channel < channel_count; ++channel) {
						const size_t sample_index = static_cast<size_t>(frame) * channel_count + channel;
						pcm[sample_index] = floatToPcm16(interleaved[sample_index]);
					}
				}
			}

			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!active_)
					return;

				const size_t bytes = pcm.size() * sizeof(int16_t);
				data_bytes_written_ += bytes;
				chunks_.emplace_back(reinterpret_cast<const uint8_t *>(pcm.data()),
						    reinterpret_cast<const uint8_t *>(pcm.data()) + bytes);
			}

			cv_.notify_one();
		}

		bool start(int mix_index, uint32_t channels, uint32_t sample_rate, const QDir &dir, const QString &base_name,
			   QString *error)
		{
			const QString file_name = base_name + QStringLiteral("-channel-") + QString::number(mix_index + 1) +
						  QStringLiteral(".wav");
			const QString file_path = dir.filePath(file_name);
			const std::filesystem::path fs_file_path = std::filesystem::u8path(file_path.toUtf8().constData());
			std::ofstream file(fs_file_path, std::ios::binary | std::ios::trunc);
				if (!file.is_open()) {
					if (error)
						*error = QStringLiteral("Unable to open the output file for writing.");
					return false;
				}

			WavHeader header;
			header.channels = static_cast<uint16_t>(channels);
			header.sample_rate = sample_rate;
			header.byte_rate = sample_rate * channels * sizeof(int16_t);
			header.block_align = static_cast<uint16_t>(channels * sizeof(int16_t));
			file.write(reinterpret_cast<const char *>(&header), sizeof(header));

			{
				std::lock_guard<std::mutex> lock(mutex_);
				active_ = true;
				mix_index_ = mix_index;
				channels_ = channels;
				sample_rate_ = sample_rate;
				planar_ = false;
				output_path_ = file_path;
				data_bytes_written_ = 0;
				stop_requested_ = false;
				file_ = std::move(file);
			}

			writer_thread_ = std::thread([this]() { writerLoop(); });
			return true;
		}

		void stop()
		{
			QString output_path;
			bool had_audio = false;

			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!active_ && !writer_thread_.joinable())
					return;
				stop_requested_ = true;
				active_ = false;
			}

			cv_.notify_all();
			if (writer_thread_.joinable())
				writer_thread_.join();

			patchHeader();

			{
				std::lock_guard<std::mutex> lock(mutex_);
				had_audio = data_bytes_written_ > 0;
				output_path = output_path_;
				output_path_.clear();
				mix_index_ = 0;
				channels_ = 0;
				sample_rate_ = 0;
				data_bytes_written_ = 0;
				stop_requested_ = false;
				file_.close();
				chunks_.clear();
			}

			if (!had_audio && !output_path.isEmpty())
				QFile::remove(output_path);
		}

		QString outputPath() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return output_path_;
		}

	private:
		void writerLoop()
		{
			for (;;) {
				std::vector<uint8_t> chunk;
				{
					std::unique_lock<std::mutex> lock(mutex_);
					cv_.wait(lock, [this]() { return stop_requested_ || !chunks_.empty(); });

					if (chunks_.empty()) {
						if (stop_requested_)
							break;
						continue;
					}

					chunk = std::move(chunks_.front());
					chunks_.pop_front();
				}

				if (!chunk.empty())
					file_.write(reinterpret_cast<const char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
			}

			file_.flush();
		}

		void patchHeader()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!file_.is_open())
				return;

			const uint32_t data_size = static_cast<uint32_t>(data_bytes_written_);
			const uint32_t riff_size = 36 + data_size;

			file_.seekp(4, std::ios::beg);
			file_.write(reinterpret_cast<const char *>(&riff_size), sizeof(riff_size));
			file_.seekp(40, std::ios::beg);
			file_.write(reinterpret_cast<const char *>(&data_size), sizeof(data_size));
			file_.flush();
		}

		mutable std::mutex mutex_;
		std::condition_variable cv_;
		std::deque<std::vector<uint8_t>> chunks_;
		std::thread writer_thread_;
		std::ofstream file_;
		QString output_path_;
		int mix_index_ = 0;
		uint32_t channels_ = 0;
		uint32_t sample_rate_ = 0;
		size_t data_bytes_written_ = 0;
		bool stop_requested_ = false;
		bool active_ = false;
		bool planar_ = false;
	};

	static size_t countBits(size_t value)
	{
		size_t count = 0;
		while (value != 0) {
			count += value & 1;
			value >>= 1;
		}
		return count;
	}

	struct CombinedState {
		bool start(int mix_mask, size_t selected_count, uint32_t channels, uint32_t sample_rate, const QDir &dir,
			   const QString &base_name, QString *error)
		{
			const QString file_name = base_name + QStringLiteral(".wav");
			const QString file_path = dir.filePath(file_name);
			const std::filesystem::path fs_file_path = std::filesystem::u8path(file_path.toUtf8().constData());
			std::ofstream file(fs_file_path, std::ios::binary | std::ios::trunc);
			if (!file.is_open()) {
				if (error)
					*error = QStringLiteral("Unable to open the output file for writing.");
				return false;
			}

			WavHeader header;
			header.channels = static_cast<uint16_t>(channels);
			header.sample_rate = sample_rate;
			header.byte_rate = sample_rate * channels * sizeof(int16_t);
			header.block_align = static_cast<uint16_t>(channels * sizeof(int16_t));
			file.write(reinterpret_cast<const char *>(&header), sizeof(header));

			{
				std::lock_guard<std::mutex> lock(mutex_);
				active_ = true;
				mix_mask_ = mix_mask;
				expected_mix_count_ = selected_count;
				channels_ = channels;
				sample_rate_ = sample_rate;
				output_path_ = file_path;
				data_bytes_written_ = 0;
				stop_requested_ = false;
				pending_timestamp_ = 0;
				pending_frames_ = 0;
				pending_planes_ = 0;
				pending_received_mask_ = 0;
				pending_samples_.clear();
				file_ = std::move(file);
			}

			writer_thread_ = std::thread([this]() { writerLoop(); });
			return true;
		}

		void stop()
		{
			QString output_path;
			bool had_audio = false;

			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!active_ && !writer_thread_.joinable())
					return;
				stop_requested_ = true;
				active_ = false;
			}

			cv_.notify_all();
			if (writer_thread_.joinable())
				writer_thread_.join();

			patchHeader();

			{
				std::lock_guard<std::mutex> lock(mutex_);
				had_audio = data_bytes_written_ > 0;
				output_path = output_path_;
				output_path_.clear();
				mix_mask_ = 0;
				expected_mix_count_ = 0;
				channels_ = 0;
				sample_rate_ = 0;
				data_bytes_written_ = 0;
				stop_requested_ = false;
				pending_timestamp_ = 0;
				pending_frames_ = 0;
				pending_planes_ = 0;
				pending_received_mask_ = 0;
				pending_samples_.clear();
				file_.close();
			}

			if (!had_audio && !output_path.isEmpty())
				QFile::remove(output_path);
		}

		QString outputPath() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return output_path_;
		}

		void handleAudio(size_t mix_idx, const struct audio_data *audio_data)
		{
			const uint32_t frames = audio_data->frames;
			const uint32_t channel_count = channels_;
			if (frames == 0 || channel_count == 0)
				return;

			const bool planar = audio_data->data[1] != nullptr;
			const size_t mix_bit = size_t{1} << mix_idx;

			std::lock_guard<std::mutex> lock(mutex_);
			if (!active_)
				return;

			const bool block_changed = pending_samples_.empty() || pending_timestamp_ != audio_data->timestamp ||
						  pending_frames_ != frames || pending_planes_ != channel_count;
			if (block_changed) {
				pending_timestamp_ = audio_data->timestamp;
				pending_frames_ = frames;
				pending_planes_ = channel_count;
				pending_received_mask_ = 0;
				pending_samples_.assign(static_cast<size_t>(frames) * channel_count, 0.0f);
			}

			if ((pending_received_mask_ & mix_bit) != 0)
				return;

			if (planar) {
				for (uint32_t frame = 0; frame < frames; ++frame) {
					for (uint32_t channel = 0; channel < channel_count; ++channel) {
						if (!audio_data->data[channel])
							continue;
						const auto *channel_data = reinterpret_cast<const float *>(audio_data->data[channel]);
						pending_samples_[static_cast<size_t>(frame) * channel_count + channel] += channel_data[frame];
					}
				}
			} else {
				const auto *interleaved = reinterpret_cast<const float *>(audio_data->data[0]);
				if (!interleaved)
					return;
				const size_t total_samples = static_cast<size_t>(frames) * channel_count;
				for (size_t sample = 0; sample < total_samples; ++sample)
					pending_samples_[sample] += interleaved[sample];
			}

			pending_received_mask_ |= mix_bit;
			if (countBits(pending_received_mask_ & mix_mask_) != expected_mix_count_)
				return;

			std::vector<int16_t> pcm;
			pcm.resize(static_cast<size_t>(frames) * channel_count);
			for (size_t i = 0; i < pending_samples_.size(); ++i)
				pcm[i] = floatToPcm16(pending_samples_[i]);

			{
				const size_t bytes = pcm.size() * sizeof(int16_t);
				data_bytes_written_ += bytes;
				chunks_.emplace_back(reinterpret_cast<const uint8_t *>(pcm.data()),
						    reinterpret_cast<const uint8_t *>(pcm.data()) + bytes);
			}

			pending_samples_.clear();
			pending_received_mask_ = 0;
			cv_.notify_one();
		}

	private:
		void writerLoop()
		{
			for (;;) {
				std::vector<uint8_t> chunk;
				{
					std::unique_lock<std::mutex> lock(mutex_);
					cv_.wait(lock, [this]() { return stop_requested_ || !chunks_.empty(); });

					if (chunks_.empty()) {
						if (stop_requested_)
							break;
						continue;
					}

					chunk = std::move(chunks_.front());
					chunks_.pop_front();
				}

				if (!chunk.empty())
					file_.write(reinterpret_cast<const char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
			}

			file_.flush();
		}

		void patchHeader()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!file_.is_open())
				return;

			const uint32_t data_size = static_cast<uint32_t>(data_bytes_written_);
			const uint32_t riff_size = 36 + data_size;

			file_.seekp(4, std::ios::beg);
			file_.write(reinterpret_cast<const char *>(&riff_size), sizeof(riff_size));
			file_.seekp(40, std::ios::beg);
			file_.write(reinterpret_cast<const char *>(&data_size), sizeof(data_size));
			file_.flush();
		}

		mutable std::mutex mutex_;
		std::condition_variable cv_;
		std::deque<std::vector<uint8_t>> chunks_;
		std::thread writer_thread_;
		std::ofstream file_;
		QString output_path_;
		int mix_mask_ = 0;
		size_t expected_mix_count_ = 0;
		uint32_t channels_ = 0;
		uint32_t sample_rate_ = 0;
		size_t data_bytes_written_ = 0;
		bool stop_requested_ = false;
		bool active_ = false;
		uint64_t pending_timestamp_ = 0;
		uint32_t pending_frames_ = 0;
		uint32_t pending_planes_ = 0;
		size_t pending_received_mask_ = 0;
		std::vector<float> pending_samples_;
	};

	static void rawAudioCaptureThunk(void *param, size_t mix_idx, struct audio_data *audio_data)
	{
		static_cast<AudioRecorderSession *>(param)->handleAudio(mix_idx, audio_data);
	}

	void handleAudio(size_t mix_idx, const struct audio_data *audio_data)
	{
		if (!recording_.load())
			return;
		if (mix_idx >= channels_.size())
			return;

		if (combine_selected_)
			combined_.handleAudio(mix_idx, audio_data);
		else
			channels_[mix_idx].handleAudio(audio_data);
	}

	std::array<ChannelState, 6> channels_{};
	CombinedState combined_{};
	std::atomic<bool> recording_{false};
	mutable std::mutex mutex_;
	int mix_mask_ = 0;
	bool combine_selected_ = false;
	QStringList output_paths_;
};

class AudioRecorderPanel final : public QWidget {
public:
	explicit AudioRecorderPanel(QWidget *parent = nullptr)
		: QWidget(parent)
	{
		setObjectName(QStringLiteral("obs-audio-recorder-panel"));

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(12, 12, 12, 12);
		layout->setSpacing(10);

		auto *form = new QFormLayout();
		form->setLabelAlignment(Qt::AlignLeft);
		form->setFormAlignment(Qt::AlignTop);
		form->setVerticalSpacing(8);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

		auto *channel_widget = new QWidget(this);
		auto *channel_grid = new QGridLayout(channel_widget);
		channel_grid->setContentsMargins(0, 0, 0, 0);
		channel_grid->setHorizontalSpacing(12);
		channel_grid->setVerticalSpacing(8);

		for (int i = 0; i < 6; ++i) {
			channel_boxes_[i] = new QCheckBox(QStringLiteral("Channel ") + QString::number(i + 1), channel_widget);
			channel_grid->addWidget(channel_boxes_[i], i / 2, i % 2);
		}

		form->addRow(tr("Audio channels"), channel_widget);

		combine_box_ = new QCheckBox(tr("Combine selected channels into one file"), this);
		form->addRow(QString(), combine_box_);

		auto *output_row = new QWidget(this);
		auto *output_layout = new QHBoxLayout(output_row);
		output_layout->setContentsMargins(0, 0, 0, 0);
		output_layout->setSpacing(6);

		output_dir_ = new QLineEdit(this);
		output_dir_->setPlaceholderText(defaultOutputDirectory());
		output_dir_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		auto *browse = new QPushButton(tr("Browse"), this);
		output_layout->addWidget(output_dir_, 1);
		output_layout->addWidget(browse, 0);
		form->addRow(tr("Output folder"), output_row);

		layout->addLayout(form);

		auto *button_row = new QWidget(this);
		auto *button_layout = new QHBoxLayout(button_row);
		button_layout->setContentsMargins(0, 0, 0, 0);
		button_layout->setSpacing(6);

		record_button_ = new QPushButton(tr("Start recording"), this);
		button_layout->addStretch(1);
		button_layout->addWidget(record_button_);
		layout->addWidget(button_row);
		layout->addStretch(1);

		loadSettings();
		applySavedSelection();

		connect(browse, &QPushButton::clicked, this, [this]() { chooseOutputFolder(); });
		connect(record_button_, &QPushButton::clicked, this, [this]() { toggleRecording(); });
		connect(combine_box_, &QCheckBox::toggled, this, [this](bool) { saveSettings(); });
		for (QCheckBox *box : channel_boxes_)
			connect(box, &QCheckBox::toggled, this, [this](bool) { saveSettings(); });
		connect(output_dir_, &QLineEdit::editingFinished, this, [this]() { saveSettings(); });
	}

	~AudioRecorderPanel() override
	{
		session_.stop();
		saveSettings();
	}

	void loadSettings()
	{
		const QString path = moduleSettingsPath();
		if (path.isEmpty())
			return;

		QSettings settings(path, QSettings::IniFormat);
		settings.beginGroup("audio_recorder");
		const int mix_mask = settings.value("mix_mask", 1).toInt();
		const bool combine_selected = settings.value("combine_selected", false).toBool();
		const QString output_dir = settings.value("output_dir", defaultOutputDirectory()).toString();
		settings.endGroup();

		output_dir_->setText(output_dir);
		pending_mix_mask_ = std::clamp(mix_mask, 1, 0x3f);
		pending_combine_selected_ = combine_selected;
	}

	void saveSettings()
	{
		const QString path = moduleSettingsPath();
		if (path.isEmpty())
			return;

		QSettings settings(path, QSettings::IniFormat);
		settings.beginGroup("audio_recorder");
		settings.setValue("mix_mask", currentMixMask());
		settings.setValue("combine_selected", combine_box_->isChecked());
		settings.setValue("output_dir", output_dir_->text().trimmed());
		settings.endGroup();
		settings.sync();
	}

	void applySavedSelection()
	{
		const int mask = std::clamp(pending_mix_mask_, 1, 0x3f);
		for (int i = 0; i < 6; ++i) {
			channel_boxes_[i]->blockSignals(true);
			channel_boxes_[i]->setChecked((mask & (1 << i)) != 0);
			channel_boxes_[i]->blockSignals(false);
		}
		combine_box_->blockSignals(true);
		combine_box_->setChecked(pending_combine_selected_);
		combine_box_->blockSignals(false);
		updateUiState();
	}

private:
	int currentMixMask() const
	{
		int mask = 0;
		for (int i = 0; i < 6; ++i) {
			if (channel_boxes_[i]->isChecked())
				mask |= (1 << i);
		}
		return mask;
	}

	void chooseOutputFolder()
	{
		const QString start = output_dir_->text().trimmed().isEmpty() ? defaultOutputDirectory() : output_dir_->text().trimmed();
		const QString folder = QFileDialog::getExistingDirectory(this, tr("Choose output folder"), start);
		if (folder.isEmpty())
			return;

		output_dir_->setText(folder);
		saveSettings();
	}

	void updateUiState()
	{
		const bool recording = session_.isRecording();
		for (QCheckBox *box : channel_boxes_)
			box->setEnabled(!recording);
		combine_box_->setEnabled(!recording);
		output_dir_->setEnabled(!recording);
		record_button_->setText(recording ? tr("Stop recording") : tr("Start recording"));
	}

	void toggleRecording()
	{
		if (session_.isRecording()) {
			session_.stop();
			updateUiState();
			saveSettings();
			return;
		}

		QString error;
		const int mix_mask = currentMixMask();
		const QString path = session_.start(mix_mask, combine_box_->isChecked(), output_dir_->text().trimmed(), &error);
		if (path.isEmpty()) {
			QMessageBox::warning(this, tr("Audio Recorder"), error.isEmpty() ? tr("Unable to start recording.") : error);
			updateUiState();
			return;
		}

		updateUiState();
		saveSettings();
	}

	std::array<QCheckBox *, 6> channel_boxes_{};
	QCheckBox *combine_box_ = nullptr;
	QLineEdit *output_dir_ = nullptr;
	QPushButton *record_button_ = nullptr;
	AudioRecorderSession session_;
	int pending_mix_mask_ = 1;
	bool pending_combine_selected_ = false;
};

bool g_dock_created = false;

void createDockTask(void *)
{
	if (g_dock_created)
		return;

	auto *panel = new AudioRecorderPanel();
	panel->setObjectName(QString::fromUtf8(kDockId));

	if (!obs_frontend_add_dock_by_id(kDockId, "Audio Record", panel)) {
		delete panel;
		return;
	}

	g_dock_created = true;
}

} // namespace

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("stevepodmore")

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-audio-recorder] module loaded");
	return true;
}

void obs_module_post_load(void)
{
	obs_queue_task(OBS_TASK_UI, createDockTask, nullptr, true);
}

void obs_module_unload(void)
{
	obs_frontend_remove_dock(kDockId);
	blog(LOG_INFO, "[obs-audio-recorder] module unloaded");
}
