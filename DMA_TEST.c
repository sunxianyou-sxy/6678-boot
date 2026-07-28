
#include <stdio.h>
#include "math.h"
#include <c6x.h>
#include "DMA_TEST.h"
#include<c6x.h>

//DMA1  2017.08.05
#define  set_DCHMAP    0x02720100       //DMA通道映射寄存器
#define  set_DMAQNUM   0x02720240       //DMA逻辑通道事件队列设置寄存器
#define  set_ESR       0x02721010
#define  set_EMCR      0x02720308
#define  set_SECR      0x02721040
#define  set_OPT       0x02724000
#define  set_IESR      0x02721060         //使能中断位；
#define  set_IECR      0x02721058         //清零中断位；
#define  set_IPR       0x02721068         //中断挂起寄存器
#define  set_ICR       0x02721070        //清零挂起中断位


/*
//DMA0
#define  set_DCHMAP    0x02700100       //DMA通道映射寄存器
#define  set_DMAQNUM   0x02700240       //DMA逻辑通道事件队列设置寄存器
#define  set_ESR       0x02701010
#define  set_EMCR      0x02700308
#define  set_SECR      0x02701040
#define  set_OPT       0x02704000
#define  set_IESR      0x02701060         //使能中断位；
#define  set_IECR      0x02701058         //清零中断位；
#define  set_IPR       0x02701068         //中断挂起寄存器
#define  set_ICR       0x02701070        //清零挂起中断位
*/


void DMA_ParaConfig(int Numb ,int SourceAddr,int Acount,int Bcount,int DestinationAddr,int IntEnable)
{

	int  Bcount_tmp = Bcount << 16;
	int  Acount_tmp = Acount << 16;
	int  ABcount = Bcount_tmp | Acount;
	int  Excursion =  Acount_tmp | Acount;                  //将计数和偏移量算好
	int  Numb_addr = Numb *0x00000004 ;
	int  OPT_aar   = Numb *0x00000020 ;
	int  set_OPT_addr = set_OPT + OPT_aar ;
	int  Numb_TCC  = Numb <<12 ;
  //  int  OPT_worth = 0x00900000  |  Numb_TCC ;
	 int  OPT_worth = 0x00900004  |  Numb_TCC ;


	      *(int*)(set_DCHMAP + Numb_addr)= OPT_aar;
		  *(int*)set_DMAQNUM             = 0x00000000;
		  *(int*)set_OPT_addr            = OPT_worth;
		  *(int*)(set_OPT_addr+0x4)      = SourceAddr;
		  *(int*)(set_OPT_addr+0x8)      = ABcount;
		  *(int*)(set_OPT_addr+0xc)      = DestinationAddr;
		  *(int*)(set_OPT_addr+0x10)     = Excursion;
		  *(int*)(set_OPT_addr+0x14)     = 0X0001FFFF;
		  *(int*)(set_OPT_addr+0x18)     = 0X00010001;
		  *(int*)(set_OPT_addr+0x1c)     = 0X00010001;                  //将参数写入参数RAM


	  if(Numb <32)
	  *(int*)set_IESR        =  IntEnable;
	  else
	  *(int*)(set_IESR+0x4)  =  IntEnable;          //使能中断
	}



void DMA_Start(int Numb)
{
	if(Numb <32)
	{
		int ESR_worth = 1 << Numb;
	  *(int*)set_EMCR    = ESR_worth;
	  *(int*)set_SECR    = ESR_worth;
	  *(int*)set_ESR     = ESR_worth;              //使能DMA
	  }
	else
	{
		Numb = Numb - 32 ;
		int  ESR_worth = 1 << Numb;
	   *(int*)(set_EMCR+0x4)    = ESR_worth;
	   *(int*)(set_SECR+0x4)    = ESR_worth;
	   *(int*)(set_ESR+0x4)     = ESR_worth;              //使能DMA
	}

	}


int DMA_TransState(int Numb)
{
	int set_IPR_tmp = 0;

	if(Numb <32)
	{
		  int IPR_worth = 1 << Numb;
		  while( set_IPR_tmp != IPR_worth )
		  {
		    set_IPR_tmp = *(int*)set_IPR;
		    set_IPR_tmp = IPR_worth & set_IPR_tmp;
		}

		  *(int*)set_ICR    = IPR_worth;
	}

	else
	{
		Numb = Numb - 32 ;
	   int IPR_worth = 1 << Numb;
	    while( set_IPR_tmp != IPR_worth )
	   {
		    set_IPR_tmp = *(int*)(set_IPR+0X4);
		    set_IPR_tmp = IPR_worth & set_IPR_tmp;
		}

			*(int*)(set_ICR+0X4)    = IPR_worth;


	}
	 return  1 ;
}





