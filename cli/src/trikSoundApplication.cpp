#include "trikSoundApplication.h"

#include <iostream>

#include <QCoreApplication>
#include <QTimer>
#include <QStringList>
#include <QPair>

#include "trikSound/types.h"
#include "trikSound/trikSoundException.h"
//#include "trikSound/utils.h"
#include "trikSound/wavfile.h"
#include "trikSound/digitalAudioFilter.h"
#include "trikSound/audioPipe.h"
#include "trikSound/audioCaptureFilter.h"
#include "trikSound/trikAudioDeviceManager.h"
#include "trikSound/thresholdVadFilter.h"
//#include "trikSound/debugUtils.h"
#include "trikSound/angleDetector.h"

#include "benchmark.h"

using namespace std;
using namespace fpml;
using namespace trikSound;

class ArgumentsException: public TrikSoundException
{
public:
    ArgumentsException(const char* msg):
        TrikSoundException(msg)
    {}
};

TrikSoundApplication::TrikSoundApplication(QObject* parent):
    QObject(parent)
  , mCmd(NO_COMMAND)
  , mOut(stdout, QIODevice::WriteOnly)
{
    mOut.setRealNumberNotation(QTextStream::FixedNotation);
    mOut.setRealNumberPrecision(4);
}

void TrikSoundApplication::run()
{
    try {
        parseArgs();
    }
    catch (ArgumentsException& exc) {
        mOut << "Arguments are incorrect. Error: " << exc.what() << endl;
        emit finished();
        return;
    }

    if (mCmd == LISTEN_FILE) {
        listenWavFile();
        emit finished();
    }
    else if (mCmd == LISTEN_MICR) {
        listen();
    }
    else if (mCmd == RECORD_FILE) {
        record();
    }
    else if (mCmd == BENCHMARK) {
        benchmark();
        emit finished();
    }
    else if (mCmd == LIST_DEVICES) {
        printAllDevicesInfo();
        emit finished();
    }
}

void TrikSoundApplication::stopRecording()
{
    mDeviceManager->stop();

    mOut << "Stop recording" << endl;

    QList< QPair<QString, AudioBuffer> > pairs;
    const AudioBuffer buf(mDeviceManager->buffer()->readAll(), mDeviceManager->audioFormat());

    if (mSplitChFlag) {
        QString filename1 = mFilename + ".left";
        QString filename2 = mFilename + ".right";

        pairs << qMakePair(filename1, buf.leftChannel()) << qMakePair(filename2, buf.rightChannel());
    }
    else {
        pairs << qMakePair(mFilename, buf);
    }

    for (auto& pair: pairs) {
        WavFile file(pair.first);
        if (!file.open(WavFile::WriteOnly, pair.second.audioFormat())) {
            mOut << "Cannot open file " << pair.first.toAscii().data() << endl;
            emit finished();
            return;
        }
        file.write(pair.second);
    }

    emit finished();
}

