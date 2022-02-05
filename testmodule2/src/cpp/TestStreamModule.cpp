//
// Created by Elec332 on 13/11/2021.
//

#include <nativesdr/TestModule.h>
#include <volk/volk.h>
#include <nativesdr/dsp/fft.h>
#include <nativesdr/dsp/iq_converter.h>
#include <nativesdr/dsp/malloc.h>
#include <nativesdr/dsp/windows.h>
#include <nativesdr/util/chart_helper.h>
#include "../../../core/src/headers/util/wav_reader.h"

class FFTTestBlock : public pipeline::threaded_block {

public:

    FFTTestBlock() : pipeline::threaded_block("FFT Block", ImColor(255, 0, 0)) {
        drawFunc = [&](size_t random) {
            double f = 0;
            if (freq) {
                f = (double) *freq;
            }
            double bandwidth = 100000;
            double sr = bandwidth;
            if (stream && stream->auxData) {
                f = (double) ((utils::sampleData*) stream->auxData)->centerFreq;
                bandwidth = ((utils::sampleData*) stream->auxData)->bandwidth;
                sr = ((utils::sampleData*) stream->auxData)->sampleRate;
            }
            int skip = 0;
            if (sr > bandwidth) {
                auto diff = sr - bandwidth;
                auto iPerD = sr / drawSamples;
                skip = (int) std::ceil((diff / sr) * drawSamples);
                if (skip % 2 != 0) {
                    skip++;
                }
                bandwidth = (drawSamples - skip) * iPerD;
            }
            auto hBw = std::abs(bandwidth / 2);
            ImVec2 start = ImGui::GetWindowPos();
            ImVec2 end = start + ImGui::GetWindowSize();
            double yStart = -100;
            ImVec2 ppu = ImGui::DrawChartFrame(start, end, yStart, 0, utils::ui::getDbScale, f - hBw, f + hBw, utils::ui::getFreqScale);
            ImGui::DrawChartLineFilled(start, end, &drawBuf[skip / 2], drawSamples - skip, ppu, yStart, utils::ui::BLUE, utils::ui::BLUE_F);
        };
        drawFuncRef = &drawFunc;
        addInput("IQ in", utils::complexStreamType(), stream);
        addOutput("Renderer", utils::uiType(), drawFuncRef, true);
        addInput("Frequency in", utils::frequencyType(), freq);

        iq = dsp::malloc<utils::complex>(pipeline::BUFFER_COUNT);
        fft = dsp::malloc<utils::complex>(pipeline::BUFFER_COUNT);
        psd = dsp::malloc<float>(pipeline::BUFFER_COUNT);
        drawBuf = dsp::malloc<float>(pipeline::BUFFER_COUNT);
        window = dsp::malloc<float>(pipeline::BUFFER_COUNT);
    }

    ~FFTTestBlock() {
        dsp::free(window);
        dsp::free(iq);
        dsp::free(fft);
        dsp::free(drawBuf);
        dsp::free(psd);
    }

