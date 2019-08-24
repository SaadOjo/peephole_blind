#include "picture_gallery.h"
#include "ui_picture_gallery.h"
#include <QString>
picture_gallery::picture_gallery(QWidget *parent, program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::picture_gallery)
{
    ui->setupUi(this);
    picture_gallery_program_state = my_program_state;
    populate_list();
}

picture_gallery::~picture_gallery()
{
    delete ui;
}

void picture_gallery::populate_list()
{
    QString picture_directory;
    picture_directory = QString::fromLocal8Bit(picture_gallery_program_state->settings_state.movie_recording_directory); //change to picture dir
    QDir dir(picture_directory); //use the same as the recording thing here.

    if(dir.exists())
    {
        fprintf(stderr,"The directory exists.\n");
    }
    else
    {
        fprintf(stderr,"The directory does not exists.\n");
    }

    QStringList filters;
    filters  << "*.bmp";
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    ui->listWidget->clear();
    for (int i = 0; i < list.size(); ++i)
    {
        QFileInfo fileInfo = list.at(i);
        ui->listWidget->addItem(fileInfo.fileName());

    }
}

void picture_gallery::on_play_btn_clicked()
{
    // QListWidgetItem *videoItem = (ui->listWidget->currentItem()).text();
     QString fn =   (ui->listWidget->currentItem())->text();
     qDebug() << fn;
     QString picture_directory; //should use mutexes
     picture_directory = QString::fromLocal8Bit(picture_gallery_program_state->settings_state.movie_recording_directory);
     picture_gallery_program_state->picture_dialog_settings_state.picture_filename =  picture_directory + fn; // + "/" + fn; //change
     picture_dialog pic_diag(this,picture_gallery_program_state);
     pic_diag.setWindowState(Qt::WindowFullScreen);
     pic_diag.exec();
     qDebug() << "Returning from picture shower!";
}
void picture_gallery::on_delete_btn_clicked()
{
    /* File does not delete when this code is used
    QMessageBox msgBox;
    msgBox.setText("Do you want to delete this file?");
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    */

    QString picture_directory;
    picture_directory = QString::fromLocal8Bit(picture_gallery_program_state->settings_state.movie_recording_directory);
    QString fn =   (ui->listWidget->currentItem())->text();
    fn = picture_directory + fn;
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
      /*
    if (ret = QMessageBox::Yes)
    {

    }
    */
}


void picture_gallery::on_back_btn_clicked()
{
    close();
}



