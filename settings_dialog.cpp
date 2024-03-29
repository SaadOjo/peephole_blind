#include "settings_dialog.h"
#include "ui_settings_dialog.h"
#include "program_state.h"

settings_dialog::settings_dialog(QWidget *parent, program_state * pstate) :
    QDialog(parent),
    ui(new Ui::settings_dialog)
{
    pstate->mutex.lock();
    settings_dialog_program_state = pstate;
    pstate->mutex.unlock();
    ui->setupUi(this);

    settings_dialog_program_state->mutex.lock();
    if(settings_dialog_program_state->settings_state.motion_detection)
    {
        ui->motion_detection_checkbox->setCheckState(Qt::Checked);
    }

    switch (settings_dialog_program_state->settings_state.action_on_motion)
    {
        case PICTURE:
                ui->action_picture_rb->setChecked(true);
                break;
        case VIDEO:
                ui->action_video_rb->setChecked(true);
                break;
    }
    settings_dialog_program_state->mutex.unlock();

}

settings_dialog::~settings_dialog()
{
    delete ui;
}

void settings_dialog::on_back_btn_clicked()
{
    this->close();
}

void settings_dialog::on_pushButton_clicked()
{
    //handle radio buttons
    settings_dialog_program_state->mutex.lock();
    if(ui->action_picture_rb->isChecked())
    {
        settings_dialog_program_state->settings_state.action_on_motion = PICTURE;
    }
    else if (ui->action_video_rb->isChecked()) //redundant
    {
        settings_dialog_program_state->settings_state.action_on_motion = VIDEO;

    }

    if(ui->motion_detection_checkbox->checkState() == Qt::Checked)
    {
        settings_dialog_program_state->settings_state.motion_detection = true;
    }
    else
    {
        settings_dialog_program_state->settings_state.motion_detection = false;

    }
    settings_dialog_program_state->mutex.unlock();

    this->close();
}