void TrikSoundApplication::parseArgs()
{
    mCmd = NO_COMMAND;
    bool filenameSet = false;
    bool micrDistSet = false;
    bool durationSet = false;
    bool thresholdSet = false;
    bool frameLengthSet = false;

    mSplitChFlag = false;

    QStringList argv = QCoreApplication::arguments();
    int argc = argv.size();

    if (argc <= 1) {
        throw ArgumentsException("To few arguments. Specify filename and distance between microphones");
    }

    for (int i = 2; i < argc; i++) {
        if (argv[i] == "-f") {
            if (++i >= argc) {
                throw ArgumentsException("Filename is missing");
            }
            mFilename = argv[i];
            filenameSet = true;
        }
        else if (argv[i] == "-c") {
            if (++i >= argc) {
                throw ArgumentsException("Microphone distance is missing");
            }
            mMicrDist = argv[i].toDouble(&micrDistSet);
        }
        else if (argv[i] == "-d") {
            if (++i >= argc) {
                throw ArgumentsException("Record duration is missing");
            }
            mDuration = argv[i].toInt(&durationSet);
        }
        else if (argv[i] == "-t") {
            if (++i >= argc) {
                throw ArgumentsException("Threshold is missing");
            }
            mThreshold = argv[i].toDouble(&thresholdSet);
        }
        else if (argv[i] == "-s") {
            mSplitChFlag = true;
        }
        else if (argv[i] == "-l") {
            if (++i >= argc) {
                throw ArgumentsException("Microphone distance is missing");
            }
            mFrameLength = argv[i].toInt(&frameLengthSet);
        }
    }

    if (argv[1] == "listen-file") {
        mCmd = LISTEN_FILE;

        if (!filenameSet) {
            throw ArgumentsException("Filename is missing");
        }
        if (!micrDistSet) {
            throw ArgumentsException("Microphone distance is missing or incorrect");
        }
    }
    else if (argv[1] == "listen") {
        mCmd = LISTEN_MICR;

        if (!micrDistSet) {
            throw ArgumentsException("Microphone distance is missing or incorrect");
        }
        if (!thresholdSet) {
            throw ArgumentsException("VAD threshold is missing or incorrect");
        }
        if (!frameLengthSet) {
            throw ArgumentsException("Frame length is missing or incorrect");
        }
    }
    else if (argv[1] == "record") {
        mCmd = RECORD_FILE;

        if (!filenameSet) {
            throw ArgumentsException("Filename is missing");
        }
        if (!durationSet) {
            throw ArgumentsException("Recording duration is missing or incorrect");
        }
    }
    else if (argv[1] == "benchmark") {
        mCmd = BENCHMARK;

        if (!filenameSet) {
            throw ArgumentsException("Filename is missing");
        }
        if (!durationSet) {
            throw ArgumentsException("Frame duration is missing or incorrect");
        }
    }
    else if (argv[1] == "list-devices") {
        mCmd = LIST_DEVICES;
    }
}

bool TrikSoundApplication::listenWavFile()
{
    WavFile file(mFilename);
    if (!file.open(WavFile::ReadOnly)) {
        mOut << "Cannot open file " << mFilename.toAscii().data() << endl;
        return false;
    }

    // offset 1000 samles
    const int offset = 1000;
    if (file.sampleCount() < offset) {
        mOut << "File is too short" << endl;
        return false;
    }
    file.seek(offset);

    AudioBuffer buf = file.readAll();
    AudioBuffer chl1 = buf.leftChannel();
    AudioBuffer chl2 = buf.rightChannel();

    dprint_sequence("ch1.test", (sample_type*) chl1.data(), (sample_type*) chl1.data() + chl1.sampleCount());
    dprint_sequence("ch2.test", (sample_type*) chl2.data(), (sample_type*) chl2.data() + chl2.sampleCount());

    vector<sample_type> vchl1((sample_type*) chl1.data(), (sample_type*) chl1.data() + chl1.sampleCount());
    vector<sample_type> vchl2((sample_type*) chl2.data(), (sample_type*) chl2.data() + chl2.sampleCount());

    DigitalAudioFilter<vector<sample_type>> filter;
    filter.handleWindow(vchl1.begin(), vchl1.end());
    filter.handleWindow(vchl2.begin(), vchl2.end());


    dprint_sequence("filt1.test", vchl1.begin(), vchl1.end());
    dprint_sequence("filt2.test", vchl2.begin(), vchl2.end());

    AngleDetector<vector<sample_type>> detector(file.audioFormat(), mMicrDist);
    double angle = 0;
    detector.handleWindow(vchl1.begin(), vchl1.end(),
                          vchl2.begin(), vchl2.end());
    angle = detector.getAngle(1);


////    out << "Angle: " << angle << endl;
    mOut << angle << endl;

    return true;
}

