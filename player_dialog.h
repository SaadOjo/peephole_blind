#ifndef PLAYER_DIALOG_H
#define PLAYER_DIALOG_H

#include <QDialog>
#include "audio_playback_thread.h"
#include "movie_decoder_thread.h"
#include "structures.h"
#include "program_state.h"

namespace Ui {
class player_dialog;
}

class player_dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit player_dialog(QWidget *parent = 0, program_state *my_program_state = NULL);
    ~player_dialog();

signals:
    void setImage(image_with_mutex *i);
    void test_set_seek_time(int time);
    
private slots:

    void on_back_btn_clicked();

    void on_pause_btn_clicked();

    void on_play_stop_btn_clicked();

    void on_seek_slider_valueChanged(int value);

    void on_volume_slider_valueChanged(int value);

    void on_mute_chk_toggled(bool checked);

    void set_time_left_label(int time_left);

    void playbackStoppedSlot();

    void slider_test(int);





    void on_seek_slider_sliderReleased();

private:
    Ui::player_dialog *ui;
    bool video_playing;
    program_state *player_dialog_program_state;

    movie_decoder_thread my_movie_decoder_thread;
    audio_playback_thread athread;

};

#endif // PLAYER_DIALOG_H
