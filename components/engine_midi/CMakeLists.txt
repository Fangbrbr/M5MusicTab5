idf_component_register(SRCS "engine_midi_smf_write.c" "engine_midi.c" "engine_midi_smf.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log heap)
