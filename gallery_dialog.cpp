#include "gallery_dialog.h"
#include "ui_gallery_dialog.h"
#include <iostream>
#include <QDebug>

gallery_dialog::gallery_dialog(QWidget *parent, program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::gallery_dialog)
{
    ui->setupUi(this);
    gallery_dialog_program_state = my_program_state;

    QDir dir("/media/mmcblk0p1/recordings");
    //QDir dir("./");

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

        for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        ui->listWidget->addItem(fileInfo.fileName());

        }
}

gallery_dialog::~gallery_dialog()
{
    delete ui;
}

void gallery_dialog::on_back_btn_clicked()
{
    close();
}

void gallery_dialog::on_play_btn_clicked()
{

   // QListWidgetItem *videoItem = (ui->listWidget->currentItem()).text();
    QString fn =   (ui->listWidget->currentItem())->text();
    qDebug() << fn;
    gallery_dialog_program_state->video_player_settings_state.recording_filename =  "/media/mmcblk0p1/recordings/" + fn;
    player_dialog plr_diag(this,gallery_dialog_program_state);
    plr_diag.setWindowState(Qt::WindowFullScreen);
    plr_diag.exec();
    qDebug() << "Returning from player!";

}
