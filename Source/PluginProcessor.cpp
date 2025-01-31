/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <random>

//==============================================================================
LearningLiveProcessingAudioProcessor::LearningLiveProcessingAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

LearningLiveProcessingAudioProcessor::~LearningLiveProcessingAudioProcessor()
{
}

//==============================================================================
const juce::String LearningLiveProcessingAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LearningLiveProcessingAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LearningLiveProcessingAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LearningLiveProcessingAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LearningLiveProcessingAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LearningLiveProcessingAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int LearningLiveProcessingAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LearningLiveProcessingAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String LearningLiveProcessingAudioProcessor::getProgramName (int index)
{
    return {};
}

void LearningLiveProcessingAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

// Helper for random polarities
std::vector<int> gen_polarity_values(int channel_count) {
    std::vector<int> result;
    result.reserve(channel_count); // Reserve space for n entries

    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(0)));

    // Fill the vector with random 1 or -1
    for (int i = 0; i < channel_count; ++i) {
        int randomValue = (std::rand() % 2 == 0) ? 1 : -1;
        result.push_back(randomValue);
    }

    return result;
}

// Generate indicies to swap channels
std::vector<int> gen_swap_values(int channel_count) {
    std::vector<int> result(channel_count);
    for (int i = 0; i < channel_count; ++i) {
        result[i] = i;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(result.begin(), result.end(), g);

    return result;
}

//==============================================================================
void LearningLiveProcessingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // Save sample rate
    sample_rate = sampleRate;
    samples_per_block = samplesPerBlock;

    polarities = gen_polarity_values(numChannels);
    swaps = gen_swap_values(numChannels);

    // DELAY 2 ===========================================================
    max_delay_in_samples = 0; // Initiate max delay in samples

    // Allocate with correct number of channels
    channel_samples_delayed.resize(numChannels);
    delay_start_indexes.resize(numChannels);

    // Set max delay and store each channel's individual delay in vector
    for (int channel = 0; channel < numChannels; channel++) {
        int delay_in_samples = static_cast<int>(std::round(delay_times[channel] * sample_rate));
        if (delay_in_samples > max_delay_in_samples) max_delay_in_samples = delay_in_samples;
        channel_samples_delayed[channel] = delay_in_samples;
    }

    // Create the audio buffer to store delays in and clear it
    delay_buffers = juce::AudioBuffer<float>(numChannels, max_delay_in_samples + samples_per_block);
    delay_buffers.clear();
}

void LearningLiveProcessingAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LearningLiveProcessingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::split_input(juce::AudioBuffer<float>& buffer, int input_channel) {
    juce::dsp::AudioBlock<float> block{ buffer };

    auto* channelData = block.getChannelPointer(input_channel);

    juce::AudioBuffer<float> split_data = juce::AudioBuffer<float>(numChannels, block.getNumSamples());
    split_data.clear();

    for (int channel = 0; channel < numChannels; channel++) {
        split_data.copyFrom(channel, 0, buffer, input_channel, 0, block.getNumSamples());
    }

    return split_data;
}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::create_delays2(juce::AudioBuffer<float>& input) {

    // Create output buffer and clear its contents
    juce::AudioBuffer<float> output = juce::AudioBuffer<float>(numChannels, input.getNumSamples());
    output.clear();

    // For each individual channel
    for (int channel = 0; channel < input.getNumChannels(); channel++) {

        // Calculate where latest input data should be written to in delay buffer (sooner for delays shorter than max delay)
        int start_offset = max_delay_in_samples - channel_samples_delayed[channel];

        // Write the latest input data to its corresponding delay buffer
        for (int sample = 0; sample < input.getNumSamples(); sample++) {
            float delayed_sample = input.getSample(channel, sample);

            // Reduce gain of delayed sample when writing to the 
            delayed_sample = delayed_sample * juce::Decibels::decibelsToGain(-6.0);

            delay_buffers.setSample(channel, sample + start_offset, delayed_sample);
        }

        // Send latest delay buffer data to output
        for (int sample = 0; sample < input.getNumSamples(); sample++) {
            float delay_data = delay_buffers.getSample(channel, sample + max_delay_in_samples);
            output.addSample(channel, sample, delay_data);
        }

        // Shift delay buffer data forward by one buffer size
        for (int sample = delay_buffers.getNumSamples() - 1; sample >= (start_offset + samples_per_block); sample--) {
            float prev_data = delay_buffers.getSample(channel, sample - samples_per_block);
            delay_buffers.setSample(channel, sample, prev_data);
        }
    }

    return output;

}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::shuffle(juce::AudioBuffer<float>& input) {
    juce::AudioBuffer<float> output(input.getNumChannels(), input.getNumSamples());

    for (int channel = 0; channel < input.getNumChannels(); channel++) {
        output.copyFrom(channel, 0, input, swaps[channel], 0, input.getNumSamples());

        float* buffer = output.getWritePointer(channel);
        for (int sample = 0; sample < output.getNumSamples(); sample++) {
            buffer[sample] *= polarities[channel];
        }
    }

    return output;
    
}

