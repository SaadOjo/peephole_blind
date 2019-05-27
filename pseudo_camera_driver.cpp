#include "pseudo_camera_driver.h"

pseudo_camera_driver::pseudo_camera_driver()
{
}
pseudo_camera_driver::~pseudo_camera_driver()
{
}
int pseudo_camera_driver::init()
{
    sprintf(filename,"/dev/i2c-1");
    if ((file = open(filename,O_RDWR)) < 0) {
        qDebug("Failed to open the bus.");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return 1;
    }
    fprintf(stderr, "Sucessfully opened i2c device.\n");

    if (ioctl(file,I2C_SLAVE_FORCE,0x30) < 0) {
        printf("Failed to acquire bus access and/or talk to slave.\n");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return 1;
    }
    fprintf(stderr, "Sucessfully initialized address.\n");
    return 0;
}

int pseudo_camera_driver::read_register(int reg)
{

    char buf[10] = {0};
    buf[0] = reg & 0xFF;
    //buf[1] = 0x00;

    if (write(file,buf,1) != 1) {
        printf("Failed to write to the i2c bus.\n");
    }

    if (read(file,buf,1) != 1) {
        printf("Failed to read from the i2c bus.\n");
    }
    fprintf(stderr,"ADDR:%02X, VAL: %02X\n",reg, buf[0]);

    return buf[0];

}

int pseudo_camera_driver::write_register(int reg, int val)
{
    char buf[10] = {0};
    buf[0] = reg & 0xFF;
    buf[1] = val & 0xFF;


    if (write(file,buf,2) != 2) {
        printf("Failed to write to the i2c bus.\n");
    }

    fprintf(stderr,"ADDR:%02X, VAL: %02X\n",reg, val);
}
int pseudo_camera_driver::store_current_settings_to_file()
{
    unsigned char settings[171];
    FILE *ptr;

    if(!init())
    {
        for(int i = 0; i<171; i++)
        {
            settings[i] = read_register(i);
        }
    }

    ptr = fopen("camera_config.bin","wb");
    fwrite(settings,sizeof(settings),1,ptr);
    fclose(ptr);

}
int pseudo_camera_driver::load_settings_from_file()
{
    unsigned char settings[171];
    FILE *ptr;

    ptr = fopen("camera_config.bin","rb");
    fread(settings,sizeof(settings),1,ptr);
    fclose(ptr);

    if(!init())
    {
        for(int i = 0; i<171; i++)
        {
            write_register(i,settings[i]);
        }
    }
    return 0;
}

int pseudo_camera_driver::set_camera()
{
    int adr[] = {0x04 , 0x0c, 0x0d, 0x11, 0x12, 0x37, 0x38, 0x39};
    int set[] = {0x00 , 0x04, 0x80, 0x83, 0x10, 0x91, 0x12, 0x43};

  if(!init())
  {
      for(int i = 0; i<8; i++)
      {
          write_register(adr[i],set[i]);
      }
  }
}

int pseudo_camera_driver::print_settings()
{
    int adr[] = {0x04 , 0x0c, 0x0d, 0x11, 0x12, 0x37, 0x38, 0x39};

  if(!init())
  {
      for(int i = 0; i<8; i++)
      {
          read_register(adr[i]);
      }
  }
}
