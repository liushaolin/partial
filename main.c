#include <iostm8s003f3.h>
#include <intrinsics.h>

#define uchar unsigned char 
#define uint  unsigned int 
#define ulong unsigned long

#define DS1302_RST  PC_ODR_ODR3
#define DS1302_DAO  PC_ODR_ODR4
#define DS1302_CLK  PC_ODR_ODR5
#define DS1302_DAI  PC_IDR_IDR4
#define DS1302_DIO  PC_DDR_DDR4

uchar KEY_DAT,TIM_SET_F,TIM_RUN_F,timer4,disp_timer;
uchar TIM_UIC,TIM_UIS,UIswitch;
uint  RT,sec;

uchar set_dat[36];
uchar set_SWF[12];
uchar DISP_CODE[4];
uchar DS1302_CODE[7];
uchar const DIG[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
uint tempRT[80]={
  923,918,913,907,901,895,888,882,875,868, //-19--10
  861,853,845,837,829,821,812,803,794,785, //-9-0
  775,766,755,746,735,725,715,704,693,682, //1-10
  671,660,649,637,626,615,603,592,580,569, //11-20
  557,546,534,523,512,500,489,478,467,456, //21-30
  445,435,424,414,403,393,383,373,364,354, //31-40
  345,336,327,318,310,301,293,285,277,269, //41-50
  262,255,247,240,234,227,221,214,208,198, //51-60
};

void io_init()                                                                  /*IO初始化*/
{
  PA_DDR=0X0E;
  PA_CR1=0X0E;
  PA_ODR=0X0E;
  
  PB_DDR=0X00;
  PB_CR1=0X30;
  PB_ODR=0X30;
  
  PC_DDR=0XF8;
  PC_CR1=0XF8;
  PC_ODR=0XF8;
  
  PD_DDR=0X5E;
  PD_CR1=0X5E;
  PD_ODR=0XA1;
}
void clock_init()                                                               /*系统时钟初始化*/
{
  CLK_ICKR|=0X01;                                                               //开内部高频时钟16MHZ
  while(!CLK_ICKR&0X02);                                                        //等待时钟稳定
  CLK_SWR=0XE1;                                                                 //切换时钟源 
  CLK_CKDIVR=0;                                                                 //无预分频 
}
void timer4_init()                                                              /*定时器4初始化*/
{
  TIM4_CR1=0X80;                                                                //自动重载
  TIM4_PSCR=0X07;                                                               //128分频     刷新频率60hz/32=521us
  TIM4_CNTR=64;                                                                 //16M/128=125KHZ=8us (64+1)*8=520us  
  TIM4_ARR=64;                                                                  //重载值
  TIM4_IER=0X01;                                                                //更新中断开
  TIM4_SR =0X00;                                                                //清中断标志位
  TIM4_EGR=0X00;                                                                //更新事件产生
  TIM4_CR1|=0X01;                                                               //开定时器
}
void adc_init()                                                                 /*ADC初始化*/    
{
  ADC_CR2=0X08;                                                                 //结果右对齐
  ADC_CR1=0X41;                                                                 //f_adc=f_cpu/8=2Mhz
  ADC_CSR=0X05;                                                                 //ADC通道5  
  ADC_TDRL=0X20;                                                                //关闭施密特触发 降低功耗
}
uint adc()                                                                      /*ADC转换*/
{
  uint dat=0;         
  ADC_CR1|=0X01;                                                                //开ADC转换
  while(!(ADC_CSR&0X80));                                                       //转换结束
  ADC_CSR&=0XEF;                                                                //清转换结束标志
  dat=ADC_DRL;                                                                  //读ADC低8位
  dat|=ADC_DRH<<8;                                                              //读ADC高8位  
  return dat;                                                                   //返回结果
}
uint Temperature_read()
{
  uint dat=0;
  unsigned long temp=0;
  uchar count;
  for(uchar i=0;i<32;i++)
    temp+=adc();
  temp>>=5;
  for(count=0;count<80;count++)
    if(temp>=tempRT[count])break;
  
  dat=(tempRT[count-1]-temp)*10/(tempRT[count-1]-tempRT[count]);
  dat=count*10+dat;
  return dat; 
}
void eeprom_init()                                                              /*EEPROM初始化*/
{
  do
  {
    FLASH_DUKR=0XAE;
    FLASH_DUKR=0X56;
  }
  while(!(FLASH_IAPSR&0X08));
}
void eeprom_write_4byte(uint add,uchar *dat)
{                                                                               /*EEPROM写入4字节*/ 
  FLASH_CR2|=0X40;                       
  FLASH_NCR2&=0XBF;
  for(uchar i=0;i<4;i++)
    *((uchar*)add+i)=*(dat+i);
  while(!(FLASH_IAPSR&0X04));
}
uchar eeprom_read(uint add)                                                     /*EEPROM读*/               
{
  uchar *addr;
  addr=(uchar*)(0x4000+add);
  return *addr;
}
void delay()                                                                    /*时钟芯片读写延时*/ 
{
  for(uchar i=10;i>0;i--)
    __no_operation();
}
void DS1302ByteWrite(uchar add,uchar dat)                                       /*写DS1302一个字节*/
{
  DS1302_RST=1;
  DS1302_DIO=1;
  for(uchar i=0;i<8;i++)
  {
    if(add&0x01)
      DS1302_DAO=1;
    else
      DS1302_DAO=0;
    DS1302_CLK=1;
    delay();
    DS1302_CLK=0;
    add>>=1;
  }
  for(uchar i=0;i<8;i++)
  {
    if(dat&0x01)
      DS1302_DAO=1;
    else
      DS1302_DAO=0;
    DS1302_CLK=1;
    delay();
    DS1302_CLK=0;
    dat>>=1;
  }
  DS1302_RST=0;
}
uchar DS1302ByteRead(uchar add)                                                 /*读出ds1302一个字节*/
{
  uchar dat=0;
  DS1302_RST=1;
  DS1302_DIO=1;
  for(uchar i=0;i<8;i++)
  {
    if(add&0x01)
      DS1302_DAO=1;
    else
      DS1302_DAO=0;
    DS1302_CLK=1;
    delay();
    DS1302_CLK=0;
    add>>=1;
  }
  DS1302_DIO=0;
  for(uchar i=0;i<8;i++)
  {
    dat>>=1;
    if(DS1302_DAI)
      dat|=0x80;
    DS1302_CLK=1;
    delay();
    DS1302_CLK=0;  
  }
  DS1302_RST=0;
  return dat;
}
void DS1302Write()                                                              /*写DS1302 时钟参数*/
{
  DS1302ByteWrite(0x8e,0x00);
  DS1302ByteWrite(0x80,0x80);
  for(uchar i=0;i<7;i++)
    DS1302ByteWrite((0x80+i*2),DS1302_CODE[i]);
  DS1302ByteWrite(0x8e,0x80);
}
void DS1302Read()                                                               /*读DS1302 时钟参数*/
{
  for(uchar i=0;i<7;i++)
    DS1302_CODE[i]=DS1302ByteRead(0x81+i*2);
}
void key_scan()                                                                 /*按键扫描*/
{
  static uchar key_temp,key_in;
  static uint delay_c;
  key_in=(PB_IDR&0X30)>>4;
  if(key_in<0X03)
  {
    if(key_in!=key_temp)
    {
      key_temp=key_in;
      delay_c=0;
    }
    else
    {
      delay_c++;
      if(delay_c==5)
      {
        switch(key_in)
        {
          case 0X01:KEY_DAT=2;break;
          case 0X02:KEY_DAT=1;break;
          default:break;
        }
      }
      if(delay_c>80)
      {
        if(delay_c%5==0&&key_in==0x01)
          KEY_DAT=12;
      }
      if(delay_c==80&&key_in==0x02)
        KEY_DAT=11;
    }
  }
  else
  {
    delay_c=0;
  }
}
void display_scan(uchar count)                                                  /*显示扫描*/
{
  uchar seg,com;
  PA_ODR=0XFF; 
  PC_ODR=0XFF;                                                                 
  PD_ODR&=0XE1;                                                                 //复位端口关闭显示
  if(count<31)
  {
    com=count/8;
    seg=count%8;
    switch(com)                                                                 //位扫描
    {
      case 0:PD_ODR_ODR1=1;break;
      case 1:PD_ODR_ODR2=1;break;
      case 2:PD_ODR_ODR3=1;break;
      case 3:PD_ODR_ODR4=1;break;
      default:break;
    }
    switch(seg)                                                                 //段扫描
    {
      case 0:if(DISP_CODE[com]&0X01)PC_ODR_ODR3=0;break;
      case 1:if(DISP_CODE[com]&0X02)PC_ODR_ODR5=0;break;
      case 2:if(DISP_CODE[com]&0X04)PC_ODR_ODR6=0;break;
      case 3:if(DISP_CODE[com]&0X08)PA_ODR_ODR2=0;break;
      case 4:if(DISP_CODE[com]&0X10)PA_ODR_ODR1=0;break;
      case 5:if(DISP_CODE[com]&0X20)PC_ODR_ODR4=0;break;
      case 6:if(DISP_CODE[com]&0X40)PC_ODR_ODR7=0;break;
      case 7:if(DISP_CODE[com]&0X80)PA_ODR_ODR3=0;break;
      default:break;
    }
  }
  else
  {
    key_scan();                                                                 //按键扫描    
    DS1302_RST=0;                                                               //DS1302数据存取
    DS1302_DAO=0;
    DS1302_CLK=0;
    if(TIM_SET_F==1)
    {
      DS1302Write();
      TIM_SET_F=0;
    }
    else
    {
      if(TIM_RUN_F==0)
      {
        DS1302Read();
        if(DS1302_CODE[0]&0x80)
        {
          DS1302_CODE[0]&=0x7F;
          DS1302ByteWrite(0x8e,0x00);
          DS1302ByteWrite(0x80,0x00);
          DS1302ByteWrite(0x8e,0x80);  
        }
      }
    }
    DS1302_DIO=1;
  }
}
uchar SET_dat()                                                                 /*数据设定*/  
{
  static uchar dat,max,min,tim_F,set_F,Project,dat_F,temp;
  if(KEY_DAT==11&&set_F==0)                                                     //非设定模式 则进入设模式
  {
    KEY_DAT=0;
    TIM_RUN_F=1;
    set_F=1;
    Project=0;
    tim_F=0;
    dat=tim_F;
    max=1;
    min=0;                                                                      //加载第一项数据
  }
  if(set_F==1)                                                                  //设定模式
  {
    if(KEY_DAT==1)
    {
      KEY_DAT=0;
      if(Project++==47)
        Project=0;
      if(Project==1&&tim_F==0)
        Project=8;
      if(Project==9&&set_SWF[0]==0)
        Project=13;
      if(Project==14&&set_SWF[1]==0)
        Project=18;
      if(Project==19&&set_SWF[2]==0)
        Project=23;
      if(Project==24&&set_SWF[3]==0)
        Project=28;
      if(Project==29&&set_SWF[4]==0)
        Project=33;
      if(Project==34&&set_SWF[5]==0)
        Project=38;
      if(Project==39&&set_SWF[6]==0)
        Project=43;
      if(Project==44&&set_SWF[7]==0)
        Project=0;
      switch(Project)                                                           //选择修改项 加载数据
      {
        case 0:dat=tim_F;max=1;min=0;break;
        case 1:dat=DS1302_CODE[1];max=59;min=0;break;
        case 2:dat=DS1302_CODE[2];max=23;min=0;break;
        case 3:dat=DS1302_CODE[3];max=31;min=1;break;
        case 4:dat=DS1302_CODE[4];max=12;min=1;break;
        case 5:dat=DS1302_CODE[6];max=99;min=0;break;  
        
        case 6:dat=set_dat[0];max=19;min=1;break;
        case 7:dat=set_dat[1];max=99;min=1;break;
        
        case  8:dat=set_SWF[0];max=1;min=0;break;
        case  9:dat=set_dat[2];max=59;min=0;break;
        case 10:dat=set_dat[3];max=23;min=0;break;
        case 11:dat=set_dat[4];max=59;min=0;break;
        case 12:dat=set_dat[5];max=23;min=0;break;
        
        case 13:dat=set_SWF[1];max=1;min=0;break;
        case 14:dat=set_dat[6];max=59;min=0;break;
        case 15:dat=set_dat[7];max=23;min=0;break;
        case 16:dat=set_dat[8];max=59;min=0;break;
        case 17:dat=set_dat[9];max=23;min=0;break;
        
        case 18:dat=set_SWF[2];max=1;min=0;break;
        case 19:dat=set_dat[10];max=59;min=0;break;
        case 20:dat=set_dat[11];max=23;min=0;break;
        case 21:dat=set_dat[12];max=59;min=0;break;
        case 22:dat=set_dat[13];max=23;min=0;break;
        
        case 23:dat=set_SWF[3];max=1;min=0;break;
        case 24:dat=set_dat[14];max=59;min=0;break;
        case 25:dat=set_dat[15];max=23;min=0;break;
        case 26:dat=set_dat[16];max=59;min=0;break;
        case 27:dat=set_dat[17];max=23;min=0;break;
        
        case 28:dat=set_SWF[4];max=1;min=0;break;
        case 29:dat=set_dat[18];max=59;min=0;break;
        case 30:dat=set_dat[19];max=23;min=0;break;
        case 31:dat=set_dat[20];max=59;min=0;break;
        case 32:dat=set_dat[21];max=23;min=0;break;
        
        case 33:dat=set_SWF[5];max=1;min=0;break;
        case 34:dat=set_dat[22];max=59;min=0;break;
        case 35:dat=set_dat[23];max=23;min=0;break;
        case 36:dat=set_dat[24];max=59;min=0;break;
        case 37:dat=set_dat[25];max=23;min=0;break;
        
        case 38:dat=set_SWF[6];max=1;min=0;break;
        case 39:dat=set_dat[26];max=59;min=0;break;
        case 40:dat=set_dat[27];max=23;min=0;break;
        case 41:dat=set_dat[28];max=59;min=0;break;
        case 42:dat=set_dat[29];max=23;min=0;break;
        
        case 43:dat=set_SWF[7];max=1;min=0;break;
        case 44:dat=set_dat[30];max=59;min=0;break;
        case 45:dat=set_dat[31];max=23;min=0;break;
        case 46:dat=set_dat[32];max=59;min=0;break;
        case 47:dat=set_dat[33];max=23;min=0;break;
        
        default:break;
      }
    }
    if(Project<6||Project>7)
      dat=dat/16*10+dat%16;
    if(KEY_DAT==2||KEY_DAT==12)
    {
      KEY_DAT=0; 
      dat++;                                                                    //数值加1
      if(dat>max)
        dat=min;
    }
    if(Project<6||Project>7)
      dat=dat/10*16+dat%10;
    if(dat_F!=dat)                                                              //数值发生改变
    {
      switch(Project)                                                           //返回改变后的数值
      {
        case 0:tim_F=dat;break;
        case 1:DS1302_CODE[1]=dat;break;
        case 2:DS1302_CODE[2]=dat;break;
        case 3:DS1302_CODE[3]=dat;break;
        case 4:DS1302_CODE[4]=dat;break;
        case 5:DS1302_CODE[6]=dat;break;
        
        case 6:set_dat[0]=dat;break;
        case 7:set_dat[1]=dat;break;
        
        case 8:set_SWF[0]=dat;break;
        case 9:set_dat[2]= dat;break;
        case 10:set_dat[3]=dat;break;
        case 11:set_dat[4]=dat;break;
        case 12:set_dat[5]=dat;break;
        
        case 13:set_SWF[1]=dat;break;
        case 14:set_dat[6]=dat;break;
        case 15:set_dat[7]=dat;break;
        case 16:set_dat[8]=dat;break;
        case 17:set_dat[9]=dat;break;
        
        case 18:set_SWF[2]=dat;break;
        case 19:set_dat[10]=dat;break;
        case 20:set_dat[11]=dat;break;
        case 21:set_dat[12]=dat;break;
        case 22:set_dat[13]=dat;break;
        
        case 23:set_SWF[3]=dat;break;
        case 24:set_dat[14]=dat;break;
        case 25:set_dat[15]=dat;break;
        case 26:set_dat[16]=dat;break;
        case 27:set_dat[17]=dat;break;
        
        case 28:set_SWF[4]=dat;break;
        case 29:set_dat[18]=dat;break;
        case 30:set_dat[19]=dat;break;
        case 31:set_dat[20]=dat;break;
        case 32:set_dat[21]=dat;break;
        
        case 33:set_SWF[5]=dat;break;
        case 34:set_dat[22]=dat;break;
        case 35:set_dat[23]=dat;break;
        case 36:set_dat[24]=dat;break;
        case 37:set_dat[25]=dat;break;
        
        case 38:set_SWF[6]=dat;break;
        case 39:set_dat[26]=dat;break;
        case 40:set_dat[27]=dat;break;
        case 41:set_dat[28]=dat;break;
        case 42:set_dat[29]=dat;break;
        
        case 43:set_SWF[7]=dat;break;
        case 44:set_dat[30]=dat;break;
        case 45:set_dat[31]=dat;break;
        case 46:set_dat[32]=dat;break;
        case 47:set_dat[33]=dat;break;
        
        default:break;
      }
      dat_F=dat;
      disp_timer=0;
    } 
    if(Project>7)
    {
      switch((Project-8)%5)
      {
        case 0:{
          DISP_CODE[0]=0X73;
          DISP_CODE[1]=DIG[(Project-8)/5+1]|0X80;                                                        
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[0];                                                //O
            if(set_SWF[(Project-8)/5]==1)
              DISP_CODE[3]=0x37;                                                //N
            else
              DISP_CODE[3]=0x71;                                                //F
          }
          else
          {
            DISP_CODE[2]=0;
            DISP_CODE[3]=0;
          }
        }break;
        case 1:{
          temp=Project-7-(Project-8)/5;
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[set_dat[temp]>>4];
            DISP_CODE[3]=DIG[set_dat[temp]&0X0F];
          }
          else
          {
            DISP_CODE[2]=0x01;
            DISP_CODE[3]=0x01;
          }
          DISP_CODE[0]=DIG[set_dat[temp+1]>>4];
          DISP_CODE[1]=DIG[set_dat[temp+1]&0X0F]|0X80;
        }break;
        case 2:{
          temp=Project-7-(Project-8)/5;
          if(disp_timer<30)
          {
            DISP_CODE[0]=DIG[set_dat[temp]>>4];
            DISP_CODE[1]=DIG[set_dat[temp]&0X0F]|0x80;
          }
          else
          {
            DISP_CODE[0]=0x01;
            DISP_CODE[1]=0x81;
          }
          DISP_CODE[2]=DIG[set_dat[temp-1]>>4];
          DISP_CODE[3]=DIG[set_dat[temp-1]&0X0F];
        }break;
        case 3:{
          temp=Project-7-(Project-8)/5;
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[set_dat[temp]>>4];
            DISP_CODE[3]=DIG[set_dat[temp]&0X0F];
          }
          else
          {
            DISP_CODE[2]=0x08;
            DISP_CODE[3]=0x08;
          }
          DISP_CODE[0]=DIG[set_dat[temp+1]>>4];
          DISP_CODE[1]=DIG[set_dat[temp+1]&0X0F]|0X80;
        }break;
        case 4:{
          temp=Project-7-(Project-8)/5;
          if(disp_timer<30)
          {
            DISP_CODE[0]=DIG[set_dat[temp]>>4];
            DISP_CODE[1]=DIG[set_dat[temp]&0X0F]|0x80;
          }
          else
          {
            DISP_CODE[0]=0x08;
            DISP_CODE[1]=0x88;
          }
          DISP_CODE[2]=DIG[set_dat[temp-1]>>4];
          DISP_CODE[3]=DIG[set_dat[temp-1]&0X0F];
        }break;
        default:break;
      }
    }
    else
    {
      switch(Project)
      {
        case 0:{
          DISP_CODE[0]=0X73;
          DISP_CODE[1]=0XF9;                                                        
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[0];                                                  
            if(tim_F==1)
              DISP_CODE[3]=0x37;                                                  
            else
              DISP_CODE[3]=0x71;                                                  
          }
          else
          {
            DISP_CODE[2]=0;
            DISP_CODE[3]=0;
          }
        }break;
        case 1:{
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[DS1302_CODE[1]>>4];
            DISP_CODE[3]=DIG[DS1302_CODE[1]&0x0F];
          }
          else
          {
            DISP_CODE[2]=0x00;
            DISP_CODE[3]=0x00;
          }
          DISP_CODE[0]=DIG[DS1302_CODE[2]>>4];
          DISP_CODE[1]=DIG[DS1302_CODE[2]&0x0F]|0x80;
        }break;
        case 2:{
          if(disp_timer<30)
          {
            DISP_CODE[0]=DIG[DS1302_CODE[2]>>4];
            DISP_CODE[1]=DIG[DS1302_CODE[2]&0x0F]|0x80;
          }
          else
          {
            DISP_CODE[0]=0x00;
            DISP_CODE[1]=0x80;
          }
          DISP_CODE[2]=DIG[DS1302_CODE[1]>>4];
          DISP_CODE[3]=DIG[DS1302_CODE[1]&0x0F];
        }break;
        case 3:{
          if(disp_timer<30)
          {
            DISP_CODE[2]=DIG[DS1302_CODE[3]>>4];
            DISP_CODE[3]=DIG[DS1302_CODE[3]&0x0F];
          }
          else
          {
            DISP_CODE[2]=0x00;
            DISP_CODE[3]=0x00;
          }
          DISP_CODE[0]=DIG[DS1302_CODE[4]>>4]|0X80;
          DISP_CODE[1]=DIG[DS1302_CODE[4]&0x0F];
        }break;
        case 4:{
          if(disp_timer<30)
          {
            DISP_CODE[0]=DIG[DS1302_CODE[4]>>4]|0X80;
            DISP_CODE[1]=DIG[DS1302_CODE[4]&0x0F];
          }
          else
          {
            DISP_CODE[0]=0x80;
            DISP_CODE[1]=0x00;
          }
          DISP_CODE[2]=DIG[DS1302_CODE[3]>>4];
          DISP_CODE[3]=DIG[DS1302_CODE[3]&0x0F];
        }break;
        case 5:{
          if(disp_timer<30)
          {
            DISP_CODE[0]=DIG[2];
            DISP_CODE[1]=DIG[0];
            DISP_CODE[2]=DIG[DS1302_CODE[6]>>4];
            DISP_CODE[3]=DIG[DS1302_CODE[6]&0x0F];
          }
          else
          {
            DISP_CODE[0]=0X00;
            DISP_CODE[1]=0x00;
            DISP_CODE[2]=0x00;
            DISP_CODE[3]=0x00;
          }
        }break;
        case 6:{
          if(disp_timer<30)
          {
            if(set_dat[0]<10)
            {
              DISP_CODE[0]=0X40;
              DISP_CODE[1]=DIG[10-set_dat[0]];
            }
            else
            {
              DISP_CODE[0]=0X00;
              DISP_CODE[1]=DIG[set_dat[0]-10];
            }
          }
          else
          {
            DISP_CODE[0]=0X00;
            DISP_CODE[1]=0x00;
          }
          DISP_CODE[2]=0x00;
          DISP_CODE[3]=DIG[5];
        }break;
        case 7:{
          if(disp_timer<30)
          {
            if(set_dat[1]<50)
            {
              DISP_CODE[0]=0XC0;
              DISP_CODE[1]=DIG[(50-set_dat[1])/10];
              DISP_CODE[2]=DIG[(50-set_dat[1])%10];
            }
            else
            {
              DISP_CODE[0]=0X80;
              DISP_CODE[1]=DIG[(set_dat[1]-50)/10];
              DISP_CODE[2]=DIG[(set_dat[1]-50)%10];
            }
          }
          else
          {
            DISP_CODE[0]=0X00;
            DISP_CODE[1]=0x00;
            DISP_CODE[2]=0x00;
          }
          DISP_CODE[3]=0X58;
        }break;
        default:break;
      }
    }
    if(KEY_DAT==11&&set_F==1)                                                   //退出保存
    {
      KEY_DAT=0;
      set_F=0;
      if(tim_F==1)
      {
        TIM_SET_F=1;
        DS1302_CODE[0]=0;
      }
      for(uchar i=0;i<9;i++)
        eeprom_write_4byte(0x4000+i*4,set_dat+i*4); 
      eeprom_write_4byte(0x4040,set_SWF);
      eeprom_write_4byte(0x4044,set_SWF+4);
      TIM_RUN_F=0;
    }
  }
  return set_F;
}
void HoursMinute()
{ 
  static uchar Point;
  if(TIM_UIS)
  {
    TIM_UIS=0;
    Point^=0x80;
  }
  DISP_CODE[0]=DIG[DS1302_CODE[2]>>4];
  DISP_CODE[1]=DIG[DS1302_CODE[2]&0x0f]|(Point&0x80);
  DISP_CODE[2]=DIG[DS1302_CODE[1]>>4];
  DISP_CODE[3]=DIG[DS1302_CODE[1]&0x0f];     
}
void MonthDay()
{
  DISP_CODE[0]=DIG[DS1302_CODE[4]>>4]|0x80;
  DISP_CODE[1]=DIG[DS1302_CODE[4]&0x0f];
  DISP_CODE[2]=DIG[DS1302_CODE[3]>>4];
  DISP_CODE[3]=DIG[DS1302_CODE[3]&0x0f];  
}
void Seconds()
{
  DISP_CODE[0]=0x40;
  DISP_CODE[1]=0xC0;
  DISP_CODE[2]=DIG[DS1302_CODE[0]>>4];
  DISP_CODE[3]=DIG[DS1302_CODE[0]&0x0f];  
}
void Temperature()
{
  uint dat;
  dat=RT+set_dat[1]-50;
  if(dat>799||dat==0)
  {
    for(uchar i=0;i<3;i++)
      DISP_CODE[i]=0x71;
  }
  else
  {
    if(TIM_UIS==1)
    {
      TIM_UIS=0;
      if(dat<200)
      {
        if(dat<101)
        {
          DISP_CODE[0]=0x40;
          dat=(200-dat)/10;
        }
        else
        {
          DISP_CODE[0]=0xC0;
          dat=200-dat;
        }
      }
      else
      {
        dat=dat-200;
        if(dat/100==0)
          DISP_CODE[0]=0x80;
        else
          DISP_CODE[0]=DIG[dat/100]|0x80;  
      }
      DISP_CODE[1]=DIG[dat%100/10];
      DISP_CODE[2]=DIG[dat%10];
    }
  }
  DISP_CODE[3]=0x58;  
}
void Temper()
{
  static uchar i=0;
  static ulong temp=0;
  if(i==128)
  {
    RT=temp>>7;
    temp=0;
    i=0;
  }
  else
  {
    temp+=Temperature_read();
    i++;
  }
}
void timer_check()
{ 
  static uchar tc=0;
  uchar temp;
  if(DS1302_CODE[2]==0x12&&DS1302_CODE[1]==0x00)
  {
    if(DS1302_CODE[0]==0x50&&tc==1)
      tc=0;
    if(DS1302_CODE[0]==0x30&&tc==0)
    {
      tc=1;
      temp=(DS1302_CODE[0]/16*10+DS1302_CODE[0]%16)+set_dat[0]-10;
      DS1302_CODE[0]=temp/10*16+temp%10;
      TIM_SET_F=1;
    }
  }
}
void CON_main()
{
  static uchar UI_DF=0;
  if(SET_dat()==0)
  { 
    Temper();
    timer_check();
    
    if(KEY_DAT==1)
    {
      KEY_DAT=0;
      PD_ODR&=0XBF;
    }
    if(KEY_DAT==2)
    {
      KEY_DAT=0;
      if(UIswitch++==3)
        UIswitch=0;
      set_SWF[8]=UIswitch;
      eeprom_write_4byte(0x4048,set_SWF+8);
      DISP_CODE[0]=0x3E;
      DISP_CODE[1]=0xB0;
      DISP_CODE[2]=DIG[0];
      DISP_CODE[3]=DIG[UIswitch+1];  
      UI_DF=1;
      TIM_UIC=0;
    }
    if(UI_DF)
    {
      if(TIM_UIC>2)
      {
        TIM_UIC=0;
        UI_DF=0;
      }
    }
    else
    {
      switch (UIswitch)
      {
        case 0:{
          if(TIM_UIC<5)
            HoursMinute();
          if(TIM_UIC>4&&TIM_UIC<7)
            MonthDay();
          if(TIM_UIC>6)
            Temperature();
          if(TIM_UIC>8)
            TIM_UIC=0;
        }break;
        case 1:{
          if(TIM_UIC<5)
            HoursMinute();
          else
            Temperature();
          if(TIM_UIC>6)
            TIM_UIC=0;
        }break;
        case 2: HoursMinute(); break;
        case 3: Seconds();break;
        default:break;
      }
      if((DS1302_CODE[1]==set_dat[2]&&DS1302_CODE[2]==set_dat[3]&&set_SWF[0]==1)||\
        (DS1302_CODE[1]==set_dat[6] &&DS1302_CODE[2]==set_dat[7]&&set_SWF[1]==1)||\
        (DS1302_CODE[1]==set_dat[10]&&DS1302_CODE[2]==set_dat[11]&&set_SWF[2]==1)||\
        (DS1302_CODE[1]==set_dat[14]&&DS1302_CODE[2]==set_dat[15]&&set_SWF[3]==1)||\
        (DS1302_CODE[1]==set_dat[18]&&DS1302_CODE[2]==set_dat[19]&&set_SWF[4]==1)||\
        (DS1302_CODE[1]==set_dat[22]&&DS1302_CODE[2]==set_dat[23]&&set_SWF[5]==1)||\
        (DS1302_CODE[1]==set_dat[26]&&DS1302_CODE[2]==set_dat[27]&&set_SWF[6]==1)||\
        (DS1302_CODE[1]==set_dat[30]&&DS1302_CODE[2]==set_dat[31]&&set_SWF[7]==1))
        PD_ODR|=0X40;
      if((DS1302_CODE[1]==set_dat[4]&&DS1302_CODE[2]==set_dat[5])||\
        (DS1302_CODE[1]==set_dat[8] &&DS1302_CODE[2]==set_dat[9])||\
        (DS1302_CODE[1]==set_dat[12]&&DS1302_CODE[2]==set_dat[13])||\
        (DS1302_CODE[1]==set_dat[16]&&DS1302_CODE[2]==set_dat[17])||\
        (DS1302_CODE[1]==set_dat[20]&&DS1302_CODE[2]==set_dat[21])||\
        (DS1302_CODE[1]==set_dat[24]&&DS1302_CODE[2]==set_dat[25])||\
        (DS1302_CODE[1]==set_dat[28]&&DS1302_CODE[2]==set_dat[29])||\
        (DS1302_CODE[1]==set_dat[32]&&DS1302_CODE[2]==set_dat[33]))
        PD_ODR&=0XBF;
    }
  }
}
void main_init()
{
  clock_init();
  io_init();
  timer4_init();
  eeprom_init();
  adc_init();
  
  for(uchar i=0;i<36;i++)
    set_dat[i]=eeprom_read(i);  
  for(uchar i=0;i<12;i++)
    set_SWF[i]=eeprom_read(0x40+i);  
  UIswitch=set_SWF[8];
  
  CPU_CFG_GCR_SWO=1;
  
  IWDG_KR=0XCC;
  IWDG_KR=0X55;
  IWDG_PR=0X06;
  IWDG_RLR=0XFF;
  IWDG_KR=0XAA;
  
  asm("rim");
}
int main( void )
{
  main_init();
  while(1)
  {
    IWDG_KR=0XAA;
    CON_main();
  }
}
#pragma vector=TIM4_OVR_UIF_vector
__interrupt void timer4_UIF_isr(void)
{
  TIM4_SR=0;
  if(timer4++==31)
  {
    timer4=0;
    if(disp_timer++==59)
    {
      disp_timer=0;
      TIM_UIC++;
      sec++;
    }
    if(disp_timer%30==0)
      TIM_UIS=1;
  }
  display_scan(timer4);
}