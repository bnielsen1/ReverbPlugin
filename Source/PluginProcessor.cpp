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

    // DIFFUSE DELAY INITIALIZATION

    // For each diffusion setup delay variables
    for (int diff = 0; diff < diffusion_count; diff++) {
        // Generate delay times for this diffusion
        std::vector<float> cur_delay_times;
        for (int channel = 0; channel < numChannels; channel++) {
            cur_delay_times.push_back((channel + 1) * delay_steps[diff]);
        }
        delay_times.push_back(cur_delay_times);

        // Initiate to 0 as reference point
        max_delays_in_samples.push_back(0);

        // Create the randomized polarities and swaps for each diffusion
        polarities.push_back(gen_polarity_values(numChannels));
        swaps.push_back(gen_swap_values(numChannels));

        // Make sure vectors are created and properly sized
        channel_samples_delayed.push_back({});
        channel_samples_delayed[diff].resize(numChannels);
        delay_buf_sizes.push_back({});
        delay_buf_sizes[diff].resize(numChannels);

        for (int channel = 0; channel < numChannels; channel++) {
            int delay_in_samples = static_cast<int>(std::round(delay_times[diff][channel] * sample_rate));
            if (delay_in_samples > max_delays_in_samples[diff]) max_delays_in_samples[diff] = delay_in_samples;
            channel_samples_delayed[diff][channel] = delay_in_samples;
        }

        // Create the audio buffer to store delays in and clear it
        delay_buffers.push_back(juce::AudioBuffer<float>(numChannels, max_delays_in_samples[diff] + samples_per_block));
        delay_buffers[diff].clear();
    }

    // FINAL DELAY INITIALIZATION

    f_samples_delayed = static_cast<int>(std::round(f_delay_time * sample_rate));
    f_delay_buffer = juce::AudioBuffer<float>(1, f_samples_delayed + samples_per_block);
    f_delay_buffer.clear();

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

