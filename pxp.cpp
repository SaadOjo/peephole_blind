#include "pxp.h"

PxP::PxP()
{
}
PxP::~PxP()
{
}
int PxP::start()
{
    if (!(pxp = pxp_init()))
        return 1;

    if (pxp_check_capabilities(pxp))
        return 1;

    if (pxp_config_output(pxp))
        return 1;

    if (pxp_config_buffer(pxp))
        return 1;
/*
    if (pxp_config_windows(pxp))
        return 1;
*/
    if (pxp_config_controls(pxp))
        return 1;

 //   if (start_pxp_convert(pxp))
  //     return 1;
}

int PxP::cc_frame_start(unsigned char *image, int size)
{
    if(pxp_convert_frame_start(pxp,image,size))
    {
    return 1;
    }
    return 0;
    //if (pxp_start(pxp,image,size))
    //    return 1;
}
int PxP::cc_frame_end()
{
    if(pxp_convert_frame_end(pxp))
    {
    return 1;
    }
    return 0;
}

int PxP::stop(unsigned char* ccd_frame)
{
    if (pxp_stop(pxp))
        return 1;

    if (pxp->outfile_state)
        if (pxp_write_outfile(pxp,ccd_frame))
            return 1;

    pxp_cleanup(pxp);

    qDebug("end of everything!");
}

int PxP::pxp_test()
{
    struct pxp_control *pxp;


    if (!(pxp = pxp_init()))
        return 1;


    if (pxp_check_capabilities(pxp))
        return 1;


    if (pxp_config_output(pxp))
        return 1;

    if (pxp_config_buffer(pxp))
        return 1;

  //if (pxp_read_infiles(pxp))
  //    return 1;

    if (pxp_config_windows(pxp))
        return 1;

    if (pxp_config_controls(pxp))
        return 1;

    if (pxp_start(pxp,NULL,0))
        return 1;

    if (pxp_stop(pxp))
        return 1;
    qDebug("2");

    if (pxp->outfile_state)
        if (pxp_write_outfile(pxp,NULL))
            return 1;
    qDebug("3");

    pxp_cleanup(pxp);

    qDebug("end of everything!");

}

