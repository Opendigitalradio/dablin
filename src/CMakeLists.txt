########################################################################
# Setup apps
########################################################################

list(APPEND dablin_sources
    dabplus_decoder.cpp
    ensemble_source.cpp
    ensemble_player.cpp
    edi_source.cpp
    edi_player.cpp
    eti_source.cpp
    eti_player.cpp
    dab_decoder.cpp
    fic_decoder.cpp
    pcm_output.cpp
    tools.cpp
    version.cpp
    wav_output.cpp
    )

set(dablin_cli_sources
    mot_manager.cpp
    pad_decoder.cpp
    dablin.cpp
    )

set(dablin_gtk_sources
    mot_manager.cpp
    pad_decoder.cpp
    dablin_gtk.cpp
    dablin_gtk_dl_plus.cpp
    dablin_gtk_sls.cpp
    )

set(common_link_list
    fec
    ${ATOMIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MPG123_LIBRARIES} ${SDL2_LIBRARIES} ${AAC_LIB} ${ICONV_LIBRARIES}
    )

include_directories(../fec)


########################################################################
# Build the applications and add install rules.
########################################################################

# dablin
add_executable(dablin ${dablin_sources} ${dablin_cli_sources})
target_link_libraries(dablin ${common_link_list})
install(TARGETS dablin DESTINATION bin)

# dablin_gtk
if(GTKMM_FOUND)
    add_executable(dablin_gtk ${dablin_sources} ${dablin_gtk_sources})
    target_link_libraries(dablin_gtk ${common_link_list} ${GTKMM_LIBRARIES})
    install(TARGETS dablin_gtk DESTINATION bin)
endif()
