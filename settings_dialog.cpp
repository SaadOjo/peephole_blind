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
}

settings_dialog::~settings_dialog()
{
    delete ui;
}

void settings_dialog::on_back_btn_clicked()
{
    this->close();
}
