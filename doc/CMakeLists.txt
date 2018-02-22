########################################################################
# Add docs install rules.
########################################################################

if(NOT MAN_INSTALL_DIR)
    set(MAN_INSTALL_DIR "share/man")
endif()

# dablin
install(FILES dablin.1 DESTINATION ${MAN_INSTALL_DIR}/man1/)

# dablin_gtk
if(GTKMM_FOUND)
    install(FILES dablin_gtk.1 DESTINATION ${MAN_INSTALL_DIR}/man1/)
endif()
