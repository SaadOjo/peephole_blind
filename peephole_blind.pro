#-------------------------------------------------
#
# Project created by QtCreator 2019-04-25T17:20:31
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = peephole_blind
TEMPLATE = app

LIBS +=         \
      -lv4l2    \
#    -lavutil   \
    -lavcodec   \
    -lavformat  \
    -lswscale   \
    -lm         \
    -lasound    \
 #   -lpxp


SOURCES += main.cpp\
        mainwindow.cpp \
    videopane.cpp \
    video_capture_thread.cpp \
    pxp.cpp \
    pseudo_camera_driver.cpp \
    menu_dialog.cpp \
    gallery_dialog.cpp \
    player_dialog.cpp \
    movie_decoder_thread.cpp \
    audio_playback_thread.cpp \
    audio_capture_thread.cpp \
    movie_encoder_thread.cpp \
    gpio_event_detector_thread.cpp \
    bell_thread.cpp \
    settings_dialog.cpp \
    interrupt.cpp \
    backlight.cpp

HEADERS  += mainwindow.h \
    videopane.h \
    structures.h \
    video_capture_thread.h \
    pxp.h \
    pseudo_camera_driver.h \
    menu_dialog.h \
    program_state.h \
    gallery_dialog.h \
    player_dialog.h \
    movie_decoder_thread.h \
    audio_playback_thread.h \
    audio_capture_thread.h \
    movie_encoder_thread.h \
    gpio_event_detector_thread.h \
    bell_thread.h \
    settings_dialog.h \
    interrupt.h \
    backlight.h

FORMS    += mainwindow.ui \
    menu_dialog.ui \
    gallery_dialog.ui \
    player_dialog.ui \
    settings_dialog.ui

OTHER_FILES += \
    notes.txt
