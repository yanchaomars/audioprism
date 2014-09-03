#include <iostream>
#include <cstring>
#include <unistd.h>

#include "SpectrogramThread.hpp"

#define DEFAULT_DFT_SIZE    2048
#define DEFAULT_WINDOW_FUNC WindowFunction::Hanning

SpectrogramThread::SpectrogramThread(ThreadSafeQueue<std::vector<double>> &samplesQueue, ThreadSafeQueue<std::vector<uint32_t>> &pixelsQueue, unsigned int sampleRate, unsigned int width) : samplesQueue(samplesQueue), pixelsQueue(pixelsQueue), sampleRate(sampleRate), width(width), dft(DEFAULT_DFT_SIZE, DEFAULT_WINDOW_FUNC), spectrogram() { }

void SpectrogramThread::run() {
    std::vector<double> samples(dft.getSize());
    std::vector<double> magnitudes;
    std::vector<uint32_t> pixels(width);

    while (true) {
        std::lock_guard<std::mutex> lg(settingsLock);

        std::vector<double> newSamples(samplesQueue.pop());

        /* Move down old samples */
        memmove(samples.data(), samples.data()+newSamples.size(), sizeof(double)*(samples.size()-newSamples.size()));
        /* Copy new samples */
        memcpy(samples.data()+(samples.size()-newSamples.size()), newSamples.data(), sizeof(double)*newSamples.size());

        /* Compute DFT */
        dft.compute(magnitudes, samples);

        /* Render spectrogram line */
        spectrogram.render(pixels, magnitudes);

        /* Put into pixels queue */
        pixelsQueue.push(pixels);
    }
}

unsigned int SpectrogramThread::getDftSize() {
    std::lock_guard<std::mutex> lg(settingsLock);
    return dft.getSize();
}

WindowFunction SpectrogramThread::getWindowFunction() {
    std::lock_guard<std::mutex> lg(settingsLock);
    return dft.getWindowFunction();
}

double SpectrogramThread::getMagnitudeMin() {
    std::lock_guard<std::mutex> lg(settingsLock);
    return spectrogram.settings.magnitudeMin;
}

double SpectrogramThread::getMagnitudeMax() {
    std::lock_guard<std::mutex> lg(settingsLock);
    return spectrogram.settings.magnitudeMax;
}

void SpectrogramThread::setDftSize(unsigned int N) {
    std::lock_guard<std::mutex> lg(settingsLock);
    dft.setSize(N);
}

void SpectrogramThread::setWindowFunction(WindowFunction wf) {
    std::lock_guard<std::mutex> lg(settingsLock);
    dft.setWindowFunction(wf);
}

void SpectrogramThread::setMagnitudeMin(double min) {
    std::lock_guard<std::mutex> lg(settingsLock);
    spectrogram.settings.magnitudeMin = min;
}

void SpectrogramThread::setMagnitudeMax(double max) {
    std::lock_guard<std::mutex> lg(settingsLock);
    spectrogram.settings.magnitudeMax = max;
}

