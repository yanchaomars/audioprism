#include <thread>
#include <iostream>
#include <getopt.h>

#include "audio/PulseAudioSource.hpp"
#include "dft/RealDft.hpp"
#include "spectrogram/SpectrumRenderer.hpp"

#include "audio/WaveAudioSource.hpp"
#include "image/MagickImageSink.hpp"

#include "ThreadSafeQueue.hpp"

#include "AudioThread.hpp"
#include "SpectrogramThread.hpp"
#include "InterfaceThread.hpp"
#include "Configuration.hpp"

using namespace Audio;
using namespace DFT;
using namespace Spectrogram;
using namespace Image;

namespace Configuration {
Settings InitialSettings;
Limits UserLimits;
}

using namespace Configuration;

void spectrogram_realtime() {
    ThreadSafeQueue<std::vector<double>> samplesQueue;
    ThreadSafeQueue<std::vector<uint32_t>> pixelsQueue;

    AudioThread audioThread(samplesQueue, InitialSettings);
    SpectrogramThread spectrogramThread(samplesQueue, pixelsQueue, InitialSettings);
    InterfaceThread interfaceThread(pixelsQueue, audioThread, spectrogramThread, InitialSettings);

    audioThread.start();
    spectrogramThread.start();
    interfaceThread.run();

    spectrogramThread.stop();
    audioThread.stop();
}

void spectrogram_audiofile(std::string audioPath, std::string imagePath) {
    WaveAudioSource audioSource(audioPath);
    RealDft realDft(InitialSettings.dftSize, InitialSettings.dftWf);
    SpectrumRenderer spectrumRenderer(InitialSettings.magnitudeMin, InitialSettings.magnitudeMax, InitialSettings.magnitudeLog, InitialSettings.colors);
    MagickImageSink image(imagePath, InitialSettings.width, (InitialSettings.orientation == Orientation::Horizontal) ? MagickImageSink::Orientation::Horizontal : MagickImageSink::Orientation::Vertical);

    unsigned int samplesOverlap = static_cast<unsigned int>(InitialSettings.samplesOverlap*InitialSettings.dftSize);

    /* Overlapped Samples */
    std::vector<double> overlapSamples(InitialSettings.dftSize);
    /* DFT of Overlapped Samples */
    std::vector<std::complex<double>> dftSamples(InitialSettings.dftSize/2+1);
    /* Pixel line */
    std::vector<uint32_t> pixels(InitialSettings.width);

    while (true) {
        std::vector<double> audioSamples(overlapSamples.size()-samplesOverlap);

        /* Read audio samples */
        audioSource.read(audioSamples);
        if (audioSamples.size() == 0)
            break;

        /* If we're on the final read and short on samples, pad with zeros */
        if (audioSamples.size() < (overlapSamples.size()-samplesOverlap))
            audioSamples.resize(overlapSamples.size()-samplesOverlap);

        /* Move down overlapSamples.size()-samplesOverlap length old samples */
        memmove(overlapSamples.data(), overlapSamples.data()+samplesOverlap, sizeof(double)*(overlapSamples.size()-samplesOverlap));
        /* Copy overlapSamples.size()-samplesOverlap length new samples */
        memcpy(overlapSamples.data()+samplesOverlap, audioSamples.data(), sizeof(double)*(overlapSamples.size()-samplesOverlap));

        /* Compute DFT */
        realDft.compute(dftSamples, overlapSamples);

        /* Render spectrogram line */
        spectrumRenderer.render(pixels, dftSamples);

        /* Add pixel row to image */
        image.append(pixels);
    }

    image.write();
}

void print_usage(std::string progname) {
    std::cerr << "\
Interactive Usage: " << progname << " [options]\n\
 Audio File Usage: " << progname << " [options] <audio file input> <image file output>\n\
\n\
Interface Settings\n\
    -h,--help                   Help\n\
    --width <width>             Width of spectrogram (default 640)\n\
    --height <height>           Height of spectrogram (default 480)\n\
    --orientation <orientation> Orientation [horizontal, vertical]\n\
                                    (default vertical)\n\
\n\
Audio Settings\n\
    -r,--sample-rate <rate>     Audio input sample rate (default 24000)\n\
\n\
DFT Settings\n\
    --overlap <percentage>      Overlap percentage (default 50)\n\
    --dft-size <size>           DFT Size, must be power of two (default 1024)\n\
    --window <window function>  Window Function [hanning, hamming, rectangular]\n\
                                    (default hanning)\n\
\n\
Spectrogram Settings\n\
    --magnitude-scale <scale>   Magnitude Scale [linear, logarithmic]\n\
                                    (default logarithmic)\n\
    --magnitude-min <value>     Magnitude Minimum (default 0.0)\n\
    --magnitude-max <value>     Magnitude Maximum (default 50.0)\n\
    --colors <color scheme>     Color Scheme [heat, blue, grayscale]\n\
                                    (default heat)\n\
\n\
Interactive Keyboard Control:\n\
    q           - Quit\n\
    h           - Hide settings information\n\
    c           - Cycle color scheme\n\
    w           - Cycle window function\n\
    l           - Toggle logarithmic/linear magnitude\n\
\n\
    -           - Decrease minimum magnitude\n\
    =           - Increase minimum magnitude\n\
\n\
    [           - Decrease maximum magnitude\n\
    ]           - Increase maximum magnitude\n\
\n\
    Left arrow  - Decrease DFT Size\n\
    Right arrow - Increase DFT Size\n\
\n\
    Down arrow  - Decrease overlap\n\
    Up arrow    - Increase overlap\n" << std::endl;
}