void TrikSoundApplication::listen()
{
//    try {
//        initAudioDevice();
//    }
//    catch (TrikSoundException& exc) {
//        mOut << exc.what();
//        return;
//    }

//    mCapture = QSharedPointer<IAudioFilter>(new AudioCaptureFilter(mDeviceManager,
//                                            (mDeviceManager->audioFormat().sampleRate() / 1000) * mFrameLength));
//    mDetector = QSharedPointer<IAudioFilter>(new AngleFilter(mMicrDist, mThreshold));

//    connect(dynamic_cast<QObject*>(mCapture.data()), SIGNAL(output(AudioBuffer)),
//            dynamic_cast<QObject*>(mDetector.data()), SLOT(input(AudioBuffer)));

//    mDeviceManager->start();

}

void TrikSoundApplication::record()
{
//    try {
//        initAudioDevice();
//    }
//    catch (TrikSoundException& exc) {
//        mOut << exc.what();
//        return;
//    }

//    mDeviceManager->setBufferCapacity((mDuration + 10) * mDeviceManager->audioFormat().sampleRate());


//    QTimer::singleShot(mDuration * 1000, this, SLOT(stopRecording()));
//    mDeviceManager->start();

//    mOut << "Start recording" << endl;
}

void TrikSoundApplication::benchmark()
{
//    qint32 res = angleDetectorBenchmark(mFilename, mDuration);

    ofstream out;
    out.open("testWindow.test");

    testWindowHandler(mFilename, out, mDuration);

//    mOut << "Benchmark result: " << res << endl;

    emit finished();
}

void TrikSoundApplication::initAudioDevice()
{
    QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultInputDevice();

    QAudioFormat audioFormat;
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setChannelCount(2);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setSampleRate(44100);
    audioFormat.setSampleType(QAudioFormat::SignedInt);
    audioFormat.setSampleSize(16);

    // 1 MB
    size_t capacity = 1 * 1024 * 1024;

    auto cbPtr = make_shared<boost::circular_buffer<sample_type>>(capacity);
    auto cbAPtr = make_shared<CircularBufferQAdapter>(cbPtr);

    #ifdef TRIK
        mDeviceManager = QSharedPointer<AudioDeviceManager>(new TrikAudioDeviceManager(deviceInfo,
                                                                                      audioFormat,
                                                                                      capacity));
    #else
        mDeviceManager = QSharedPointer<AudioDeviceManager>(new AudioDeviceManager(deviceInfo,
                                                                                  audioFormat,
                                                                                  cbAPtr));
    #endif
}

void TrikSoundApplication::printAllDevicesInfo()
{
    mOut << "INPUT DEVICES:" << endl;
    for (auto& info: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        printDeviceInfo(info);
    }
    mOut << "OUTPUT DEVICES:" << endl;
    for (auto& info: QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        printDeviceInfo(info);
    }
}

void TrikSoundApplication::printDeviceInfo(const QAudioDeviceInfo& info)
{
    mOut << "Device name: " << info.deviceName() << endl;
    QString indent = "*   ";
    mOut << indent << "Codecs: ";
    for (auto& codec: info.supportedCodecs()) {
        mOut << codec << " ";
    }
    mOut << endl;
    mOut << indent << "Sample rates: ";
    for (auto& rate: info.supportedSampleRates()) {
        mOut << rate << " ";
    }
    mOut << endl;
    mOut << indent << "Sample sizes: ";
    for (auto& ssize: info.supportedSampleSizes()) {
        mOut << ssize << " ";
    }
    mOut << endl;
    mOut << indent << "Sample types: ";
    for (auto& type: info.supportedSampleTypes()) {
        mOut << type << " ";
    }
    mOut << endl;
    mOut << indent << "Channels count: ";
    for (auto& count: info.supportedChannelCounts()) {
        mOut << count << " ";
    }
    mOut << endl;
    mOut << indent << "Byte orders: ";
    for (auto& order: info.supportedByteOrders()) {
        mOut << order << " ";
    }
    mOut << endl;
}