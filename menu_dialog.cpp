#include "menu_dialog.h"
#include "ui_menu_dialog.h"

menu_dialog::menu_dialog(QWidget *parent, program_state *my_program_state) :
    QDialog(parent),
    ui(new Ui::menu_dialog)
{
    ui->setupUi(this);

    menu_dialog_program_state = my_program_state;
}

menu_dialog::~menu_dialog()
{
    delete ui;
}

void menu_dialog::on_back_btn_clicked()
{
    close();
}

void menu_dialog::on_setting_btn_clicked()
{
    settings_dialog my_settings_dialog(this, menu_dialog_program_state);
    my_settings_dialog.setWindowState(Qt::WindowFullScreen);
    my_settings_dialog.exec();

}

void menu_dialog::on_gallery_btn_clicked()
{
    gallery_dialog my_gallery_dialog(this, menu_dialog_program_state); //we don't know what it will need (yet) so we pass everything
    my_gallery_dialog.setWindowState(Qt::WindowFullScreen);
    my_gallery_dialog.exec();
}

void menu_dialog::on_picture_gallery_button_clicked()
{

    picture_gallery my_picture_gallery(this, menu_dialog_program_state); //we don't know what it will need (yet) so we pass everything
    my_picture_gallery.setWindowState(Qt::WindowFullScreen);
    my_picture_gallery.exec();
}