    void loop() override {
        if (stream) {
            stream->read([&](const utils::complex* dat, int samples) {
                if (lastSamples != samples) {
                    resetRoot(samples);
                }
                int factor = 2;
                volk_32fc_32f_multiply_32fc((lv_32fc_t*) iq, (lv_32fc_t*) dat, window, samples);
                plan->execute();
                volk_32fc_s32f_power_spectrum_32f(psd, (lv_32fc_t*) fft, (float) samples, samples);
                int offset = 0;
                for (int i = 0; i < samples / factor; ++i) {
                    float max = -INFINITY;
                    for (int j = 0; j < factor; ++j) {
                        if (psd[offset + j] > max) {
                            max = psd[offset + j];
                        }
                    }
                    drawBuf[i] = (drawBuf[i] * (1 - attack)) + (max * attack);
                    offset += factor;
                }
                drawSamples = samples / factor;
            });
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void drawMiddle() override {
    }

    bool hasMenu() override {
        return true;
    }

    void drawMenu() override {
        ImGui::SliderFloat("FFT Attack", &attack, 0, 1);
        if (ImGui::Combo("##filter", &filter, "Square\0Hamming\0Hann\0")) {
            resetRoot(lastSamples);
        }
    }

    void resetRoot(int newSamples) {
        dsp::WindowFunction windowFunc = getWindow();
        for (int i = 0; i < newSamples; i++) {
            window[i] = i % 2 ? windowFunc(i, newSamples) : -windowFunc(i, newSamples);
        }
        if (lastSamples != newSamples) {
            plan = dsp::create_plan(newSamples, iq, fft, true);
            lastSamples = newSamples;
        }
    }

    [[nodiscard]] dsp::WindowFunction getWindow() const {
        switch (filter) {
            case 0:
                return dsp::squareWindow;
            case 1:
                return dsp::hammingWindow;
            case 2:
                return dsp::hannWindow;
            default:
                return dsp::squareWindow;
        }
    }

private:

    pipeline::datastream<utils::complex>* stream = nullptr;
    uint64_t* freq = nullptr;
    utils::drawFunc drawFunc;
    utils::drawFunc* drawFuncRef;

    dsp::fft_plan_ptr plan;
    float* window;
    utils::complex* iq;
    utils::complex* fft;
    float* psd;
    float* drawBuf;
    int lastSamples = 0;
    int drawSamples = 0;

    float attack = 0.2;
    int filter = 0;

};

class FileStreamTestBlock : public pipeline::threaded_block {

    //std::string fileName = "D:/Downloads/15-29-07_92783258Hz.wav";
//    std::string fileName = "D:/Downloads/ecars_net_7255_HDSDR_20180225_174354Z_7255kHz_RF.wav";
    std::string fileName = "E:\\SDR\\Data\\IQ\\2020_03_17\\Ilyama\\445057332_1.wav";

public:

    FileStreamTestBlock() : pipeline::threaded_block("File Block", ImColor(255, 0, 0)) {
        sampleData = dsp::malloc<utils::sampleData>(1);
        stream = pipeline::createStream<utils::complex>();
        stream->auxData = sampleData;
        fopen_s(&file, fileName.c_str(), "rb");
        data = readWAV(file);
        startPos = ftell(file);
        buf = dsp::malloc<int16_t>(pipeline::BUFFER_COUNT * 2);
        converter = dsp::getConverter(data->sampleData.bitsPerSample, data->sampleData.bitsPerSample != 8, data->sampleData.blockAlign);

        std::cout << "Center freq: " << data->centerFreq << std::endl;
        std::cout << "Length: " << data->length << std::endl;

        std::cout << "A Format: " << data->sampleData.AudioFormat << std::endl;
        std::cout << "Channels: " << data->sampleData.NumOfChan << std::endl;
        std::cout << "Samples Sec: " << data->sampleData.SamplesPerSec << std::endl;
        std::cout << "bpSample: " << data->sampleData.bitsPerSample << std::endl;
        std::cout << "allign: " << data->sampleData.blockAlign << std::endl;
        std::cout << "byperSample: " << data->sampleData.bytesPerSec << std::endl;
        sampleData->bandwidth = data->sampleData.SamplesPerSec;
        sampleData->sampleRate = data->sampleData.SamplesPerSec;
        sampleData->centerFreq = data->centerFreq;

        addOutput("IQ out", utils::complexStreamType(), stream, true);
    }

    ~FileStreamTestBlock() {
        pipeline::deleteStream(stream);
        dsp::free(sampleData);
        fclose(file);
        data.reset();
        dsp::free(buf);
    }

    void loop() override {
        int samples = 1024 * 128;
        int elementSize = sizeof(int16_t) * 2;
        stream->write([&](utils::complex* dat) {
            auto len = fread(buf, elementSize, samples, file);
            if (len < samples) {
                fseek(file, startPos, SEEK_SET);
                fread(&buf[len], elementSize, samples - len, file);
            }
            converter(buf, dat, samples);
            return samples;
        });
//        double base = 1000 * 1000 * 1000;
//        std::this_thread::sleep_for(std::chrono::nanoseconds((int) ((base * samples) / data->sampleData.SamplesPerSec)));
    }

    void start() override {
        stream->start();
        pipeline::threaded_block::start();
    }

    void stop() override {
        stream->stop();
        pipeline::threaded_block::stop();
    }

    void drawMiddle() override {
        std::string str = "File: " + fileName;
        ImGui::TextUnformatted(str.c_str());
    }

private:

    pipeline::datastream<utils::complex>* stream;
    utils::sampleData* sampleData;
    FILE* file = nullptr; //Shut up clang...
    long startPos = 0;
    std::shared_ptr<file_data> data;
    dsp::IQConverter converter;

    int16_t* buf;

};

class AMBlock : public pipeline::threaded_block {

public:

    AMBlock() : pipeline::threaded_block("AM Block", ImColor(255, 0, 0)) {
        streamOut = pipeline::createStream<utils::audio>();
        middleBuf = dsp::malloc<float>(pipeline::BUFFER_COUNT);
        addOutput("Audio out", utils::audioStreamType(), streamOut, true);
        addInput("IQ in", utils::complexStreamType(), streamIn);
    }

    ~AMBlock() {
        pipeline::deleteStream(streamOut);
        dsp::free(middleBuf);
    }

    void loop() override {
        if (streamIn) {
            streamIn->read([&](const utils::complex* data, int len) {
                volk_32fc_magnitude_32f(middleBuf, (lv_32fc_t*) data, len);
                streamOut->write([&](utils::audio* stream) {
                    volk_32f_x2_interleave_32fc((lv_32fc_t*) stream, middleBuf, middleBuf, len);
                    return len;
                });
            });
        }
    }

    void start() override {
        streamOut->start();
        pipeline::threaded_block::start();
    }

    void stop() override {
        streamOut->stop();
        pipeline::threaded_block::stop();
    }

    void drawMiddle() override {
    }

private:

    float* middleBuf;
    pipeline::datastream<utils::complex>* streamIn = nullptr;
    pipeline::datastream<utils::audio>* streamOut;

};

pipeline::block_ptr createStreamFileBlock() {
    return std::make_shared<FileStreamTestBlock>();
}

pipeline::block_ptr createFFTBlock() {
    return std::make_shared<FFTTestBlock>();
}

pipeline::block_ptr createAMBlock() {
    return std::make_shared<AMBlock>();
}