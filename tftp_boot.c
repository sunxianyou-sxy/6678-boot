/**

 *
 * @version V1.0
 *
TFTP_BOOT
 *
 **/
/* C standard Header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* XDCtools Header files */
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>

/* BIOS6 include */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/hal/Cache.h>

/* NDK include */
#include <ti/ndk/inc/netmain.h>
#include <ti/ndk/inc/_stack.h>
#include <ti/ndk/inc/tools/console.h>
#include <ti/ndk/inc/tools/servers.h>

/* csl include */
#include <ti/csl/csl_psc.h>
#include <ti/csl/csl_pscAux.h>
#include <ti/csl/csl_chipAux.h>
#include <ti/csl/csl_bootcfgAux.h>
#include <ti/csl/csl_mdio.h>
#include <ti/csl/csl_mdioAux.h>
#include <ti/csl/csl_cacheAux.h>
#include <ti/csl/csl_cache.h>

#ifdef C66_PLATFORMS
#include <ti/csl/csl_cpsgmii.h>
#include <ti/csl/csl_cpsgmiiAux.h>
#endif
#include "system/resource_mgr.h"

#include <ti/csl/cslr_device.h>
#include <ti/ndk/inc/tools/console.h>
#include "c66x_uart.h"
#include "DMA_TEST.h"



// CRC32表
static uint32_t crc32_table[256];

// 初始化CRC32表
int i,j;
void init_crc32_table() {
    uint32_t crc;
    for (i = 0; i < 256; i++) {
        crc = i;
        for ( j = 8; j > 0; j--) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            }
            else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
}

