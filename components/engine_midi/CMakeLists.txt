idf_component_register(SRCS "engine_midi.c" "engine_midi_smf.c" "engine_midi_rec.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log heap)
