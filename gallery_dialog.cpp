#include "gallery_dialog.h"
#include "ui_gallery_dialog.h"
#include <iostream>

gallery_dialog::gallery_dialog(QWidget *parent, program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::gallery_dialog)
{
    ui->setupUi(this);
    gallery_dialog_program_state = my_program_state;
    populate_list();

}

gallery_dialog::~gallery_dialog()
{
    delete ui;
}
void gallery_dialog::populate_list()
{
    QString video_directory;
    video_directory = QString::fromLocal8Bit(gallery_dialog_program_state->settings_state.movie_recording_directory);
    //QDir dir("/media/mmcblk0p1/recordings");
    //QDir dir("./"); //use the same as the recording thing here.
     QDir dir(video_directory); //use the same as the recording thing here.


    if(dir.exists())
    {
        fprintf(stderr,"The directory exists.\n");
    }
    else
    {
        fprintf(stderr,"The directory does not exists.\n");
    }

    QStringList filters;
    filters << "*.txt" << "*.mp4";
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    ui->listWidget->clear();
        for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        ui->listWidget->addItem(fileInfo.fileName());

        }
}


void gallery_dialog::on_back_btn_clicked()
{
    close();
}

void gallery_dialog::on_play_btn_clicked()
{

   // QListWidgetItem *videoItem = (ui->listWidget->currentItem()).text();
    if(ui->listWidget->count() > 0)
    {
        QString fn =   (ui->listWidget->currentItem())->text();
        qDebug() << fn;
        QString video_directory;
        video_directory = QString::fromLocal8Bit(gallery_dialog_program_state->settings_state.movie_recording_directory);
        gallery_dialog_program_state->video_player_settings_state.recording_filename =  video_directory + fn; // + "/" + fn;
        //gallery_dialog_program_state->video_player_settings_state.recording_filename =  "./" + fn;
        player_dialog plr_diag(this,gallery_dialog_program_state);
        plr_diag.setWindowState(Qt::WindowFullScreen);
        plr_diag.exec();
        qDebug() << "Returning from player!";
    }


}

void gallery_dialog::on_pushButton_clicked()
{
    /* File does not delete when this code is used
    QMessageBox msgBox;
    msgBox.setText("Do you want to delete this file?");
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    */


    QString video_directory;
    video_directory = QString::fromLocal8Bit(gallery_dialog_program_state->settings_state.movie_recording_directory);


    if (ui->listWidget->count()!=0)  // do not delete when nothing is selected.
    {
        QString fn =   (ui->listWidget->currentItem())->text();
        fn = video_directory + fn;
        qDebug() << "filetodeltete: " + fn;

        char file_name[25];
        memcpy( file_name, fn.toStdString().c_str() ,fn.size());
        int status = remove(file_name);

          if (status == 0)
          {
              printf("%s file deleted successfully.\n", file_name);
          }
          else
          {
            printf("Unable to delete the file\n");
          }
          populate_list();
    }
      /*
    if (ret = QMessageBox::Yes)
    {

    }
    */
}

void gallery_dialog::on_delete_all_pb_clicked()
{
    QString video_directory;
    int total_items = ui->listWidget->count();
    char file_name[25];
    for(int i = 0; i<total_items; i++)
    {

        video_directory = QString::fromLocal8Bit(gallery_dialog_program_state->settings_state.movie_recording_directory);
        QString fn =   (ui->listWidget->item(i))->text();
        fn = video_directory + fn;
        qDebug() << "filetodeltete: " + fn;

        memcpy( file_name, fn.toStdString().c_str() ,fn.size()); //here can make a function to delete the files.
        int status = remove(file_name);

          if (status == 0)
          {
              printf("%s file deleted successfully.\n", file_name);
          }
          else
          {
            printf("Unable to delete the file\n");
          }
    }
    populate_list();

}