// 计算CRC32值
uint32_t calculate_crc32(const char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for ( i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}




/* Common phy register */
#define PHY_IDENTIFIER_1_REG                    2
#define PHY_IDENTIFIER_2_REG                    3

/* Phy identification */
#define PHY_MOTORCOMM_IDENTIFIER                0x0
#define PHY_MOTORCOMM_MANUFACTURER              0x011a

/* YT8521SH phy register */
#define YT8521SH_EXT_REG_ADDR_OFFSET_REG        0x1E
#define YT8521SH_EXT_REG_DATA_REG               0x1F
#define YT8521SH_EXT_LED2_CFG                   0xA00E
#define YT8521SH_EXT_LED1_CFG                   0xA00D



#define KICK0   (*((volatile unsigned int *)(0x02620038)))
#define KICK1   (*((volatile unsigned int *)(0x0262003C)))

#define KICK0_UNLOCK 	(0x83E70B13)
#define KICK1_UNLOCK 	(0x95A4F1E0)
#define KICK_LOCK 			(0x1)

// CorePac0-7 IPC 触发寄存器（IPCGRx）
#define  IPCGR_0_REGS			(*((volatile unsigned int *)(0x02620240)))
#define  IPCGR_1_REGS			(*((volatile unsigned int *)(0x02620244)))
#define  IPCGR_2_REGS			(*((volatile unsigned int *)(0x02620248)))
#define  IPCGR_3_REGS			(*((volatile unsigned int *)(0x0262024C)))
#define  IPCGR_4_REGS			(*((volatile unsigned int *)(0x02620250)))
#define  IPCGR_5_REGS			(*((volatile unsigned int *)(0x02620254)))
#define  IPCGR_6_REGS			(*((volatile unsigned int *)(0x02620258)))
#define  IPCGR_7_REGS			(*((volatile unsigned int *)(0x0262025C)))

// 核0~核7 魔术地址（L2地址后4字节）
#define CORE_0_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1087FFFC)))
#define CORE_1_MAGIC_ADDR   	(*((volatile unsigned int *)(0X1187FFFC)))
#define CORE_2_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1287FFFC)))
#define CORE_3_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1387FFFC)))
#define CORE_4_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1487FFFC)))
#define CORE_5_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1587FFFC)))
#define CORE_6_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1687FFFC)))
#define CORE_7_MAGIC_ADDR  		(*((volatile unsigned int *)(0X1787FFFC)))




/*  UDP tftp port id */
#define TFTP_PORT     69



char *HostName             = "tidsp";
char *LocalIPAddr          = "192.168.2.33";            // Local device Ip addr
char *REMOTE_IPADDR_STRING = "192.168.2.101";           // Remote device IP addr
char *LocalIPMask          = "255.255.255.0";           // Not used when using DHCP
char *GatewayIP            = "0.0.0.0";                 // Not used when using DHCP
char *DomainName           = "demo.net";                // Not used when using DHCP
char *DNSServer            = "0.0.0.0";                 // Used when set to anything but zero


/**
 * @brief enable psc module
 *
 * @param void
 *
 * @return NULL
 */
void psc_init()
{
    /* Set psc as Always on state */
    CSL_PSC_enablePowerDomain(CSL_PSC_PD_ALWAYSON);

#ifdef C665_PLATFORMS
    /* Enable EMAC Clock*/
    CSL_PSC_setModuleNextState (CSL_PSC_LPSC_EMAC_SS, PSC_MODSTATE_ENABLE);
#endif

    /* Start state change */
    CSL_PSC_startStateTransition(CSL_PSC_PD_ALWAYSON);

    /* Wait until the status change is completed */
    while(!CSL_PSC_isStateTransitionDone(CSL_PSC_PD_ALWAYSON));

#ifdef C66_PLATFORMS
    /* PASS power domain is turned OFF by default. It needs to be turned on before doing any
     * PASS device register access. This not required for the simulator. */

    /* Set PASS Power domain to ON */
    CSL_PSC_enablePowerDomain (CSL_PSC_PD_PASS);

    /* Enable the clocks for PASS modules */
    CSL_PSC_setModuleNextState (CSL_PSC_LPSC_PKTPROC, PSC_MODSTATE_ENABLE);
    CSL_PSC_setModuleNextState (CSL_PSC_LPSC_CPGMAC, PSC_MODSTATE_ENABLE);
    CSL_PSC_setModuleNextState (CSL_PSC_LPSC_Crypto, PSC_MODSTATE_ENABLE);

    /* Start the state transition */
    CSL_PSC_startStateTransition (CSL_PSC_PD_PASS);

    /* Wait until the state transition process is completed. */
    while (!CSL_PSC_isStateTransitionDone (CSL_PSC_PD_PASS));
#endif
}

#ifdef C66_PLATFORMS
void init_sgmii (uint32_t macPortNum)
{
    int32_t wait_time;
    CSL_SGMII_ADVABILITY    sgmiiCfg;
    CSL_SGMII_STATUS        sgmiiStatus;

    /* Configure the SERDES, set MPY as 10x mode */
    CSL_BootCfgSetSGMIIConfigPLL(0x00000051);

    /* delay 100 cycles */
    cpu_delaycycles(100);

    /*
     * ENRX: 0x1 - Enable Receive Channel
     * RATE: 0x10 - PLL output clock rate by a factor of 2x
     * ALIGN: 0x01 - Enable Comma alignment
     * EQ : 0xC - Set to 1100b when RATE = 0x10
     * ENOC: 0x1 - Enable offset compensation
     */
    CSL_BootCfgSetSGMIIRxConfig (macPortNum, 0x00700621);
    /*
     * ENRX: 0x1 - Enable Transmit channel
     * RATE: 0x10 - PLL output clock rate by a factor of 2x
     * INVPAIR: 0x0 - Normal polarity
     */
    CSL_BootCfgSetSGMIITxConfig (macPortNum, 0x000108A1);

    /* Wait sgmii serdes configure complete time set as 1ms */
    wait_time = 1000;
    while(wait_time) {
        CSL_SGMII_getStatus(macPortNum, &sgmiiStatus);
        if (sgmiiStatus.bIsLocked == 1) {
            break;
        } else {
            /* delay 1000 cycles */
            cpu_delaycycles(1000);
            wait_time --;
        }
    }

    if(wait_time == 0) {
    	uart_printf("configure sgmii0 serdes time out!\n");
        return;
    }

    /* Reset the port before configuring it */
    CSL_SGMII_doSoftReset(macPortNum);
    while(CSL_SGMII_getSoftResetStatus(macPortNum) != 0);

    /*
     * Hold the port in soft reset and set up
     * the SGMII control register:
     *      (1) Disable Master Mode
     *      (2) Enable Auto-negotiation
     */
    CSL_SGMII_startRxTxSoftReset(macPortNum);
    CSL_SGMII_disableMasterMode(macPortNum);
    CSL_SGMII_enableAutoNegotiation(macPortNum);
    CSL_SGMII_endRxTxSoftReset(macPortNum);

    /*
     * Setup the Advertised Ability register for this port:
     *      (1) Enable Full duplex mode
     *      (2) Enable Auto Negotiation
     *      (3) Enable the Link
     */
    sgmiiCfg.linkSpeed      =   CSL_SGMII_1000_MBPS;
    sgmiiCfg.duplexMode     =   CSL_SGMII_FULL_DUPLEX;
    sgmiiCfg.bLinkUp        =   1;
    CSL_SGMII_setAdvAbility(macPortNum, &sgmiiCfg);

    do {
        CSL_SGMII_getStatus(macPortNum, &sgmiiStatus);
    } while(sgmiiStatus.bIsLinkUp != 1);

    /* All done with configuration. Return Now. */
    return;
}

static int queue_manager_init(void)
{
    QMSS_CFG_T      qmss_cfg;
    CPPI_CFG_T      cppi_cfg;

    /* Initialize the components required to run this application:
     *  (1) QMSS
     *  (2) CPPI
     *  (3) Packet Accelerator
     */
    /* Initialize QMSS */
    if(CSL_chipReadDNUM() == 0) {
        qmss_cfg.master_core = 1;
    } else {
        qmss_cfg.master_core = 0;
    }
    qmss_cfg.max_num_desc = MAX_NUM_DESC;
    qmss_cfg.desc_size = MAX_DESC_SIZE;
    qmss_cfg.mem_region = Qmss_MemRegion_MEMORY_REGION0;

    if(res_mgr_init_qmss (&qmss_cfg) != 0) {
    	uart_printf("Failed to initialize the QMSS subsystem \n");
        return -1;
    } else {
    	uart_printf("QMSS successfully initialized \n");
    }

    /* Initialize CPPI */
    if(CSL_chipReadDNUM() == 0) {
        cppi_cfg.master_core = 1;
    } else {
        cppi_cfg.master_core = 0;
    }
    cppi_cfg.dma_num = Cppi_CpDma_PASS_CPDMA;
    cppi_cfg.num_tx_queues = NUM_PA_TX_QUEUES;
    cppi_cfg.num_rx_channels = NUM_PA_RX_CHANNELS;
    if(res_mgr_init_cppi (&cppi_cfg) != 0) {
    	uart_printf("Failed to initialize CPPI subsystem \n");
        return -1;
    } else {
    	uart_printf("CPPI successfully initialized \n");
    }

    if(res_mgr_init_pass() != 0) {
    	uart_printf("Failed to initialize the Packet Accelerator \n");
        return -1;
    } else {
    	uart_printf("PA successfully initialized \n");
    }

    return 0;
}
#endif

static void UDP_perform_send()
{
    SOCKET              sudp = INVALID_SOCKET;
    struct sockaddr_in  sin1;
    struct sockaddr_in  sin0;
    struct timeval      timeout;
    Error_Block         errorBlock;

    Error_init(&errorBlock);

    //uart_printf("UDP Transmit Task started\n");

    /* Raise priority to transfer data & wait for the link to come up. */
    //TaskSetPri(TaskSelf(), 1);

    /* Allocate the file environment for this task */
    fdOpenSession(TaskSelf());

    /* Create the UDP socket */
    sudp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sudp == INVALID_SOCKET) {
    	uart_printf("Failed to create socket, %d\n", fdError());
        goto leave;
    }

    // Initialize the bind the local address
    bzero(&sin0, sizeof(struct sockaddr_in));
    sin0.sin_family = AF_INET;
    sin0.sin_addr.s_addr    = inet_addr(LocalIPAddr);
    sin0.sin_port   = htons(TFTP_PORT);

    if(bind(sudp, (PSA) &sin0, sizeof(sin0)) < 0) {
    	uart_printf("Failed to bind the socket \n");
         goto leave;
     }

    // Set the socket IO timeout
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    if( setsockopt( sudp, SOL_SOCKET, SO_SNDTIMEO,
        &timeout, sizeof(timeout)) < 0 )
    {
    	uart_printf("Failed to setsockopt \n");
        goto leave;
    }
    if( setsockopt( sudp, SOL_SOCKET, SO_RCVTIMEO,
           &timeout, sizeof(timeout)) < 0 )
       {
           uart_printf("Failed to setsockopt \n");
           goto leave;
       }

     memset(0xf80000000,0,0x70000000);//

    /* Set Port = 69, leaving IP address = Any */
    bzero(&sin1, sizeof(struct sockaddr_in));
    sin1.sin_family = AF_INET;
    sin1.sin_addr.s_addr    = inet_addr(REMOTE_IPADDR_STRING);
    sin1.sin_port   = htons(TFTP_PORT);
    int i,j,k;
    char *src=(char *)0xf8000000;   //app.bin文件存储地址，128mb空间
	char filename[20] = "app.bin";
	char buf[516] = {0};
	short* p1 = (short*)buf;
	*p1 = htons(1);
	char* p2 = buf+2;
	strcpy(p2, filename);
	char* p4 = p2+strlen(p2)+1;
	strcpy(p4, "octet");

	int size = 4+strlen(p2)+strlen(p4);
	int sen;
	//发送下载请求
	sen=sendto(sudp, buf, sizeof(buf), 0, (struct sockaddr*)&sin1, sizeof(sin1));
	uart_printf("sendto sen=%d\n",sen);
	if(sen < 0)
	{
		uart_printf("发送传输请求失败\n");
		goto leave;
	}


	int addrlen=sizeof(sin1);
	int  res=0;
	int  byte_count=0;
	unsigned short num=1;
	//FILE *fd=fopen(filename,"wb+");
	while(1){
		bzero(buf,sizeof(buf));
		res=recvfrom(sudp,buf,sizeof(buf),0,(struct sockaddr*)&sin1,&addrlen);
		byte_count=byte_count+res-4;
		if(res<0){
			uart_printf("recvfrom failed=%d\n", fdError());
			goto leave;
		}
		if(buf[1]==3){//数据包头
			if(htons(num)==*(unsigned short*)(buf+2)){//数据包编号确认

				//fwrite(buf+4,1,res-4,fd);//将数据内容写入文件
				for(i=0;i<res-4;i++)
				{
					*src++=buf[i+4];
				}

				num++;
				buf[1]=4;
				//*(unsigned short*)(buf+2)=htons(num);
				int sres=sendto(sudp,buf,4,0,(struct sockaddr*)&sin1,sizeof(sin1));//发送应答包
				if(res<516){//退出
					uart_printf("download success\n");
					break;
				}
			}
		}else if(buf[1]==5){
			uart_printf("收到错误包\n");
			//fprintf(stderr, "DOWNLOAD_ERROR:%d : %s\n", ntohs(*(short*)(buf+2)), buf+4);
		 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
		 	*(Uint32 *)0x023100EC = 0x00003000; //将软件复位设置为soft reset模式
		 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
		 	*(Uint32 *)0x023100E8 = 0x00005A69; //启动软件复位

		}

	}

	//uart_printf("src=%X\n",src);
	Cache_wbAll();
   //crc校验
	Uint32 crc_src_add_low=0xf8000000+byte_count-4;
	//printf("crc_src ==%x\n", crc_src_add_low);

	Uint32 crc_src_add_high=0xf8000000+byte_count-2;
	//printf("crc_src ==%x\n", crc_src_add_high);

	//printf("crc_src_high ==%x\n", (((Uint32)(*(short *)(crc_src_add_high)))<<16));
	//printf("crc_src_low ==%x\n", ((Uint32)(*(short *)(crc_src_add_low))));

	Uint32  crc_src=  (((Uint32)(*(short *)(crc_src_add_high)))<<16)|(((Uint32)(*(short *)(crc_src_add_low)))&0x0000ffff);
	uart_printf("crc_src ==%x\n", (crc_src));


	// 初始化CRC32表
	    init_crc32_table();
	    char *src_addr=(char *)0xf8000000;
	Uint32 crc32 = calculate_crc32(src_addr, byte_count-4);
	uart_printf("crc32 ==%x\n", (crc32));



	if(crc32 !=crc_src)
	{
		//fprintf(stderr, "DOWNLOAD_ERROR:%d : %s\n", ntohs(*(short*)(buf+2)), buf+4);
				 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
				 	*(Uint32 *)0x023100EC = 0x00003000; //将软件复位设置为soft reset模式
				 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
				 	*(Uint32 *)0x023100E8 = 0x00005A69; //启动软件复位


	}



	*(Uint32 *)0xfc000000=0x12345678;
  	uart_printf("写入fc000000=%x\n",*(Uint32 *)0xfc000000);


#if 0
	   uart_printf("boot delay...\n");
	    //延时60s
		for(i=0;i<60;i++)
		{
			uart_printf("boot delay  %d\n",60-i);
			for(j=0;j<10000000;j++)
			{
			asm(" nop 1");

			}
		}
#endif


    //软件复位操
 	uart_printf("start reset\n");
 	//if(((*(Uint32 *)0x023100EC)&(0x00003000))==0)    //判断是否已复位，如已复位就跳过此函数

 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
 	*(Uint32 *)0x023100EC = 0x00003000; //将软件复位设置为soft reset模式
 	*(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
 	*(Uint32 *)0x023100E8 = 0x00005A69; //启动软件复位


#if 0
  	//关闭当前程序的服务
     if(sudp != INVALID_SOCKET) {
         fdClose(sudp);
      }

      //TaskSleep(2000);
      fdCloseSession(TaskSelf());
      //TaskSetPri(TaskSelf(), NC_PRIORITY_LOW);
      //TaskDestroy(TaskSelf());
      NC_NetStop(0);

#endif




leave:
            *(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
            *(Uint32 *)0x023100EC = 0x00003000; //将软件复位设置为soft reset模式
            *(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
            *(Uint32 *)0x023100E8 = 0x00005A69; //启动软件复位
            //TaskSleep(20);
            asm (" NOP 20");
            TaskDestroy(TaskSelf());


}

static HANDLE hEchoUdp=0;

/* NetworkOpen */
/* This function is called after the configuration has booted */
static void NetworkOpen()
{

    //(void) TaskCreate(UDP_perform_send, "UDPBenchmarkTX", OS_TASKPRIHIGH, 0x1400, 0, 0, 0 );

    //hEchoUdp = DaemonNew( SOCK_DGRAM, 0, 7, UDP_perform_send, OS_TASKPRINORM, OS_TASKSTKNORM, 0, 1 );

    //UDP_perform_send();


    return;
}

/* NetworkClose */
/*
 * This function is called when the network is shutting down,
 * or when it no longer has any IP addresses assigned to it.
 */
static void NetworkClose()
{


	//ConsoleClose();
	//TaskSetPri(tftpboot, NC_PRIORITY_LOW);
	//TaskDestroy(tftpboot);
    return;
}

/* NetworkIPAddr */
/*
 * This function is called whenever an IP address binding is
 * added or removed from the system.
 */
static void NetworkIPAddr(IPN IPAddr, uint IfIdx, uint fAdd)
{
    static uint fAddGroups = 0;
    IPN IPTmp;

    if(fAdd) {
    	uart_printf("Network Added: ");
    } else {
    	uart_printf("Network Removed: ");
    }

    /* Print a message */
    IPTmp = ntohl(IPAddr);
    uart_printf("If-%d:%d.%d.%d.%d\n", IfIdx,
            (UINT8)(IPTmp>>24)&0xFF, (UINT8)(IPTmp>>16)&0xFF,
            (UINT8)(IPTmp>>8)&0xFF, (UINT8)IPTmp&0xFF );

    /* This is a good time to join any multicast group we require */
    if(fAdd && !fAddGroups) {
        fAddGroups = 1;
    }


    (void) TaskCreate(UDP_perform_send, "UDPBenchmarkTX", OS_TASKPRIHIGH, 0x1400, 0, 0, 0 );


    return;
}



/**
 * @brief BIOS task function
 *
 * @details get IP address and open http server
 *
 * @param arg0 the first arg
 *
 * @param arg1 the second arg
 *
 * @return NULL
 */
int ndk_client(UArg arg0, UArg arg1)
{
    int               rc;
    HANDLE            hCfg;
    CI_SERVICE_TELNET telnet;

    /*
     * THIS MUST BE THE ABSOLUTE FIRST THING DONE IN AN APPLICATION before
     *  using the stack!!
     */
    rc = NC_SystemOpen(NC_PRIORITY_LOW, NC_OPMODE_INTERRUPT);
    if(rc) {
    	uart_printf("NC_SystemOpen Failed (%d)\n", rc);
        goto task_exit;
    }

    /* Create and build the system configuration from scratch. */
    /* Create a new configuration */
    hCfg = CfgNew();
    if(!hCfg) {
    	uart_printf("Unable to create configuration\n");
        goto task_exit;
    }

    /* We better validate the length of the supplied names */
    if(strlen(DomainName) >= CFG_DOMAIN_MAX ||
       strlen(HostName) >= CFG_HOSTNAME_MAX) {
    	uart_printf("Names too long\n");
        goto task_exit;
    }

    /* Add our global hostname to hCfg (to be claimed in all connected domains) */
    CfgAddEntry(hCfg, CFGTAG_SYSINFO, CFGITEM_DHCP_HOSTNAME, 0,
                 strlen(HostName), (UINT8 *)HostName, 0);

    /* set ip address */
        CI_IPNET NA;
        CI_ROUTE RT;
        IPN IPTmp;

        /* Setup manual IP address */
        bzero(&NA, sizeof(NA));
        NA.IPAddr = inet_addr(LocalIPAddr);
        NA.IPMask = inet_addr(LocalIPMask);
        strcpy(NA.Domain, DomainName);
        NA.NetType = 0;

        /* Add the address to interface 2 */
        CfgAddEntry(hCfg, CFGTAG_IPNET, 1, 0,
                           sizeof(CI_IPNET), (UINT8 *)&NA, 0);

        /*
         * Add the default gateway. Since it is the default, the
         * destination address and mask are both zero (we go ahead
         * and show the assignment for clarity).
         */
        bzero(&RT, sizeof(RT));
        RT.IPDestAddr = 0;
        RT.IPDestMask = 0;
        RT.IPGateAddr = inet_addr(GatewayIP);

        /* Add the route */
        CfgAddEntry(hCfg, CFGTAG_ROUTE, 0, 0,
                           sizeof(CI_ROUTE), (UINT8 *)&RT, 0);

        /* Manually add the DNS server when specified */
        IPTmp = inet_addr(DNSServer);
        if(IPTmp)
            CfgAddEntry(hCfg, CFGTAG_SYSINFO, CFGITEM_DHCP_DOMAINNAMESERVER,
                         0, sizeof(IPTmp), (UINT8 *)&IPTmp, 0);




    /*
     * Boot the system using this configuration
     *
     * We keep booting until the function returns 0. This allows
     * us to have a "reboot" command.
     */
    do {
        rc = NC_NetStart(hCfg, NetworkOpen, NetworkClose, NetworkIPAddr);
    } while(rc > 0);

    uart_printf("return ndk client \n");

    /* Delete Configuration*/
    CfgFree(hCfg);
    /* Close the OS */
    NC_SystemClose();





task_exit:
    //NC_SystemClose();
    Task_exit();
    return 0;
}


uint16_t phy_reg_read(uint8_t phy_addr, uint16_t reg_addr)
{
    hMdioRegs->USER_GROUP [0].USER_ACCESS_REG = (CSL_FMK (MDIO_USER_ACCESS_REG_PHYADR, phy_addr) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_REGADR, reg_addr) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_WRITE, 0) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_GO, 1));

    cpu_delaycycles(50000000);

    return CSL_FEXT(hMdioRegs->USER_GROUP [0].USER_ACCESS_REG, MDIO_USER_ACCESS_REG_DATA);
}

