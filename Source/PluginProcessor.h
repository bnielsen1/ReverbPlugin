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

private:

    double sample_rate;
    int numChannels = 4;

    //float delay_time = 800 * pow(10, -3);
    std::vector<float> delay_times = { 0.05f, 0.1f, 0.15f, 0.2f };

    //int samples_delayed;
    std::vector<int> channel_samples_delayed;

    int samples_per_block;

    //int delay_buf_size;
    std::vector<int> delay_buf_sizes;

    //float* delay_buffer;
    std::vector<std::vector<float>> delay_buffers;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearningLiveProcessingAudioProcessor)
};