void householderMix4Channels(juce::AudioBuffer<float>& buffer)
{
    // Ensure we have exactly 4 channels
    jassert(buffer.getNumChannels() == 4);

    const int numSamples = buffer.getNumSamples();

    // Get write pointers for all channels
    float* ch0 = buffer.getWritePointer(0);
    float* ch1 = buffer.getWritePointer(1);
    float* ch2 = buffer.getWritePointer(2);
    float* ch3 = buffer.getWritePointer(3);

    // Householder mixing matrix coefficients
    const float scale = 0.25f; // 1/sqrt(4) for energy preservation
    const float diag = scale;    // Diagonal elements
    const float offDiag = -scale; // Off-diagonal elements

    for (int i = 0; i < numSamples; ++i)
    {
        // Read original samples
        const float in0 = ch0[i];
        const float in1 = ch1[i];
        const float in2 = ch2[i];
        const float in3 = ch3[i];

        // Apply Householder matrix mixing
        // Matrix structure:
        // [ diag  offDiag offDiag offDiag ]
        // [ offDiag diag  offDiag offDiag ]
        // [ offDiag offDiag diag  offDiag ]
        // [ offDiag offDiag offDiag diag  ]
        ch0[i] = diag * in0 + offDiag * in1 + offDiag * in2 + offDiag * in3;
        ch1[i] = offDiag * in0 + diag * in1 + offDiag * in2 + offDiag * in3;
        ch2[i] = offDiag * in0 + offDiag * in1 + diag * in2 + offDiag * in3;
        ch3[i] = offDiag * in0 + offDiag * in1 + offDiag * in2 + diag * in3;
    }
}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::final_delay(juce::AudioBuffer<float>& buffer) {

    juce::AudioBuffer<float> output = juce::AudioBuffer<float>(numChannels, buffer.getNumSamples());
    output.clear();

    juce::AudioBuffer<float> live_clone = juce::AudioBuffer<float>(numChannels, buffer.getNumSamples());
    live_clone.makeCopyOf(buffer);

    int channel = 0;

    // Apply latest delay buffer data to live signal clone
    for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
        float delay_data = f_delay_buffer.getSample(channel, sample + f_samples_delayed);
        live_clone.addSample(channel, sample, delay_data);
    }

    // Decrease gain on live data
    for (int sample = 0; sample < live_clone.getNumSamples(); sample++) {
        float dropped_gain = live_clone.getSample(channel, sample);

        // Reduce gain of delayed sample when writing
        dropped_gain = dropped_gain * juce::Decibels::decibelsToGain(-2.0);

        live_clone.setSample(channel, sample, dropped_gain);
    }

    // Apply householder to latest delay buffer
    householderMix4Channels(live_clone);

    // Write the live data + delay buffer data to the beginning of the actual delay_data buffer
    for (int sample = 0; sample < live_clone.getNumSamples(); sample++) {
        float delayed_sample = live_clone.getSample(channel, sample);

        f_delay_buffer.setSample(channel, sample, delayed_sample);
    }

    // Send latest delay buffer data to output
    for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
        float delay_data = f_delay_buffer.getSample(channel, sample + f_samples_delayed);
        output.addSample(channel, sample, delay_data);
    }

    // Shift delay buffer data forward by one buffer size
    for (int sample = f_delay_buffer.getNumSamples() - 1; sample >= samples_per_block; sample--) {
        float prev_data = f_delay_buffer.getSample(channel, sample - samples_per_block);
        f_delay_buffer.setSample(channel, sample, prev_data);
    }

    return output;
}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::create_delays2(juce::AudioBuffer<float>& input, int diff) {

    // Create output buffer and clear its contents
    juce::AudioBuffer<float> output = juce::AudioBuffer<float>(numChannels, input.getNumSamples());
    output.clear();

    // For each individual channel
    for (int channel = 0; channel < input.getNumChannels(); channel++) {

        // Calculate where latest input data should be written to in delay buffer (sooner for delays shorter than max delay)
        int start_offset = max_delays_in_samples[diff] - channel_samples_delayed[diff][channel];

        // Write the latest input data to its corresponding delay buffer
        for (int sample = 0; sample < input.getNumSamples(); sample++) {
            float delayed_sample = input.getSample(channel, sample);

            // Reduce gain of delayed sample when writing to the 
            //delayed_sample = delayed_sample * juce::Decibels::decibelsToGain(-6.0);

            delay_buffers[diff].setSample(channel, sample + start_offset, delayed_sample);
        }

        // Send latest delay buffer data to output
        for (int sample = 0; sample < input.getNumSamples(); sample++) {
            float delay_data = delay_buffers[diff].getSample(channel, sample + max_delays_in_samples[diff]);
            output.addSample(channel, sample, delay_data);
        }

        // Shift delay buffer data forward by one buffer size
        for (int sample = delay_buffers[diff].getNumSamples() - 1; sample >= (start_offset + samples_per_block); sample--) {
            float prev_data = delay_buffers[diff].getSample(channel, sample - samples_per_block);
            delay_buffers[diff].setSample(channel, sample, prev_data);
        }
    }

    return output;

}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::shuffle(juce::AudioBuffer<float>& input, int diff) {
    juce::AudioBuffer<float> output(input.getNumChannels(), input.getNumSamples());

    for (int channel = 0; channel < input.getNumChannels(); channel++) {
        output.copyFrom(channel, 0, input, swaps[diff][channel], 0, input.getNumSamples());

        float* buffer = output.getWritePointer(channel);
        for (int sample = 0; sample < output.getNumSamples(); sample++) {
            buffer[sample] *= polarities[diff][channel];
        }
    }

    return output;
    
}