void phy_reg_write(uint8_t phy_addr, uint16_t reg_addr, uint16_t data)
{
    hMdioRegs->USER_GROUP [0].USER_ACCESS_REG = (CSL_FMK (MDIO_USER_ACCESS_REG_DATA, data) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_PHYADR, phy_addr) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_REGADR, reg_addr) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_WRITE, 1) |
                                                CSL_FMK (MDIO_USER_ACCESS_REG_GO, 1));
}

void yt8521sh_phy_config(uint8_t phy_addr)
{
    /* Config LED2 to ON when phy link up */
    phy_reg_write(phy_addr, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_LED2_CFG);
    cpu_delaycycles(50000000);
    phy_reg_write(phy_addr, YT8521SH_EXT_REG_DATA_REG, 0x0070);
    cpu_delaycycles(50000000);

    /* Config LED1 to BLINK when phy link up and tx rx active */
    phy_reg_write(phy_addr, YT8521SH_EXT_REG_ADDR_OFFSET_REG, YT8521SH_EXT_LED1_CFG);
    cpu_delaycycles(50000000);
    phy_reg_write(phy_addr, YT8521SH_EXT_REG_DATA_REG, 0x0670);
    cpu_delaycycles(50000000);

    return;
}

void phy_config(uint8_t phy_addr)
{
    uint16_t phy_identity = 0;
    uint16_t manufacturer_id = 0;

    /* enable the MDIO state machine */
    CSL_MDIO_enableStateMachine();

    phy_identity = phy_reg_read(phy_addr, PHY_IDENTIFIER_1_REG);
    manufacturer_id = phy_reg_read(phy_addr, PHY_IDENTIFIER_2_REG);

    /*
     * Select different initialization APIs according to phy id
     * At present, only the YT8521SH phy chip is initialized
     * and the rest of the phy chips are not initialized
     */
    if(phy_identity == PHY_MOTORCOMM_IDENTIFIER && \
        manufacturer_id == PHY_MOTORCOMM_MANUFACTURER) {
        yt8521sh_phy_config(phy_addr);
    }
}

