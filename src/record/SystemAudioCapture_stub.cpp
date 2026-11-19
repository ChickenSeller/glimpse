#include "SystemAudioCapture.h"

#include <QCoreApplication>

struct SystemAudioCapture::Private {
};

bool SystemAudioCapture::isSupported()
{
    return false;
}

SystemAudioCapture::SystemAudioCapture(AudioTrack *)
    : d(std::make_unique<Private>())
{
}

SystemAudioCapture::~SystemAudioCapture() = default;

bool SystemAudioCapture::start(QString *error)
{
    *error = QCoreApplication::translate("SystemAudioCapture", "Recording the sound played is not supported here.");
    return false;
}

void SystemAudioCapture::stop()
{
}
