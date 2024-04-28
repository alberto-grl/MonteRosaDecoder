# MonteRosaDecoder

Decodes Morse code telemetry from audio piped through stdin.
Customized for IQ2MI/B, a ham radio beacon located on Monte Rosa, 4,556 m asl.

I have been using an RTL-SDR dongle. Call it with something like

rtl_fm -f 144414k -M usb -s 260k -r 48000 -l 1 -p 75 -o 4 -g 40  |  tee >(./MonteRosaDecoder) >(aplay -r 48000 -f S16_LE -t raw -c 1) >/dev/null

An example of telemetry is:

VVVDEIQ2MI/B JN35WW BAT13.85V SUN13.92V TIN 6C TOUT 5C WWW.ARIMI.IT

A shorter file with timestamp is generated:

IQ2MI/B: 2024-04-28 15:10:20 BAT13.82V SUN13.88V TIN 6C TOUT 4C 

It is intended to be fed to JS8Call using ultrajs8 (see  https://github.com/alberto-grl/GW-JS8)

A JS8 message with #ROSA at the beginning triggers the retransmission of telemetry.

This code is a mess and is intended only as a general guidance.
Support for ncurses and portaudio input is inherited from another project, but not used right now.

FFT is used for filtering, my dongle does not have TCXO and a fixed frequency filter is out of question. 
The bin with highest power in a few hundred Hz range is selected. Good for VHF where there are no interfering stations.
There are two low pass one-pole IIR filters for baseband signal and noise floor level.

The 500 mW signal is received with a piece of wire inside my house, have line-of-sight. QRB is 226 Km.



Alberto I4NZX





