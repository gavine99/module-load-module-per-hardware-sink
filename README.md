# module-load-module-per-hardware-sink
Load a module automatically for every hardware sink in pulseaudio

example config;

load-module module-load-module-per-hardware-sink module="module-ladspa-sink" module_args="sink_name=%m-compressor sink_master=%m dont_move=1 plugin=sc4_1882 label=sc4 control=1,1.5,300,-20,3,1,10" switch_on_connect=1 steal_default=1

breakout;

module="module-ladspa-sink" - string: module to load for hardware sink found at startup or hot-plugged

module_args="sink_name=%m-compressor sink_master=%m dont_move=1 plugin=sc4_1882 label=sc4 control=1,1.5,300,-20,3,1,10" - string: arguments to pass to the module as it loads  ("%m" is replaced by the name of the sink that triggered the module load)

switch_on_connect=1 - boolean: automatically switch to a sink of the module just loaded (like module-switch-on-connect does but without all of its options)

steal_default=1 - boolean: when a new default sink is set by some other software check to see if a module has been loaded for that sink. If the loaded module has a sink, set it as the default sink