void applyHadamardMatrix(juce::AudioBuffer<float>& buffer)
{
    // Ensure the buffer has exactly 4 channels
    jassert(buffer.getNumChannels() == 4);

    constexpr float hadamardMatrix[4][4] = {
        {  1.0f,  1.0f,  1.0f,  1.0f },
        {  1.0f, -1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f, -1.0f },
        {  1.0f, -1.0f, -1.0f,  1.0f }
    };

    const int numSamples = buffer.getNumSamples();

    // Compute input energy for normalization
    float inputEnergy = 0.0f;
    for (int channel = 0; channel < 4; ++channel)
    {
        const float* readPointer = buffer.getReadPointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            inputEnergy += readPointer[sample] * readPointer[sample];
        }
    }

    // Temporary storage for transformed data
    std::array<std::vector<float>, 4> transformedChannels;
    for (auto& channel : transformedChannels)
        channel.resize(numSamples, 0.0f);

    // Apply the Hadamard matrix transformation
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Transform each sample using the Hadamard matrix
        for (int row = 0; row < 4; ++row)
        {
            float transformedSample = 0.0f;
            for (int col = 0; col < 4; ++col)
            {
                transformedSample += hadamardMatrix[row][col] * buffer.getReadPointer(col)[sample];
            }
            transformedChannels[row][sample] = transformedSample;
        }
    }

    // Compute transformed energy
    float transformedEnergy = 0.0f;
    for (int channel = 0; channel < 4; ++channel)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            transformedEnergy += transformedChannels[channel][sample] * transformedChannels[channel][sample];
        }
    }

    // Calculate normalization factor
    float normalizationFactor = (transformedEnergy > 0.0f) ? std::sqrt(inputEnergy / transformedEnergy) : 1.0f;

    // Write the transformed data back to the buffer with normalization
    for (int channel = 0; channel < 4; ++channel)
    {
        float* writePointer = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float normalizedSample = transformedChannels[channel][sample] * normalizationFactor;

            // Clip to prevent overflow
            writePointer[sample] = juce::jlimit(-1.0f, 1.0f, normalizedSample);
        }
    }

    DBG("Input Energy: " << inputEnergy);
    DBG("Transformed Energy: " << transformedEnergy);
    DBG("Normalization Factor: " << normalizationFactor);

}



void LearningLiveProcessingAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    juce::dsp::AudioBlock<float> block{ buffer };

    for (int input_channel = 0; input_channel < 1; ++input_channel) {
        
        juce::AudioBuffer<float> multichannel_data = split_input(buffer, input_channel);
        juce::AudioBuffer<float> delay_data = create_delays2(multichannel_data);
        juce::AudioBuffer<float> shuffled_data = shuffle(delay_data);
        juce::AudioBuffer<float> delay2 = create_delays2(shuffled_data);
        juce::AudioBuffer<float> output = shuffle(delay2);
        //applyHadamardMatrix(shuffled_data);

        //buffer.clear(0, 0, buffer.getNumSamples());
        //buffer.clear(1, 0, buffer.getNumSamples());

        for (int channel = 0; channel < numChannels; channel++) {
            if (channel == 0) buffer.copyFrom(input_channel, 0, output, channel, 0, block.getNumSamples());
            else buffer.addFrom(input_channel, 0, output, channel, 0, block.getNumSamples());
        }
    }

    // Make input signal stereo
    buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

}

//==============================================================================
bool LearningLiveProcessingAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* LearningLiveProcessingAudioProcessor::createEditor()
{
    return new LearningLiveProcessingAudioProcessorEditor (*this);
}

//==============================================================================
void LearningLiveProcessingAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void LearningLiveProcessingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LearningLiveProcessingAudioProcessor();
}
