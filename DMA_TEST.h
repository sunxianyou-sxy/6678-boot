
#ifndef    _DMA_TEST_H
#define   _DMA_TEST_H

extern void DMA_ParaConfig(int Numb ,int SourceAddr,int Acount,int Bcount,int DestinationAddr,int IntEnable);
extern void DMA_Start(int Numb);
extern int DMA_TransState(int Numb);
#endif
