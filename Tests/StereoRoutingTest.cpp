#include <JuceHeader.h>
#include "../Source/PluginChannelRouting.hpp"
#include "../Source/StereoRouting.hpp"
#include "../Source/UpdateManifestValidation.hpp"
#include <cmath>
#include <iostream>

class GainProcessor : public juce::AudioProcessor
{
public:
    GainProcessor() : AudioProcessor (BusesProperties().withInput ("in", juce::AudioChannelSet::stereo())
                                                       .withOutput ("out", juce::AudioChannelSet::stereo())) {}
    const juce::String getName() const override { return "Test gain"; }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override { audio.applyGain (0.5f); }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    uint32 parsedMask = 0;
    if (! parsePluginChannelMask ("5", parsedMask) || parsedMask != 5
        || ! pluginUsesChannel (parsedMask, 0) || pluginUsesChannel (parsedMask, 1)
        || ! pluginUsesChannel (parsedMask, 2) || pluginUsesChannel (parsedMask, 8)
        || parsePluginChannelMask ("256", parsedMask)
        || parsePluginChannelMask ("-1", parsedMask)
        || parsePluginChannelMask ("abc", parsedMask))
        return 8;
    int currentVersion[3] {}, newerVersion[3] {};
    if (! parseLightHostVersion ("1.2.3", currentVersion)
        || ! parseLightHostVersion ("1.2.4", newerVersion)
        || ! isNewerLightHostVersion (newerVersion, currentVersion)
        || isNewerLightHostVersion (currentVersion, newerVersion)
        || parseLightHostVersion ("1.2", currentVersion)
        || parseLightHostVersion ("1.2.x", currentVersion)
        || ! isValidLightHostReleaseAssetUrl (
            "https://github.com/Anna-Spanner/LightHost/releases/download/v1.2.3/Setup.exe")
        || ! isValidLightHostReleaseAssetUrl (
            "https://github.com/Anna-Spanner/LightHost/releases/download/v1.2.3/LightHost-linux-x64")
        || isValidLightHostReleaseAssetUrl ("https://example.com/Setup.exe")
        || isValidLightHostReleaseAssetUrl (
            "https://github.com/Anna-Spanner/LightHost/releases/download/../Setup.exe")
        || ! isValidLightHostSha256 (juce::String::repeatedString ("a", 64))
        || isValidLightHostSha256 ("not-a-hash"))
        return 9;
    using Graph = juce::AudioProcessorGraph;
    using IO = Graph::AudioGraphIOProcessor;
    for (int inputs : { 1, 2 })
    {
        Graph graph;
        graph.setPlayConfigDetails (inputs, 2, 44100.0, 64);
        const Graph::NodeID input (1), output (2), effect (3);
        graph.addNode (std::make_unique<IO> (IO::audioInputNode), input);
        graph.addNode (std::make_unique<IO> (IO::audioOutputNode), output);
        graph.addNode (std::make_unique<GainProcessor>(), effect);
        if (! connectStereo (graph, input, effect).isEmpty()
            || ! connectStereo (graph, effect, output).isEmpty()
            || connectStereo (graph, Graph::NodeID (4), output).isEmpty())
            return 1;
        graph.prepareToPlay (44100.0, 64);
        juce::AudioBuffer<float> buffer (2, 64);
        for (int sample = 0; sample < 64; ++sample)
        {
            buffer.setSample (0, sample, 0.25f);
            buffer.setSample (1, sample, inputs == 1 ? 0.0f : 0.75f);
        }
        juce::MidiBuffer midi;
        graph.processBlock (buffer, midi);
        for (int sample = 0; sample < 64; ++sample)
            if (std::abs (buffer.getSample (0, sample) - 0.125f) > 0.0001f
                || std::abs (buffer.getSample (1, sample) - (inputs == 1 ? 0.125f : 0.375f)) > 0.0001f)
                return 2;
        graph.releaseResources();
    }
    {
        Graph graph;
        graph.setPlayConfigDetails (4, 4, 44100.0, 64);
        const Graph::NodeID input (100), output (101);
        graph.addNode (std::make_unique<IO> (IO::audioInputNode), input);
        graph.addNode (std::make_unique<IO> (IO::audioOutputNode), output);
        for (int channel = 0; channel < 4; ++channel)
        {
            const Graph::NodeID effect ((uint32) (110 + channel));
            graph.addNode (std::make_unique<GainProcessor>(), effect);
            if (! connectIndependentChannelInput (graph, input, effect, channel).isEmpty()
                || ! connectIndependentChannelOutput (graph, effect, output, 0, channel).isEmpty())
                return 3;
        }
        graph.prepareToPlay (44100.0, 64);
        juce::AudioBuffer<float> buffer (4, 64);
        for (int channel = 0; channel < 4; ++channel)
            for (int sample = 0; sample < 64; ++sample)
                buffer.setSample (channel, sample, (float) (channel + 1) / 8.0f);
        juce::MidiBuffer midi;
        graph.processBlock (buffer, midi);
        for (int channel = 0; channel < 4; ++channel)
            for (int sample = 0; sample < 64; ++sample)
                if (std::abs (buffer.getSample (channel, sample) - (float) (channel + 1) / 16.0f) > 0.0001f)
                    return 4;
        graph.releaseResources();
    }
    {
        Graph graph;
        graph.setPlayConfigDetails (4, 4, 44100.0, 64);
        const Graph::NodeID input (200), output (201);
        graph.addNode (std::make_unique<IO> (IO::audioInputNode), input);
        graph.addNode (std::make_unique<IO> (IO::audioOutputNode), output);
        const uint32 channelMask = 0b0101;
        for (int channel = 0; channel < 4; ++channel)
        {
            if (! pluginUsesChannel (channelMask, channel))
                continue;
            const Graph::NodeID effect ((uint32) (210 + channel));
            graph.addNode (std::make_unique<GainProcessor>(), effect);
            if (! connectIndependentChannelInput (graph, input, effect, channel).isEmpty()
                || ! connectIndependentChannelOutput (graph, effect, output, 0, channel).isEmpty())
                return 5;
        }
        for (int channel = 0; channel < 4; ++channel)
            if (! pluginUsesChannel (channelMask, channel)
                && ! graph.addConnection ({ { input, channel }, { output, channel } }))
                return 6;
        graph.prepareToPlay (44100.0, 64);
        juce::AudioBuffer<float> buffer (4, 64);
        for (int channel = 0; channel < 4; ++channel)
            for (int sample = 0; sample < 64; ++sample)
                buffer.setSample (channel, sample, (float) (channel + 1) / 8.0f);
        juce::MidiBuffer midi;
        graph.processBlock (buffer, midi);
        for (int channel = 0; channel < 4; ++channel)
        {
            const float expected = (float) (channel + 1) / 8.0f
                                   * (pluginUsesChannel (channelMask, channel) ? 0.5f : 1.0f);
            for (int sample = 0; sample < 64; ++sample)
                if (std::abs (buffer.getSample (channel, sample) - expected) > 0.0001f)
                    return 7;
        }
        graph.releaseResources();
    }
    std::cout << "Mono duplication, stereo isolation, per-channel plugin selection, and missing-node error passed.\n";
    return 0;
}
