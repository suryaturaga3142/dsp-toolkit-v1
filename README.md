# dsp-toolkit-v1

My little oscilloscope worked for as a $500Hz$ signal analyzer pretty well. It didn't really do much more than that. Based on large improvements, this is a toolkit I developed for digital signal processing. The MCU uses far higher sampling rate and provides data to the laptop for a script to make more responsive and detailed graphs. It even allows easy switching of modes through a configuration file!

> [!NOTE]
> **Use of AI**: This project used a fair amount of vibe coding, both for the algorithm implementation and the scripts. I worked out and maintained an organized file structure using header and source files that were included only in their mode. There's seperate files for configuration and globals.
>
> Google AI Studio helped me with getting one mode done, after which I was able to expand functionality as well as get callback functions up and running on my own. Pretty fun.

### Table of Contents

1. [Project Overview](#project-overview)
2. [Operation](#operation)
3. [Tools Used](#tools-used)
4. [Development](#development)
5. [Final Thoughts](#final-thoughts)

## Project Overview

How did I go from using an ADC that measures within 3 clock cycles to graphing at only $500Hz$? Small leaks here and there. Constant printing, the cost of starting and stopping UART transmission, etc. This project works on fixing that and provides a range of tools for signal processing and visualization for the average signals and STM32 enthusiast.

This project builds on [oscilloscope_v1](https://github.com/suryaturaga3142/oscilloscope_v1), creating a library of files to use in a neat and organized manner. You'll notice, my ```main.c``` barely has anything beyong initialization! Everything relies on configuration and group initialization. Based on the mode chosen, data from the ADC is processed with the help of CMSIS-DSP library and transmitted as raw bytes to the laptop through UART DMA.

The different modes allow the signal to be analyzed and viewed with real and complex FFT, Cepstral Analysis, and Hilbert Transform. As an additional feature, even a 2D colormap spectrogram of a signal can be viewed.

## Operation

In ```app_config.h```, one of the following modes have to be defined / uncommented: ```MODE_RFFT, MODE_CFFT, MODE_CEPSTRAL_ MODE_HILBERT```. ```wavegen.c``` can be used to configure the DAC if that is being used as a test signal for the device. That's it! Then, you can just head over to your connected laptop and run the corresponding script under [my-scripts](my-scripts/) based on the mode selected. Protection bits prevent you from accidentally choosing the wrong script.

The above image shows a typical RFFT graph. It's also exactly how CFFT looks when the signal is entirely real.

The above image shows the FFT spectrogram for real signals.

The above image shows an example graph of cepstral analysis. The peaking quefrencies point to harmonics in the time signal.

The above image shows an example result from the Hilbert Transform. It's tracking the instantaneous amplitude and frequency!

## Tools Used

All the scripting tools used are the same as always, so I won't bring them up. The CMSIS-DSP functions proved to be very helpful in this, too. I've already explained my use of AI earlier here, so I'll leave that.

## Development

I started by fixing everything that I could from the oscilloscope. First, I switched to use of DMA double buffering. The ping pong action I leveraged itself let me push my triggering timer to $5kHz$, which is the maximum rate given laptop UART rate of $115,200\ baud$. From there, it was free reign. If I was going to stick to real time DSP with my laptop, $5kHz$ was the fastest it would get. I made data collection and transmission use double buffering with DMA to reduce strain on the CPU.

Even though voltages were stored as 12 bits inside ```uint16_t```, I was slow with data transmission since I was printing like 5 sig figs. Problem solved when I just sent the raw data instead. A single start flag, 2 bytes per value (each ASCII char is a byte and values are ```uint16_t```), and termination. Brings the whole thing to max 1030 bytes, which is much better and faster as a toolkit.

I first implemented the FFT related functions just out of interest. I was noticing some harmonics form in my graphs when I tested it, and my research rabbit hole led me to Cepstral Analysis! Understanding the idea of cepstral domain and quefrencies was crazy, but upon seeing the peaks and 'rahmonics', I was pretty sold. It's an excellent tool to keep for monitoring imperfections in any process producing a signal. Looking into wave modulation pushed me towards Hilbert Transforms, which explained to me the idea of instantaneous signal decoding at long last.

## Final Thoughts

A big step up because of how organized the project is. This project taught me a lot about using external libraries, maintaining clean workflows, planning out complex coding, and more technical things like making use of callbacks for double buffering, which is an invaluable tool.

I suppose one point to make is that this project isn't "applied" to anything. I've just developed a pretty cool and technical set of libraries useful for actual projects. I will consider this project "done" once I manage to take libraries I built from here and use them for something like speech visualization or voice activated robotics. See you!
