#include "player_dialog.h"
#include "ui_player_dialog.h"
#include <QDebug>

player_dialog::player_dialog(QWidget *parent, program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::player_dialog)
{
    ui->setupUi(this);
    player_dialog_program_state =  my_program_state;

    //here if thw function below return error then no need to open this stuff. exit and pop a message. (error return already implemented)
    int ret = my_movie_decoder_thread.setFilename(my_program_state->video_player_settings_state.recording_filename);

    fprintf(stderr,"The value returned from set filename is %d. \n", ret);

    if(ret < 0)
    {
        fprintf(stderr,"This should infact close. \n");
         video_file_open_failed = true;
    }
    else
    {
        video_file_open_failed = false;

    }


    connect(this,SIGNAL(setImage(image_with_mutex*)),ui->videoplyr_pane ,SLOT(setPicture(image_with_mutex*)));
    connect(&my_movie_decoder_thread,SIGNAL(frameDecoded(image_with_mutex *)),ui->videoplyr_pane, SLOT(setPicture(image_with_mutex*)), Qt::DirectConnection);
    connect(&my_movie_decoder_thread,SIGNAL(movie_stopped_signal()),this ,SLOT(playbackStoppedSlot()),Qt::DirectConnection);

    //dont forget to delete.
    connect(&my_movie_decoder_thread,SIGNAL(audio_capture_started(start_context*)),&athread,SLOT(act_on_audio_thread_start(start_context*)) , Qt::DirectConnection);
    connect(&my_movie_decoder_thread,SIGNAL(movie_stopped_signal()),&athread,SLOT(act_on_audio_thread_stop()) , Qt::DirectConnection);

    QString fileName = "./video_oynatci.png";
    image_with_mutex first_image;
    first_image.mutex.lock();
    qDebug() << fileName;
    first_image.image = new QImage(fileName);
    first_image.mutex.unlock();
    emit setImage(&first_image);

    connect(&my_movie_decoder_thread,SIGNAL(set_time_left(int)),this,SLOT(set_time_left_label(int)));
    connect(&my_movie_decoder_thread,SIGNAL(set_slider_value(int)),ui->seek_slider,SLOT(setValue(int)));


    ui->play_stop_btn->setText("play");
    video_playing = false;

    ui->seek_slider->setTracking(false);
    ui->seek_slider->setRange(0,100);

    //connect(ui->seek_slider,SIGNAL(valueChanged(int)),&my_movie_decoder_thread,SLOT(seek_request_slot(int)));
    connect(this,SIGNAL(test_set_seek_time(int)),&my_movie_decoder_thread,SLOT(seek_request_slot(int)));

}

void player_dialog::slider_test(int slider_value)
{
    fprintf(stderr,"Slider_valus is : %d \n", slider_value);
}

player_dialog::~player_dialog()
{
    delete ui;
}

void player_dialog::on_back_btn_clicked()
{
//go to video select menu
    //need to stop all threads before we go back
    my_movie_decoder_thread.stopThread();
    athread.stopThread();
    close();
}

void player_dialog::on_pause_btn_clicked()
{
    //pause the video
    emit test_set_seek_time(50);
}

void player_dialog::on_play_stop_btn_clicked()
{
    if(video_file_open_failed)
    {
        close();
    }
    else
    {

        if(!video_playing)
        {
            ui->play_stop_btn->setText("stop");
            video_playing = true;

            if(!my_movie_decoder_thread.isRunning())  //Play Video
            {
                my_movie_decoder_thread.startThread();
            }
            //audio thread is controlled by the movie decoder thread.
        }
        else
        {
            ui->play_stop_btn->setText("play");
            my_movie_decoder_thread.stopThread();
        }
    }

}

void player_dialog::playbackStoppedSlot()
{
    qDebug() << "Stopped button pressed.";
    ui->play_stop_btn->setText("play");
    video_playing = false;
}

void player_dialog::set_time_left_label(int time_left)
{
    QString time_left_string;
    int minutes = time_left/60;
    int seconds = time_left - minutes*60;
    time_left_string = QString::number(minutes) + ":";
    if(seconds<10)
    {
        time_left_string += "0";
    }
    time_left_string += QString::number(seconds);
    ui->video_time_label->setText(time_left_string);
}


void player_dialog::on_seek_slider_valueChanged(int value)
{
    // can use same speaker class but maybe a new object
}

void player_dialog::on_volume_slider_valueChanged(int value)
{
    // use the slider value to change the audio level
}

void player_dialog::on_mute_chk_toggled(bool checked)
{
    //turn audio on or off using checked bool
}

void player_dialog::on_seek_slider_sliderReleased()
{
}
