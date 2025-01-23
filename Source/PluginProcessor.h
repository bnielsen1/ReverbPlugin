/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class LearningLiveProcessingAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    LearningLiveProcessingAudioProcessor();
    ~LearningLiveProcessingAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioBuffer<float> create_delays(juce::AudioBuffer<float> &buffer, int input_channel);
    juce::AudioBuffer<float> shuffle(juce::AudioBuffer<float>& input_audio, int input_channel);

private:

    double sample_rate;
    int numChannels = 4;

    float max_delay = 0.4f;
    std::vector<float> delay_times = { 0.1f, 0.2f, 0.3f, 0.4f };

    std::vector<int> channel_samples_delayed;
    int max_delay_in_samples;

    int samples_per_block;

    std::vector<int> delay_buf_sizes;

    //std::vector<std::vector<float>> delay_buffers;
    juce::AudioBuffer<float> delay_buffers;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearningLiveProcessingAudioProcessor)
};
