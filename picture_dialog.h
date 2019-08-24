#ifndef PICTURE_DIALOG_H
#define PICTURE_DIALOG_H

#include <QDialog>
#include <QDebug>
#include "program_state.h"
#include "structures.h"


namespace Ui {
class picture_dialog;
}

class picture_dialog : public QDialog
{
    Q_OBJECT

public:
    explicit picture_dialog(QWidget *parent = 0, program_state *my_program_state = NULL);
    ~picture_dialog();

signals:
    void setImage(image_with_mutex* IWM);

private slots:

    void on_back_btn_clicked();

private:
    program_state *picture_dialog_program_state;
    Ui::picture_dialog *ui;
};

#endif // PICTURE_DIALOG_H
