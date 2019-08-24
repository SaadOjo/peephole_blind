#ifndef PROGRAM_STATE_H
#define PROGRAM_STATE_H

#include <QMutex>
#include <QString>


typedef struct settings //might be a good idea to make it thread safe. only these not the program state. because we only care about if the individual stuff is being used properly
{
  char* movie_recording_name_prefix; //Qstring might be better?
  char* movie_recording_directory; //might be causing seg faults??
  char* picture_directory;
  int   bell_sound;
  int   movie_recording_number;
  int   picture_number;

}settings;

typedef struct video_player_settings //change to movie_player_settings
{
  QString recording_filename; //already being used.
} video_player_settings;

typedef struct picture_dialog_settings //change to movie_player_settings
{
  QString picture_filename; //already being used.
} picture_dialog_settings;

typedef struct movie_encoder_settings
{
  QString encoding_directory;
  int recording_number;
} video_encoder_settings;


typedef struct program_state
{
  QMutex mutex;
  settings settings_state;
  video_player_settings video_player_settings_state;
  picture_dialog_settings picture_dialog_settings_state;

}program_state;

/*
void set_default_program_state(program_state* local_program_state)
{
    //set settings
    local_program_state->settings_state.movie_name_prefix = "recording_";

}
*/

#endif // PROGRAM_STATE_H
