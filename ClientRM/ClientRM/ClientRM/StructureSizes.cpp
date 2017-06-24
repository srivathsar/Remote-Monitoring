
#include "ClientInclude.h"

int ComputeStructSizes()
{
	int CPMData;
	int SPMData;
	int PData;
	int KLRTTree, KLFileTransfer, SGFileData;
	int iCCMDataSize;
	int iSCMDataSize;

	iCCMDataSize = sizeof(CCMDATA);
	CPMData = sizeof(CPMDATA);

	iSCMDataSize = sizeof(SCMDATA);
	SPMData = sizeof(SPMDATA);

	PData = sizeof(PDATA);

	KLRTTree = sizeof(KLRTTREE);
	KLFileTransfer = sizeof(FILE_TRANSFER_DATA);
	SGFileData = sizeof(SGFILEDATA);

	return 0;
}