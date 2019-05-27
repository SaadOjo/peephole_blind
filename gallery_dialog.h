#ifndef GALLERY_DIALOG_H
#define GALLERY_DIALOG_H

#include <QDialog>
#include <QDebug>
#include <QDir>
#include "player_dialog.h"
#include "program_state.h"

namespace Ui {
class gallery_dialog;
}

class gallery_dialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit gallery_dialog(QWidget *parent = 0, program_state *my_program_state = NULL);
    ~gallery_dialog();
    
private slots:
    void on_back_btn_clicked();

    void on_play_btn_clicked();

private:
    Ui::gallery_dialog *ui;
    program_state *gallery_dialog_program_state;
};

#endif // GALLERY_DIALOG_H