// This doesn't work for some reason :(
juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::applyHadamardMatrix(juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> output = juce::AudioBuffer<float>(buffer.getNumChannels(), buffer.getNumSamples());
    output.clear();

    float H[4][4] = {
    { 1,  1,  1,  1 },
    { 1, -1,  1, -1 },
    { 1,  1, -1, -1 },
    { 1, -1, -1,  1 }
    };

    // Normalize matrix
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            H[i][j] /= sqrt(4);
        }
    }

    // Applying hadamard into output
    // For each column of all input vectors
    for (int sample = 0; sample < buffer.getNumSamples(); sample++) {

        // Solve for the value of each row of that column
        for (int row = 0; row < 4; row++) {
            float summed_value = 0;

            // Calculate each row value in vector times each column value from respective row in matrix
            for (int i = 0; i < 4; i++) {
                float vector_val = buffer.getSample(i, sample);
                float matrix_val = H[row][i];

                summed_value += (vector_val * matrix_val);
            }

            output.setSample(row, sample, summed_value);
        }
    }
    return output;
}

// works! thanks deepseek!
void hadamardMix4Channels(juce::AudioBuffer<float>& buffer)
{
    // Ensure we have exactly 4 channels
    jassert(buffer.getNumChannels() == 4);

    const int numSamples = buffer.getNumSamples();

    // Get write pointers for all channels
    float* ch0 = buffer.getWritePointer(0);
    float* ch1 = buffer.getWritePointer(1);
    float* ch2 = buffer.getWritePointer(2);
    float* ch3 = buffer.getWritePointer(3);

    // Hadamard mixing matrix with scaling factor 1/sqrt(4) = 0.5
    for (int i = 0; i < numSamples; ++i)
    {
        // Read original samples
        const float in0 = ch0[i];
        const float in1 = ch1[i];
        const float in2 = ch2[i];
        const float in3 = ch3[i];

        // Apply Hadamard matrix mixing
        ch0[i] = (in0 + in1 + in2 + in3) * 0.5f;
        ch1[i] = (in0 - in1 + in2 - in3) * 0.5f;
        ch2[i] = (in0 + in1 - in2 - in3) * 0.5f;
        ch3[i] = (in0 - in1 - in2 + in3) * 0.5f;
    }
}

juce::AudioBuffer<float> LearningLiveProcessingAudioProcessor::diffuse(juce::AudioBuffer<float>& buffer, int diff_count) {
    juce::AudioBuffer<float> output;
    output.makeCopyOf(buffer);

    for (int diff = 0; diff < diff_count; diff++) {
        juce::AudioBuffer<float> d = create_delays2(output, diff);
        juce::AudioBuffer<float> s = shuffle(d, diff);
        hadamardMix4Channels(s);
        output = s;
    }

    return output;
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

        //juce::AudioBuffer<float> demo = juce::AudioBuffer<float>(4, 1);
        //demo.setSample(0, 0, 1);
        //demo.setSample(1, 0, 2);
        //demo.setSample(2, 0, -1);
        //demo.setSample(3, 0, 3);

        //juce::AudioBuffer<float> ht = applyHadamardMatrix(demo);

        //float t1 = ht.getSample(0, 0);
        //float t2 = ht.getSample(1, 0);
        //float t3 = ht.getSample(2, 0);
        //float t4 = ht.getSample(3, 0);
        
        juce::AudioBuffer<float> multichannel_data = split_input(buffer, input_channel);
        juce::AudioBuffer<float> diffused_signal = diffuse(multichannel_data, 3);
        juce::AudioBuffer<float> final_delayed = final_delay(diffused_signal);

        //buffer.clear(0, 0, buffer.getNumSamples());
        //buffer.clear(1, 0, buffer.getNumSamples());

        for (int channel = 0; channel < numChannels; channel++) {
            buffer.addFrom(input_channel, 0, final_delayed, channel, 0, block.getNumSamples());
            buffer.addFrom(input_channel, 0, diffused_signal, channel, 0, block.getNumSamples());

            //if (channel == 0) buffer.copyFrom(input_channel, 0, output, channel, 0, block.getNumSamples());
            //else buffer.addFrom(input_channel, 0, output, channel, 0, block.getNumSamples());
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
