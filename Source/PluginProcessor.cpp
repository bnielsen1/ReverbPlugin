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

//==============================================================================
void LearningLiveProcessingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // Save sample rate
    sample_rate = sampleRate;
    samples_per_block = samplesPerBlock;

    // OLD

    //delay_buffers.resize(numChannels);
    //channel_samples_delayed.resize(numChannels);
    //delay_buf_sizes.resize(numChannels);

    //for (int channel = 0; channel < numChannels; channel++) {
    //    channel_samples_delayed[channel] = static_cast<int>(delay_times[channel % delay_times.size()] * sample_rate);

    //    delay_buf_sizes[channel] = std::max(channel_samples_delayed[channel], samples_per_block);
    //    delay_buffers[channel].resize(delay_buf_sizes[channel], 0.0f);
    //}

    // NEW

    max_delay_in_samples = max_delay * sample_rate;

    delay_buffers = juce::AudioBuffer<float>(numChannels, std::max(max_delay_in_samples, samples_per_block));
    delay_buffers.clear();

    channel_samples_delayed.resize(numChannels);
    delay_buf_sizes.resize(numChannels);

    for (int channel = 0; channel < numChannels; channel++) {
        channel_samples_delayed[channel] = static_cast<int>(delay_times[channel % delay_times.size()] * sample_rate);
        delay_buf_sizes[channel] = std::max(channel_samples_delayed[channel], samples_per_block);
    }
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

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::create_delays(juce::AudioBuffer<float>& buffer, int input_channel) {

    juce::dsp::AudioBlock<float> block{ buffer };

    auto* channelData = block.getChannelPointer(input_channel);

    juce::AudioBuffer<float> processed_data = juce::AudioBuffer<float>(numChannels, block.getNumSamples());
    processed_data.clear();

    for (int channel = 0; channel < numChannels; channel++) {

        // Copy original signal into backup buffer and decrease its volume
        float* copy_buffer = new float[block.getNumSamples()];
        for (int sample = 0; sample < block.getNumSamples(); sample++) {
            if (sample == 1 || sample == 20 || sample == 100) {
                std::cout << channelData[sample] << "\n";
            }
            copy_buffer[sample] = channelData[sample];
            copy_buffer[sample] *= juce::Decibels::decibelsToGain(-18.0); // Drop volume to 75% of original value
        }

        // Write store buffer to output
        int store_offset = delay_buf_sizes[channel] - std::min(samples_per_block, channel_samples_delayed[channel]);
        for (int sample = 0; sample < block.getNumSamples(); sample++) {
            //channelData[sample] += delay_buffers.getSample(channel, (sample + store_offset));
            processed_data.setSample(channel, sample, delay_buffers.getSample(channel, (sample + store_offset)));
        }

        // Shift store contents forward (loop starts rightmost unwritten sample)
        int shift_distance = std::min(samples_per_block, channel_samples_delayed[channel]);
        for (int sample = store_offset - 1; sample >= 0; sample--) {
            delay_buffers.setSample(channel, (sample + shift_distance), delay_buffers.getSample(channel, sample));
        }

        // Insert copy buffer contents into beginning of store buffer
        delay_buffers.copyFrom(channel, 0, copy_buffer, block.getNumSamples());

        delete[] copy_buffer;
    }

    return processed_data;
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

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::shuffle(juce::AudioBuffer<float>& input_audio, int input_channel) {
    // Generate random flips and polarity swaps
    std::vector<int> polarities = gen_polarity_values(numChannels);
    std::vector<int> swaps = gen_swap_values(numChannels);

    // flipping polarities
    for (int i = 0; i < polarities.size(); i++) {
        input_audio.applyGain(i, 0, input_audio.getNumSamples(), polarities[i]);
    }

    // swapping channels
    juce::AudioBuffer<float> output_audio = juce::AudioBuffer<float>(input_audio.getNumChannels(), input_audio.getNumSamples());
    for (int i = 0; i < swaps.size(); i++) {
        output_audio.copyFrom(i, 0, input_audio, swaps[i], 0, input_audio.getNumSamples());
    }

    return output_audio;
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

    // Make input signal stereo
    buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

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

    for (int input_channel = 0; input_channel < block.getNumChannels(); ++input_channel) {

        juce::AudioBuffer<float> delay_data = create_delays(buffer, input_channel);
        juce::AudioBuffer<float> shuffled_data = shuffle(delay_data, input_channel);
        applyHadamardMatrix(shuffled_data);

        //buffer.clear(0, 0, buffer.getNumSamples());
        //buffer.clear(1, 0, buffer.getNumSamples());

        for (int channel = 0; channel < numChannels; channel++) {
            if (channel == 0) buffer.copyFrom(input_channel, 0, shuffled_data, channel, 0, block.getNumSamples());
            else buffer.addFrom(input_channel, 0, shuffled_data, channel, 0, block.getNumSamples());
        }
    }

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