int main(int argc, char *argv[]) {
    unsigned int overlap = 50;

    static struct option long_options[] = {
        {"help",            no_argument,        0,  'h'},
        {"width",           required_argument,  0,  0},
        {"height",          required_argument,  0,  0},
        {"orientation",     required_argument,  0,  0},
        {"sample-rate",     required_argument,  0,  'r'},
        {"overlap",         required_argument,  0,  0},
        {"dft-size",        required_argument,  0,  0},
        {"window",          required_argument,  0,  0},
        {"magnitude-scale", required_argument,  0,  0},
        {"magnitude-min",   required_argument,  0,  0},
        {"magnitude-max",   required_argument,  0,  0},
        {"colors",          required_argument,  0,  0},
    };

    while (1) {
        int options_index;
        int c = getopt_long(argc, argv, "hr:", long_options, &options_index);

        if (c == -1) {
            break;
        } else if (c == 'h') {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else if (c == 'r') {
            std::cout << "option r with _" << optarg << "_" << std::endl;
        } else if (c == 0) {
            std::string option_name = long_options[options_index].name;
            std::string option_arg = (long_options[options_index].has_arg == required_argument) ? optarg : "";

            if (option_name == "orientation") {
                if (option_arg == "horizontal") {
                    InitialSettings.orientation = Orientation::Horizontal;
                } else if (option_arg == "vertical") {
                    InitialSettings.orientation = Orientation::Vertical;
                } else {
                    std::cerr << "Invalid value for orientation.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "width") {
                try {
                    InitialSettings.width = std::stoul(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid value for width.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "height") {
                try {
                    InitialSettings.height = std::stoul(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid value for height.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "overlap") {
                try {
                    overlap = std::stoul(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid value for overlap.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }

                unsigned int overlapMin = static_cast<unsigned int>(std::round(UserLimits.samplesOverlapMin*100.0));
                unsigned int overlapMax = static_cast<unsigned int>(std::round(UserLimits.samplesOverlapMax*100.0));

                if (overlap > 100 || overlap < overlapMin || overlap > overlapMax) {
                    std::cerr << "Invalid value for overlap (must be >= " << overlapMin << " and <= " << overlapMax << ").\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }

                InitialSettings.samplesOverlap = static_cast<float>(overlap)/100.0;
            } else if (option_name == "dft-size") {
                unsigned int dftSize;
                try {
                    dftSize = std::stoul(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid value for DFT size.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }

                if ((dftSize & (dftSize - 1)) != 0 || dftSize < UserLimits.dftSizeMin || dftSize  > UserLimits.dftSizeMax) {
                    std::cerr << "Invalid value for DFT size (must be power of 2 and >= " << UserLimits.dftSizeMin << " and <= " << UserLimits.dftSizeMax << ").\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }

                InitialSettings.dftSize  = dftSize;
            } else if (option_name == "window") {
                if (option_arg == "hanning")
                    InitialSettings.dftWf = RealDft::WindowFunction::Hanning;
                else if (option_arg == "hamming")
                    InitialSettings.dftWf = RealDft::WindowFunction::Hamming;
                else if (option_arg == "rectangular")
                    InitialSettings.dftWf = RealDft::WindowFunction::Rectangular;
                else {
                    std::cerr << "Invalid window function.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "magnitude-scale") {
                if (option_arg == "logarithmic")
                    InitialSettings.magnitudeLog = true;
                else if (option_arg == "linear")
                    InitialSettings.magnitudeLog = false;
                else {
                    std::cerr << "Invalid magnitude scale.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "magnitude-min") {
                try {
                    InitialSettings.magnitudeMin = std::stod(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid magnitude minimum.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "magntiude-max") {
                try {
                    InitialSettings.magnitudeMax = std::stod(option_arg);
                } catch (const std::invalid_argument &e) {
                    std::cerr << "Invalid magnitude maximum.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            } else if (option_name == "colors") {
                if (option_arg == "logarithmic")
                    InitialSettings.magnitudeLog = true;
                else if (option_arg == "linear")
                    InitialSettings.magnitudeLog = false;
                else {
                    std::cerr << "Invalid magnitude scale.\n\n";
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    /* Validate magnitude min/max with magnitude scale mode */
    if (InitialSettings.magnitudeLog) {
        if (InitialSettings.magnitudeMin < UserLimits.magnitudeLogMin) {
            std::cout << "Invalid magnitude min (must be >= " << UserLimits.magnitudeLogMin << ").\n\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else if (InitialSettings.magnitudeMax > UserLimits.magnitudeLogMax ){
            std::cout << "Invalid magnitude max (must be <= " << UserLimits.magnitudeLogMax << ").\n\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        if (InitialSettings.magnitudeMin < UserLimits.magnitudeLogMin) {
            std::cout << "Invalid magnitude min (must be >= " << UserLimits.magnitudeLinearMin << ").\n\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else if (InitialSettings.magnitudeMax > UserLimits.magnitudeLogMax ){
            std::cout << "Invalid magnitude max (must be <= " << UserLimits.magnitudeLinearMax << ").\n\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if ((argc - optind) > 0 && (argc - optind) != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Audio file mode */
    if ((argc - optind) == 2) {
        spectrogram_audiofile(std::string(argv[optind]), std::string(argv[optind+1]));

    /* Realtime mode */
    } else {
        spectrogram_realtime();
    }

    return 0;
}