/**
 * @brief main function
 *
 * @details Program unique entry
 *
 * @param void
 *
 * @return successful execution of the program
 *     @retval 0 successful
 *     @retval 1 failed
 */
int main(void)
{
/*
    int *v1=(int *)0x89000000;
    *v1=333;
    int *v2;
    v2=v1;
    printf("v2=%d\n",*v2);
*/


	uint32_t main_pll_freq;
    Task_Handle task;
    Error_Block eb;
    Task_Params TaskParams;
    uint32_t wait_time;
    /* Status of the call to initialize the platform */
    uint32_t sgmiiSERDESStatus;

    /*
     * Set L2 cache size as 64KB
     * equivalent to the l2Mode configuration in the Platform.xdc file
     */
    //CACHE_setL2Size(CACHE_64KCACHE);

    /* Start TCSL so its free running */
    //CSL_chipWriteTSCL(0);

    /* Enable the PSC */
    psc_init();

    /* UART initialization */
    uart_init(CSL_UART_REGS);

    /* Get the cpu freq */
    main_pll_freq = platform_get_main_pll_freq();

    /* Set the default baud rate to 115200 */
    /*
     * CPU frequency = main pll out
     * uart input clock = main pll out / 6
     */
    uart_set_baudrate(CSL_UART_REGS, main_pll_freq/6, 115200);

    CACHE_setL2Size(CACHE_0KCACHE);
    CACHE_setL1DSize(CACHE_L1_0KCACHE);

    uart_printf("value=====%x\n",*(Uint32 *)0xfc000000);
    	if(*(Uint32 *)0xfc000000==0x12345678)
    	{

    		*(Uint32 *)0xfc000000=0;
    		int i,j,k;
    			unsigned int entry[8],a1,a2,*dst,a3,a5,*dst_ver,*src_ver;
    		   unsigned int a4=0x880000;
    		   unsigned int *p=(unsigned int *)0xf8000000;
    		   static void (*APPEntry)(void);

    			uart_printf("image move...\n");


    		  // UInt key=Task_disable();     //禁用任务调度

    		      //搬移段到指定地址
    		      for(j=0;j<8;j++)
    		      {
    		     	 a5=*p++;
    		     	 if((j==1)&&(a5==0))     //判断是否是单核程序
    		     	 {
    		     		 goto core0_start;

    		     	 }

    		 	 entry[j]=*p++;
    		 	 a1=*p++;
    		 	 do{
    		 		a3=a1&3;
    		 		switch(a3)
    		 		{
    		 		case 0:
    		 			break;
    		 		case 1:
    		 		 a1=a1+3;
    		 		 break;
    		 		case 2:
    		 		 a1=a1+2;
    		 		 break;
    		 		case 3:
    		 		 a1=a1+1;
    		 		 break;
    		 		default:
    		 			break;
    		 		}

    		 	 a5=(*p++);

    		 	 if(a5<a4)
    		 		 a5=0x10000000+0x1000000*j+a5;
    		          dst=(unsigned int *)a5;
                      dst_ver=(unsigned int *)a5;
                      src_ver=p;
    		 	 for(i=0;i<(a1>>2);i++)
    		 	 {
    		 		 *dst++=*p++;
    		 	 }


    		 	 for(i=0;i<(a1>>2);i++)
    		 	 {
    		 	   if((*dst_ver++)!=(*src_ver++))
    		 	   {
    		 	      uart_printf("memory error!!!\n");
    		 	      *(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
    		 	      *(Uint32 *)0x023100EC = 0x00003000; //将软件复位设置为soft reset模式
    		 	      *(Uint32 *)0x023100E8 = 0x00015A69; //向RSTCTRL中写入KEY值
    		 	      *(Uint32 *)0x023100E8 = 0x00005A69; //启动软件复位
    		 	   }
    		 	      }

    		 	 }while( (a1=*p++)>0);

    		      }

    		   uart_printf("core1 to core7  IPC...\n");
    		 	    KICK0 = KICK0_UNLOCK;
    		 	  	KICK1 = KICK1_UNLOCK;

    		 		//加载核1~核7魔术地址
    		 		CORE_1_MAGIC_ADDR  = (unsigned int )entry[1];
    		 		CORE_2_MAGIC_ADDR  = (unsigned int )entry[2];
    		 		CORE_3_MAGIC_ADDR  = (unsigned int )entry[3];
    		 		CORE_4_MAGIC_ADDR  = (unsigned int )entry[4];
    		 		CORE_5_MAGIC_ADDR  = (unsigned int )entry[5];
    		 		CORE_6_MAGIC_ADDR  = (unsigned int )entry[6];
    		 		CORE_7_MAGIC_ADDR  = (unsigned int )entry[7];


    		 		//向核1~核7发送IPC启动运行中断
    		 		IPCGR_1_REGS  = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 				         asm (" NOP 1");
    		 		IPCGR_2_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 		                 asm (" NOP 1");
    		 		IPCGR_3_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 						 asm (" NOP 1");
    		 		IPCGR_4_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 						 asm (" NOP 1");
    		 		IPCGR_5_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 						 asm (" NOP 1");
    		 		IPCGR_6_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 						 asm (" NOP 1");
    		 		IPCGR_7_REGS = 0x01;
    		 		for (i=0; i<1000000; i++)
    		 						 asm (" NOP 1");


    		 		// 上锁寄存器
    		 		KICK0 = KICK_LOCK;
    		 		KICK1 = KICK_LOCK;


    		 		for (i=0; i<10000000; i++)
    		 						 asm (" NOP 1");


    		 core0_start:
    		     //跳转到_c_int00处执行程序
    		     uart_printf("core0 boot start...\n");
    		     //将制定地址强制转换为不带参数不带返回值的值函数指针。
    		     APPEntry = (void (*)(void)) entry[0];
    		     (*APPEntry)();



    		  //   Task_restore(key);


    	}


    /* configure network phy chip */
    phy_config(0x0);
    //phy_config(0x1);

    /* Unlock the chip configuration registers */
    CSL_BootCfgUnlockKicker();


    /* initialize the sgmii network subsystem */
    init_sgmii(0);
    //init_sgmii(1);

    if(queue_manager_init() != 0)
        return -1;


    /* Lock the chip configuration registers */
    CSL_BootCfgLockKicker();

    /* Variable initialization */
    Error_init(&eb);

    /* Task params initialization */
    Task_Params_init(&TaskParams);

    TaskParams.stackSize = 0x8000;

    /* create a Task, which is StackTest */
    task = Task_create((Task_FuncPtr)ndk_client, &TaskParams, &eb);
    if(task == NULL) {
    	uart_printf("Task_create() failed!\n");
        BIOS_exit(0);
    }

    /* Start the BIOS 6 Scheduler */
    BIOS_start();
    return 0;







}
