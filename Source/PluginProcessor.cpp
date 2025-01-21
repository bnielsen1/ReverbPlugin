/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    for (int input_channel = 0; input_channel < block.getNumChannels(); ++input_channel)
    {
        auto* channelData = block.getChannelPointer(input_channel);

        for (int channel = 0; channel < numChannels; channel++) {

            // Copy original signal into backup buffer and decrease its volume
            float* copy_buffer = new float[block.getNumSamples()];
            for (int sample = 0; sample < block.getNumSamples(); sample++) {
                if (sample == 1 || sample == 20 || sample == 100) {
                    std::cout << channelData[sample] << "\n";
                }
                copy_buffer[sample] = channelData[sample];
                copy_buffer[sample] *= juce::Decibels::decibelsToGain(-5.0); // Drop volume to 75% of original value
            }

            // Write store buffer to output
            int store_offset = delay_buf_sizes[channel] - std::min(samples_per_block, channel_samples_delayed[channel]);
            for (int sample = 0; sample < block.getNumSamples(); sample++) {
                channelData[sample] += delay_buffers.getSample(channel, (sample + store_offset));
            }

            // Shift store contents forward (loop starts rightmost unwritten sample)
            int shift_distance = std::min(samples_per_block, channel_samples_delayed[channel]);
            for (int sample = store_offset - 1; sample >= 0; sample--) {
                delay_buffers.setSample(channel, (sample + shift_distance), delay_buffers.getSample(channel, sample));
            }

            // Insert copy buffer contents into beginning of store buffer
            delay_buffers.copyFrom(channel, 0, copy_buffer, block.getNumSamples());
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
