#include "picture_dialog.h"
#include "ui_picture_dialog.h"

picture_dialog::picture_dialog(QWidget *parent,program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::picture_dialog)
{
    picture_dialog_program_state = my_program_state;
    ui->setupUi(this);


    connect(this,SIGNAL(setImage(image_with_mutex*)),ui->videoplyr_pane ,SLOT(setPicture(image_with_mutex*)));

    //QString fileName = "index.png";
    QString fileName =  picture_dialog_program_state->picture_dialog_settings_state.picture_filename;

    image_with_mutex first_image; //this might cause seg faults
    first_image.mutex.lock();
    qDebug() << fileName;
    first_image.image = new QImage(fileName);
    first_image.mutex.unlock();
    emit setImage(&first_image);


}

picture_dialog::~picture_dialog()
{
    delete ui;
}

void picture_dialog::on_back_btn_clicked()
{
    close();
}
