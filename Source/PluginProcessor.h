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
    juce::AudioBuffer<float> create_delays2(juce::AudioBuffer<float>& buffer, int diff);
    juce::AudioBuffer<float> shuffle(juce::AudioBuffer<float>& input, int diff);
    juce::AudioBuffer<float> applyHadamardMatrix(juce::AudioBuffer<float>& buffer);
    juce::AudioBuffer<float> diffuse(juce::AudioBuffer<float>& buffer, int diff_count);
    juce::AudioBuffer<float> final_delay(juce::AudioBuffer<float>& buffer);

private:

    // REVERB PRIVATE GLOBALS

    double sample_rate;
    int numChannels = 8;
    int samples_per_block;

    // DIFFUSE DELAY VARIABLES

    std::vector<float> delay_steps = { 0.01f, 0.02f, 0.04f, 0.08f };
    std::vector<std::vector<float>> delay_times;
    int diffusion_count = delay_steps.size();

    std::vector<std::vector<int>> polarities;
    std::vector<std::vector<int>> swaps;

    std::vector<int> max_delays_in_samples;
    std::vector<std::vector<int>> channel_samples_delayed;
    std::vector<std::vector<int>> delay_buf_sizes;

    std::vector<juce::AudioBuffer<float>> delay_buffers;


    // FINAL DELAY VALUES
    
    float f_delay_time = 0.2f;
    juce::AudioBuffer<float> f_delay_buffer;
    int f_samples_delayed;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearningLiveProcessingAudioProcessor)
};
