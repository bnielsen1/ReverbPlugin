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

    juce::AudioBuffer<float> split_input(juce::AudioBuffer<float>& buffer, int input_channel);
    //juce::AudioBuffer<float> create_delays(juce::AudioBuffer<float> &buffer);
    juce::AudioBuffer<float> create_delays2(juce::AudioBuffer<float>& buffer);
    juce::AudioBuffer<float> shuffle(juce::AudioBuffer<float>& input);

private:

    double sample_rate;
    int numChannels = 1;

    std::vector<float> delay_times = { 0.1f };

    std::vector<int> channel_samples_delayed;
    int max_delay_in_samples;

    int samples_per_block;

    std::vector<int> delay_buf_sizes;
    std::vector<int> delay_start_indexes;

    //std::vector<std::vector<float>> delay_buffers;
    juce::AudioBuffer<float> delay_buffers;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearningLiveProcessingAudioProcessor)
};
