##! [0]
TEMPLATE = app
QT += widgets
DEFINES += PPALSA PORTAUDIO
CPPFLAGS=-fpermissive
LIBS = -lasound -lm -lpthread -lportaudio
FORMS = perfect_pitch.ui
HEADERS = kde_ui.h ppwidget.h utils.h perfect_pitch.h unix_sound.h ppstatusbar.h
SOURCES = main.cpp kde_ui.cpp ppwidget.cpp ppstatusbar.cpp worker.cpp utils.c perfect_pitch.c \
unix_sound.c
#core.c
