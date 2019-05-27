#ifndef MENU_DIALOG_H
#define MENU_DIALOG_H

#include <QDialog>
#include <QDebug>

#include "program_state.h"
#include "gallery_dialog.h"

namespace Ui {
class menu_dialog;
}

class menu_dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit menu_dialog(QWidget *parent = 0, program_state *my_program_state = NULL);
    ~menu_dialog();
    
private slots:
    void on_back_btn_clicked();

    void on_setting_btn_clicked();

    void on_gallery_btn_clicked();

private:
    Ui::menu_dialog *ui;
    program_state *menu_dialog_program_state;
};

#endif // MENU_DIALOG_H
