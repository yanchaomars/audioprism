#ifndef _INTERFACE_THREAD_HPP
#define _INTERFACE_THREAD_HPP

#include <functional>

#include <SDL.h>
#include <SDL_ttf.h>

#include "AudioThread.hpp"
#include "SpectrogramThread.hpp"
#include "ThreadSafeQueue.hpp"

class InterfaceThread {
  public:
    InterfaceThread(ThreadSafeQueue<std::vector<uint32_t>> &pixelsQueue, AudioSource &audioSource, std::mutex &audioSourceLock, RealDft &dft, std::mutex &dftLock, Spectrogram &spectrogram, std::mutex &spectrogramLock, AudioThread &audioThread, SpectrogramThread &spectrogramThread, unsigned int width, unsigned int height);
    ~InterfaceThread();

    void run();
    std::atomic<bool> running;

  private:
    ThreadSafeQueue<std::vector<uint32_t>> &pixelsQueue;
    AudioSource &audioSource;
    std::mutex &audioSourceLock;
    RealDft &dft;
    std::mutex &dftLock;
    Spectrogram &spectrogram;
    std::mutex &spectrogramLock;
    AudioThread &audioThread;
    SpectrogramThread &spectrogramThread;

    SDL_Window *win;
    SDL_Renderer *renderer;
    SDL_Texture *pixelsTexture;
    SDL_Texture *settingsTexture;
    SDL_Texture *cursorTexture;
    SDL_Rect settingsRect;
    SDL_Rect cursorRect;
    TTF_Font *font;

    void updateSettings();
    void renderSettings();
    void renderCursor(int x);
    void handleKeyDown(const uint8_t *state);

    /* Interface settings */
    const unsigned int width, height;
    bool hideInfo;

    /* Cached settings from audio source, dft, and spectrogram classes */
    struct {
        unsigned int sampleRate;
        unsigned int readSize;
        RealDft::WindowFunction wf;
        unsigned int dftSize;
        std::function<float (int)> fPixelToHz;
        double magnitudeMin;
        double magnitudeMax;
        bool magnitudeLog;
        Spectrogram::ColorScheme colors;
    } settings;
};

#endif

