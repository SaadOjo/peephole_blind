#ifndef PICTURE_GALLERY_H
#define PICTURE_GALLERY_H

#include <QDialog>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include "program_state.h"
#include "picture_dialog.h"

namespace Ui {
class picture_gallery;
}

class picture_gallery : public QDialog
{
    Q_OBJECT
    
public:
    explicit picture_gallery(QWidget *parent = 0, program_state *my_program_state = NULL);
    ~picture_gallery();
    
private slots:
    void on_play_btn_clicked();

    void on_back_btn_clicked();

    void on_delete_btn_clicked();

private:
    void populate_list();
    program_state *picture_gallery_program_state;
    Ui::picture_gallery *ui;
};

#endif // PICTURE_GALLERY_H
