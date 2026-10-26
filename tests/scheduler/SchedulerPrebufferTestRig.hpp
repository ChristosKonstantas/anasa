#ifndef SCHEDULER_PREBUFFER_TEST_RIG_HPP
#define SCHEDULER_PREBUFFER_TEST_RIG_HPP

#include <vector>

#include "audio/AudioBlockProcessor.hpp"
#include "audio/AudioBuffer.hpp"
#include "execution/ImmediateRenderExecutor.hpp"
#include "render/Renderer.hpp"
#include "render/TestRenderKernel.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{
    class SchedulerPrebufferTestRig
    {
    public:
        static constexpr int CHUNK_COUNT = 16;
        static constexpr int TOTAL_FRAMES = CHUNK_COUNT * CHUNK_FRAMES;

        explicit SchedulerPrebufferTestRig(const SchedulerSettings& settings)
            : versions(CHUNK_COUNT),
              readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, audioSettings.channelCount, audioSettings.audioBlockFrames),
              renderer(kernel, versions),
              executor(renderer),
              scheduler(settings, audioSettings, RenderSettings{}, TOTAL_FRAMES, state, versions, executor, readyAudioQueue),
              processor(audioSettings.audioBlockFrames, TOTAL_FRAMES, state, readyAudioQueue),
              output(audioSettings.channelCount, audioSettings.audioBlockFrames),
              outputChannels(audioSettings.channelCount)
        {
            for (int channel = 0; channel < audioSettings.channelCount; ++channel)
                outputChannels[channel] = output[channel].data();
        }

        void requestPlayAt(int frame)
        {
            SchedulerTestRig::handleCommand(scheduler, {CommandType::SetViewport, 0, 0});
            SchedulerTestRig::handleCommand(scheduler, {CommandType::Seek, frame, 0});
            SchedulerTestRig::handleCommand(scheduler, {CommandType::Play, 0, 0});
        }

        AudioProcessResult consumeBlock()
        {
            return processor.processBlock({outputChannels, audioSettings.audioBlockFrames});
        }

        AudioSettings           audioSettings;
        SharedState             state;
        VersionTable            versions;
        SpscQueue<AudioBlock>   readyAudioQueue;
        TestRenderKernel        kernel;
        Renderer                renderer;
        ImmediateRenderExecutor executor;
        Scheduler               scheduler;
        AudioBlockProcessor     processor;
        AudioBuffer             output;
        std::vector<float*>     outputChannels;
    };
} // namespace anasa

#endif // SCHEDULER_PREBUFFER_TEST_RIG_HPP