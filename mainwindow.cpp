#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    my_bell_thread = NULL;
    ui->setupUi(this);

    display_on_timer.setSingleShot(true);
    display_on_timer.setInterval(10000);
    connect(&display_on_timer, SIGNAL(timeout()), this, SLOT(operational_timeout()));
    display_on_timer.start(); //turns off light after application is first loaded.

    my_touch_detector = new touch_detector;
    qApp->installEventFilter(my_touch_detector);
    connect(my_touch_detector, SIGNAL(touch_detected_signal()), this, SLOT(screen_pressed()));


    my_safe_encode_video_context.mutex.lock();
    my_safe_encode_video_context.put_data = false;
    my_safe_encode_video_context.is_encoding = false;
    my_safe_encode_video_context.mutex.unlock();

    my_safe_encode_audio_context.mutex.lock();
    my_safe_encode_audio_context.audio_safe_queue = NULL;
    my_safe_encode_audio_context.mutex.unlock();

    //will call set_default_program_state(); here;

    my_program_state.settings_state.movie_recording_name_prefix = "recording";
    //my_program_state.settings_state.movie_recording_directory = "/media/mmcblk0p1/recordings/";
    my_program_state.settings_state.movie_recording_directory = "./";

    my_program_state.settings_state.picture_directory = "./";

    my_program_state.settings_state.movie_recording_number = 0; //default
    my_program_state.settings_state.picture_number = 0; //default

    my_program_state.settings_state.bell_sound = 1; //default


    //my_bell_thread          = new bell_thread(this, &my_program_state);
    //my_gpio_event_detector_thread  = new gpio_event_detector_thread(this, BELL_PIN);
    //my_gpio_event_detector_thread->startThread();
    //connect(my_gpio_event_detector_thread, SIGNAL(rising_edge()), this, SLOT(ring_bell_slot()));

    myinterrupt.startThread();
    connect(&myinterrupt, SIGNAL(edge()), this, SLOT(motion_detected_slot()));


    my_audio_capture_thread = new audio_capture_thread(this, &my_safe_encode_audio_context);
    my_video_capture_thread = new video_capture_thread(this, &my_safe_encode_video_context, &my_program_state);
    my_movie_encoder_thread = new movie_encoder_thread(this, &my_safe_encode_video_context, &my_safe_encode_audio_context, &my_program_state);

    connect(my_video_capture_thread, SIGNAL(renderedImage(image_with_mutex *)), ui->videoPane, SLOT(setPicture(image_with_mutex *)),Qt::DirectConnection); //just made direct. (has solved the issue.)

    connect(my_audio_capture_thread,SIGNAL(audio_capture_started_signal(start_context*)),&my_audio_playback_thread,SLOT(act_on_audio_thread_start(start_context*)));
    connect(my_audio_capture_thread,SIGNAL(audio_capture_stopped_signal()),&my_audio_playback_thread,SLOT(act_on_audio_thread_stop()));
    //for audio.

    my_program_state.video_player_settings_state.recording_filename = "empty";

    my_video_capture_thread->startThread();// this will basically always be on on this menu.

    record_or_stop_btn_state = true;
    ui->record_or_stop_btn->setText("Record");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_record_or_stop_btn_clicked()
{
    if(record_or_stop_btn_state)
    {
        record_or_stop_btn_state = false;
        ui->record_or_stop_btn->setText("Stop");
        my_video_capture_thread->set_camera_color_space(YUV422);
        my_movie_encoder_thread->startThread();
        //my_video_capture_thread->startThread();
        my_audio_capture_thread->startThread(ENCODER_QUEUE);
        //can also have a method that turns on queues while encoder is running. (can be useful for hear and talk.)

    }//record
    else
    {
        record_or_stop_btn_state = true;
        ui->record_or_stop_btn->setText("Record");
        my_movie_encoder_thread->stopThread();
        my_video_capture_thread->set_camera_color_space(RGB16);
        //my_video_capture_thread->stopThread();
        my_audio_capture_thread->stopThread();
    }
}

void MainWindow::on_menu_btn_clicked()
{


    //what happends if the encoder is running?

    my_video_capture_thread->stopThread();
    //my_audio_capture_thread->stopThread();

    menu_dialog mnu_dlg(this,&my_program_state);
    mnu_dlg.setWindowState(Qt::WindowFullScreen);
    mnu_dlg.exec();
    qDebug() << "Returning from menu dialog!";
    my_video_capture_thread->startThread();
    //my_audio_capture_thread->startThread(PLAYBACK_AND_ENCODER_QUEUES); //depends on previous state


}
void MainWindow::ring_bell_slot()
{

    qDebug("We have detected and event!");
/*
    bool audio_playback_was_running = my_audio_playback_thread.isRunning();
    if(audio_playback_was_running)
    {
        my_audio_playback_thread.stopThread(); //will most likely work without problem. Yes the queue will pile up and therefore will no longer be in sync but we need to empty the queue.
        //flush the queue.
    }

    if(my_bell_thread != NULL)
    {
       my_bell_thread->ring();
    }


    if(audio_playback_was_running)
    {
        //my_audio_capture_thread->flush_playback_queue();
        my_audio_playback_thread.startThread();
    }
    */
}

void MainWindow::on_talk_btn_pressed()
{
    //check if encoder is not running and stuff. code below is just for testing,

    //if encoder is running,
    //make a function in audio capture thread which changes modes while running and calls appropriate signals.
    //then just switch mode with both playback and encoder queues running.

    if(my_movie_encoder_thread->isRunning() && my_audio_capture_thread->isRunning())
    {
       my_audio_capture_thread->change_capture_type(PLAYBACK_AND_ENCODER_QUEUES);
    }
    else
    {
        my_audio_capture_thread->startThread(PLAYBACK_QUEUE);
    }
}

void MainWindow::on_talk_btn_released()
{
    if(my_movie_encoder_thread->isRunning() && my_audio_capture_thread->isRunning()) //bit stupid here.
    {
       my_audio_capture_thread->change_capture_type(PLAYBACK_QUEUE_OFF);
    }
    else
    {
        my_audio_capture_thread->stopThread();
    }
}

void MainWindow::on_hear_btn_pressed()
{
    if(my_movie_encoder_thread->isRunning() && my_audio_capture_thread->isRunning())
    {
       my_audio_capture_thread->change_capture_type(LISTEN_PLAYBACK_AND_ENCODER_QUEUES);
    }
    else
    {
        my_audio_capture_thread->startThread(LISTEN_PLAYBACK_QUEUE);
    }
}

void MainWindow::on_hear_btn_released()
{
    if(my_movie_encoder_thread->isRunning() && my_audio_capture_thread->isRunning()) //bit stupid here.
    {
       my_audio_capture_thread->change_capture_type(PLAYBACK_QUEUE_OFF);
    }
    else
    {
        my_audio_capture_thread->stopThread();
    }
}

void MainWindow::operational_timeout()
{
    mybacklight.turn_off();
}
void MainWindow:: screen_pressed()
{
    mybacklight.turn_on(); //problem is that screen will turnoff even if you press other buttons and not press video pane.
    display_on_timer.start();
}

void MainWindow:: motion_detected_slot()
{
    mybacklight.turn_on();
    my_video_capture_thread->set_take_photos_flag();
    display_on_timer.start();
}

